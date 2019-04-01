/*
 * Copyright 2017 sprin0
 * 
 * This file is part of JpnCap.
 * 
 * JpnCap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * JpnCap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with JpnCap.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <leptonica/allheaders.h>
#include <tesseract/capi.h>
#include <ctype.h>

#include "recognize.h"
#include "string_util.h"

#define DEFAULT_DPI_STR "70"


static PIX *pixbuf_to_leptpix(GdkPixbuf *pixbuf) {
	PIX *output;
	int width, height, rowstride;
	guchar *pixels;
	int x, y;
	const char *xdpi, *ydpi;
	union {
		guchar chars[4];
		l_uint32 int_pix;
	} pixel;

	if (gdk_pixbuf_get_has_alpha(pixbuf) ||
	        gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB ||
	        gdk_pixbuf_get_bits_per_sample(pixbuf) != 8 ||
	        gdk_pixbuf_get_n_channels(pixbuf) != 3)
		return NULL;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	output = pixCreateNoInit(width, height, 32);

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			memset(pixel.chars, 0, sizeof(pixel.chars));
			pixel.chars[3] = *(pixels + y * rowstride + x * 3 + 0);
			pixel.chars[2] = *(pixels + y * rowstride + x * 3 + 1);
			pixel.chars[1] = *(pixels + y * rowstride + x * 3 + 2);
			output->data[y * width + x] = pixel.int_pix;
		}
	}
	
	xdpi = gdk_pixbuf_get_option(pixbuf, "x-dpi");
	ydpi = gdk_pixbuf_get_option(pixbuf, "y-dpi");
	if (xdpi == NULL)
		xdpi = DEFAULT_DPI_STR;
	if (ydpi == NULL)
		ydpi = DEFAULT_DPI_STR;
	pixSetResolution(output, atoi(xdpi), atoi(ydpi));

	return output;
}

static char *TesseractRecogize(PIX *img, TessBaseAPI *tess_handle) {
	char *text;

	TessBaseAPISetImage2(tess_handle, img);
	if (TessBaseAPIRecognize(tess_handle, NULL) != 0) {
		fprintf(stderr, "Could not tesseract recognize\n");
		return NULL;
	}

	if ((text = TessBaseAPIGetUTF8Text(tess_handle)) == NULL) {
		fprintf(stderr, "Could not get tesseract text\n");
		return NULL;
	}

	TessBaseAPIClear(tess_handle);
	return text;
}

Vector *substitutions_load(const char *file_name) {
	FILE *subs_file;
	Vector *subs;
	Substitution sub;
	char *line = NULL, *sub_pos = NULL;
	size_t len = 0;
	
	subs = vector_create(sizeof(sub));
	if ((subs_file = fopen(file_name, "r")) == NULL)
		return subs; /* Return empty substitutions */
	
	while (getline(&line, &len, subs_file) != -1) {
		size_t line_len = strlen(line);
		if (line_len > 1 && line[line_len - 1] == '\n')
			line[--line_len] = '\0';
		if (line_len > 1 && line[line_len - 1] == '\r')
			line[--line_len] = '\0';
		
		if ((sub_pos = strchr(line, '=')) == NULL)
			continue;
		if (line == sub_pos)
			continue;

		*(sub_pos++) = '\0';
		sub.from = strdup(line);
		sub.to = strdup(sub_pos);
		
		vector_append(subs, &sub);
	}
	free(line);
	fclose(subs_file);
	
	return subs;
}

char *substitutions_apply(Vector *subs, const char *text) {
	char *output, *input;
	size_t pos = 0;
	const Substitution *sub;
	
	input = output = strdup(text);
	while ((sub = vector_get_const(subs, pos++))) {
		if (string_count_string(input, sub->from) == 0)
			continue;
		output = string_replace_string(input, sub->from, sub->to);
		free(input);
		input = output;
	}
	return output;
}

void substitutions_destroy(Vector *substitutions) {
	size_t pos = 0;
	const Substitution *sub;
	
	while ((sub = vector_get_const(substitutions, pos++)) != NULL) {
		free(sub->from);
		free(sub->to);
	}
	vector_destroy(substitutions);
}

static char *postprocess_text(const char *text, int remove_whitespaces,
	Vector *substitutions) {
	int i, j;
	char *text2, *output;
	
	/* Make a copy of text and make space for a line feed at the end. */
	text2 = malloc(strlen(text) + 2);
	strcpy(text2, text);

	if (remove_whitespaces) {
		i = j = 0;
		do
			while (isspace(text2[i]))
				i++;
		while ((text2[j++] = text2[i++]));
	}
	strcat(text2, "\n");

	output = substitutions_apply(substitutions, text2);
	free(text2);

	return output;
}

char *processPixbuf(GdkPixbuf *pixbuf, text_ori orientation,
	int remove_whitespaces, TessBaseAPI *tess_handle, Vector *substitutions) {
	PIX *img;
	char *text, *processed_text;

	img = pixbuf_to_leptpix(pixbuf);
	if (img == NULL) {
		fprintf(stderr, "Got invalid picture to process\n");
		return NULL;
	}

	switch (orientation) {
	case TEXT_ORIENTATION_VERTICAL:
		TessBaseAPISetPageSegMode(tess_handle, PSM_SINGLE_BLOCK_VERT_TEXT);
		break;
	case TEXT_ORIENTATION_HORIZONTAL:
		TessBaseAPISetPageSegMode(tess_handle, PSM_SINGLE_BLOCK);
		break;
	default:
	case TEXT_ORIENTATION_AUTO:
		TessBaseAPISetPageSegMode(tess_handle, PSM_AUTO);
		/* This may lead to "Empty page!!" warnings and tesseract not
		 * being able to do its work. Maybe remove auto setting?*/
		break;
	}

	text = TesseractRecogize(img, tess_handle);
	pixDestroy(&img);
	processed_text = postprocess_text(text, remove_whitespaces, substitutions);

	TessDeleteText(text);
	return processed_text;
}

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

#define PPROG_NEG_THRES 0.55f
#define PPROG_SCALE_FAC 2.0f
#define PPROG_HALFWIDTH 5
#define PPROG_FRACT 2.5f
#define PPROG_MAX_AR_VERTICAL 1.8f

/* Pre-processing functions adapted from leptonica_util.c from Capture2Text
   http://capture2text.sourceforge.net/
------------------------------------------------------------------------------*/
#define NO_VALUE    (-1)
#define LEPT_TRUE   1
#define LEPT_FALSE  0
#define LEPT_OK     0
#define LEPT_ERROR  1

/* Minimum number of foreground pixels that a line must contain for it to be
   part of a span. Needed because sometimes furigana does not have a perfect
   gap between the text and itself. */
#define FURIGANA_MIN_FG_PIX_PER_LINE  2

/* Minimum width of a span (in pixels) for it to be included in the span
   list. */
#define FURIGANA_MIN_WIDTH  5

/* Maximum number of spans used during furigana removal */
#define FURIGANA_MAX_SPANS  50

typedef struct {
	int start;
	int end;
} Span;

static void preprocess_auto_negate(PIX *pixs, l_float32 dark_bg_threshold) {
	PIX *otsu_pixs = NULL;
	PIX *otsu_pixs2 = NULL;
	BOX *crop = boxCreate(1, 1, pixs->w - 1, pixs->h - 1);
	l_float32 border_avg = 0.0f;

	otsu_pixs2 = pixClipRectangle(pixs, crop, NULL);
	boxDestroy(&crop);
	pixOtsuAdaptiveThreshold(otsu_pixs2, 2000, 2000, 0, 0, 0.0f, NULL,
	                         &otsu_pixs);
	pixDestroy(&otsu_pixs2);

	/* Get the average intensity of the border pixels,
	   with average of 0.0 being completely white and 1.0 being completely
	   black. */
	border_avg  = pixAverageOnLine(otsu_pixs, 0, 0, otsu_pixs->w - 1, 0,
	                               1); /* Top */
	border_avg += pixAverageOnLine(otsu_pixs, 0, otsu_pixs->h - 1,
	                               otsu_pixs->w - 1, otsu_pixs->h - 1,
								   1); /* Bottom */
	border_avg += pixAverageOnLine(otsu_pixs, 0, 0, 0, otsu_pixs->h - 1,
	                               1); /* Left */
	border_avg += pixAverageOnLine(otsu_pixs, otsu_pixs->w - 1, 0,
	                               otsu_pixs->w - 1, otsu_pixs->h - 1,
								   1); /* Right */
	border_avg /= 4.0f;

	pixDestroy(&otsu_pixs);

	/* If background is dark */
	if (border_avg > dark_bg_threshold) {
		/* Negate image */
		pixInvert(pixs, pixs);
	}
}

/* Clear/erase a left-to-right section of the provided binary PIX.
   Returns 0 on success. */
static l_int32 erase_area_left_to_right(PIX *pixs, l_int32 x, l_int32 width) {
	l_int32 status = LEPT_ERROR;
	BOX box;

	box.x = x;
	box.y = 0;
	box.w = width;
	box.h = pixs->h;

	status = pixClearInRect(pixs, &box);

	return status;
}

/* Erase the furigana from the provided binary PIX. Works by finding spans of
   foreground text and removing the spans that are too narrow and are likely
   furigana. Use this version for vertical text.
   Returns LEPT_OK on success. */
static int erase_furigana_vertical(PIX *pixs, float scale_factor) {
	int min_fg_pix_per_line =
		(int)(FURIGANA_MIN_FG_PIX_PER_LINE * scale_factor);
	int min_span_width = (int)(FURIGANA_MIN_WIDTH * scale_factor);
	l_uint32 x = 0;
	l_uint32 y = 0;
	int num_fg_pixels_on_line = 0;
	int good_line = LEPT_FALSE;
	int num_good_lines_in_cur_span = 0;
	int total_good_lines = 0;
	l_uint32 pixel_value = 0;
	Span span = { NO_VALUE, NO_VALUE };
	Span span_list[FURIGANA_MAX_SPANS];
	int total_spans = 0;
	int ave_span_width = 0;
	int span_idx = 0;
	Span *cur_span = NULL;
	l_int32 status = LEPT_ERROR;

	/* Get list of spans that contain fg pixels */
	for (x = 0; x < pixs->w; x++) {
		num_fg_pixels_on_line = 0;
		good_line = LEPT_FALSE;

		for (y = 0; y < pixs->h; y++) {
			status = pixGetPixel(pixs, x, y, &pixel_value);

			if (status != LEPT_OK)
				return status;

			/* If this is a foreground pixel */
			if (pixel_value == 1) {
				num_fg_pixels_on_line++;

				/* If this line has already met the minimum number of fg
				   pixels, stop scanning it */
				if (num_fg_pixels_on_line >= min_fg_pix_per_line) {
					good_line = LEPT_TRUE;
					break;
				}
			}
		}

		/* If last line is good, set it bad in order to close the span */
		if (good_line && (x == pixs->w - 1)) {
			good_line = LEPT_FALSE;
			num_good_lines_in_cur_span++;
		}

		/* If this line has the minimum number of fg pixels */
		if (good_line) {
			/* Start a new span */
			if (span.start == NO_VALUE)
				span.start = x;

			num_good_lines_in_cur_span++;
			
		/* Line doesn't have enough fg pixels to consider as part of a span */
		} else {
			/* If a span has already been started, then end it */
			if (span.start != NO_VALUE) {
				/* If this span isn't too small (needed so that the average
				   isn't skewed) */
				if (num_good_lines_in_cur_span >= min_span_width) {
					span.end = x;

					total_good_lines += num_good_lines_in_cur_span;

					/* Add span to the list */
					span_list[total_spans] = span;
					total_spans++;

					/* Prevent span list overflow */
					if (total_spans >= FURIGANA_MAX_SPANS)
						break;
				}
			}

			/* Reset span */
			span.start = NO_VALUE;
			span.end = NO_VALUE;
			num_good_lines_in_cur_span = 0;
		}
	}

	if (total_spans == 0)
		return LEPT_OK;

	/* Get average width of the spans */
	ave_span_width = total_good_lines / total_spans;

	x = 0;

	/* Erase areas of the PIX where either no span exists or where a span is too
	   narrow */
	for (span_idx = 0; span_idx < total_spans; span_idx++) {
		cur_span = &span_list[span_idx];

		/* If span is at least of average width, erase area between the previous
		   span and this span */
		if ((cur_span->end - cur_span->start + 1) >=
		     (int)(ave_span_width * 0.9)) {
			
			status = erase_area_left_to_right(pixs, x, cur_span->start - x);

			if (status != LEPT_OK)
				return status;

			x = cur_span->end + 1;
		}
	}

	/* Clear area between the end of the right-most span and the right edge of
	   the PIX */
	if ((x != 0) && (x < (pixs->w - 1))) {
		status = erase_area_left_to_right(pixs, x, pixs->w - x);

		if (status != LEPT_OK)
			return status;
	}

	return LEPT_OK;
}


/* Erase the furigana from the provided binary PIX. Works by finding spans of
   foreground text and removing the spans that are too narrow and are likely
   furigana. Use this version for horizontal text.
   Returns LEPT_OK on success. */
static int erase_furigana_horizontal(PIX *pixs, float scale_factor) {
	PIX *pixd, *pixd2;
	
	pixd = pixRotate90(pixs, 1);
	erase_furigana_vertical(pixd, scale_factor);
	pixd2 = pixRotate90(pixd, -1);
	pixDestroy(&pixd);
	pixCopy(pixs, pixd2);
	pixDestroy(&pixd2);

	return LEPT_OK;
}

static void preprocess_leptpix(PIX *pix, text_ori orientation) {
	PIX *pixs;
	PIX *pixs2;
	float aspect_ratio;

	pixs = pixCreate(pixGetWidth(pix), pixGetHeight(pix), 32);
	pixCopy(pixs, pix);

	/* Convert to grayscale */
	pixs2 = pixConvertRGBToGray(pixs, 0.0f, 0.0f, 0.0f);
	pixDestroy(&pixs);
	pixs = pixs2;

	/* Negate image if border pixels are dark */
	preprocess_auto_negate(pixs, PPROG_NEG_THRES);

	/* Upscale image */
	pixs2 = pixScaleGrayLI(pixs, PPROG_SCALE_FAC, PPROG_SCALE_FAC);
	pixDestroy(&pixs);
	pixs = pixs2;

	/* Apply unsharp mask */
	pixs2 = pixUnsharpMaskingGray(pixs, PPROG_HALFWIDTH, PPROG_FRACT);
	/* Copy resolution because pixUnsharpMaskingGray removes it. */
	pixCopyResolution(pixs2, pixs);
	pixDestroy(&pixs);
	pixs = pixs2;

	/* Binarize */
	pixOtsuAdaptiveThreshold(pixs, 2000, 2000, 0, 0, 0.0f, NULL, &pixs2);
	pixDestroy(&pixs);
	pixs = pixs2;

	switch (orientation) {
	case TEXT_ORIENTATION_VERTICAL:
		erase_furigana_vertical(pixs, PPROG_SCALE_FAC);
		break;
	case TEXT_ORIENTATION_HORIZONTAL:
		erase_furigana_horizontal(pixs, PPROG_SCALE_FAC);
		break;
	default:
	case TEXT_ORIENTATION_AUTO:
		aspect_ratio = (float)pixGetWidth(pixs) / pixGetHeight(pixs);
		if (aspect_ratio < PPROG_MAX_AR_VERTICAL)
			erase_furigana_vertical(pixs, PPROG_SCALE_FAC);
		else
			erase_furigana_horizontal(pixs, PPROG_SCALE_FAC);
		break;
	}

	pixCopy(pix, pixs);
	pixDestroy(&pixs);
}

/* End of pre-process functions
------------------------------------------------------------------------------*/


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
	char *text2 = strdup(text), *output;

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
		fprintf(stderr, "Got invalid picture to process.\n");
		return NULL;
	}

	preprocess_leptpix(img, orientation);

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

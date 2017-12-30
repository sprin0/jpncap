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

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <locale.h>
#include <tesseract/capi.h>

#include "recognize.h"
#include "dictionary.h"
#include "vector.h"
#include "japanese_util.h"
#include "main_window.h"

#include "configuration.h"

int main(int argc, char **argv) {
	GtkApplication *app;
	main_window *mw;
	TessBaseAPI *TesseractHandle;
	Vector *substitutions;
	Vector *deinflect_rules;
	Dictionary *dictionary;
	int status = 0;

	/* Tesseract needs this setlocale call on non English platforms */
	setlocale(LC_NUMERIC, "C");
	TesseractHandle = TessBaseAPICreate();
	if (TessBaseAPIInit3(TesseractHandle, NULL, "jpn") != 0) {
		fprintf(stderr, "Could not initiate tesseract. Please check if "
			"tesseract and its Japanese components are installed "
			"properly.\n");
		status = 1;
		goto cleanup_a;
	}
	if ((deinflect_rules =
		jpn_deinflect_load(JPNCAP_RESOURCES_PATH "/deinflect.txt"))
			== NULL) {
		
		fprintf(stderr, "Could not load the deinflect rules from %s\n",
			JPNCAP_RESOURCES_PATH "/deinflect.txt");
		status = 1;
		goto cleanup_b;
	}
	if ((dictionary =
		dictionary_load(JPNCAP_RESOURCES_PATH "/dict.db")) == NULL) {
		
		fprintf(stderr, "Could not load the dictionary from %s\n",
			JPNCAP_RESOURCES_PATH "/dict.db");
		status = 1;
		goto cleanup_c;
	}
	/*
	if ((substitutions = substitutions_load(RESOURCES_PATH "/substitutions.txt")) == NULL) {
		perror("Unable to open subsitutions file");
		status = 1;
		goto cleanup_d;
	}
	* */
	substitutions = substitutions_load(JPNCAP_RESOURCES_PATH "/substitutions.txt");

	app = gtk_application_new("nodomain.jpncap", G_APPLICATION_FLAGS_NONE);
	mw = malloc(sizeof(*mw));
	mw->app = app;
	mw->tess_handle = TesseractHandle;
	mw->substitutions = substitutions;
	mw->deinflect_rules = deinflect_rules;
	mw->dictionary = dictionary;
	g_signal_connect(app, "activate", G_CALLBACK(create_main_window), mw);
	g_signal_connect(app, "startup", G_CALLBACK(startup_main_window), mw);
	g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_main_window), mw);
	status = g_application_run(G_APPLICATION(app), argc, argv);

	free(mw);
	g_object_unref(app);
	
	substitutions_destroy(substitutions);
	/* cleanup_d: */
	dictionary_destroy(dictionary);
	cleanup_c:
	jpn_rules_destroy(deinflect_rules);
	cleanup_b:
	TessBaseAPIEnd(TesseractHandle);
	TessBaseAPIDelete(TesseractHandle);
	cleanup_a:
	
	return status;
}

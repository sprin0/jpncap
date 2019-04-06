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

#pragma once

#include <gtk/gtk.h>

#include "vector.h"
#include "dictionary.h"
#include "recognize.h"

#define MAIN_WINDOW_HISTORY_ENTRIES_MAX 50

typedef struct {
	GtkApplication *app;
	GtkWidget *window;
	GtkWidget *main_box;
	GtkWidget *button_box;
	GtkWidget *button;
	GtkWidget *history_box;
	GtkWidget *back_button;
	GtkWidget *forward_button;
	GtkWidget *menu_button;
	GtkWidget *text_paned;
	GtkWidget *raw_text_view;
	GtkWidget *raw_scrolled_window;
	GtkWidget *dict_text_view;
	GtkWidget *dict_scrolled_window;
	
	char *history_entries[MAIN_WINDOW_HISTORY_ENTRIES_MAX];
	int cur_history_entry;
	GtkClipboard *clipboard;
	gulong clipboard_hanlder_id;
	gboolean setting_auto_clipboard;
	text_ori setting_orientation;
	gboolean setting_remove_whitespaces;
	dictionary_Language setting_language;
	
	TessBaseAPI *tess_handle;
	Vector *substitutions;
	Vector *deinflect_rules;
	Dictionary *dictionary;
} main_window;

void create_main_window(GtkApplication* app, gpointer data);
void startup_main_window(GApplication* app, gpointer pdata);
void shutdown_main_window(GApplication *app, gpointer pdata);
void mw_history_move(main_window* mw, int n);
void updateTextViews(main_window* mw, const char* text);

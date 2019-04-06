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

#define _GNU_SOURCE
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "main_window.h"
#include "capture.h"
#include "recognize.h"
#include "dictionary.h"

const char SHORT_HELP[] = "Type text into the field above or use the Capture "
	"button to detect text from the screen.\nThen move the text cursor in front"
	" of a word to lookup.";

static void history_back(GtkButton* button, gpointer pdata) {
	main_window* mw = (main_window*)pdata;
	mw_history_move(mw, -1);
}

static void history_forward(GtkButton* button, gpointer pdata) {
	main_window* mw = (main_window*)pdata;
	mw_history_move(mw, 1);
}

static void auto_clipboard_callback(GSimpleAction* action, GVariant* parameter,
	gpointer pdata) {
    GVariant *state = g_action_get_state(G_ACTION(action));
    g_action_change_state(G_ACTION(action),
		g_variant_new_boolean(!g_variant_get_boolean (state)));    
    g_variant_unref(state);
}

static void auto_clipboard_set_state(GSimpleAction* action, GVariant* state,
	gpointer pdata) {
	main_window* mw;
	
	mw = (main_window*)pdata;
	g_simple_action_set_state(action, state);
	mw->setting_auto_clipboard = g_variant_get_boolean(state);
}

static int string_contains_japanese(const char *utf8_str) {
	gunichar *ucs4_str, *p_ucs4_str;
	
	ucs4_str = g_utf8_to_ucs4_fast(utf8_str, -1, NULL);
	if (ucs4_str == NULL) /* utf8_str is not a valid UTF-8 string */
		return 0;
	for (p_ucs4_str = ucs4_str; *p_ucs4_str; ++p_ucs4_str) {
		if ((*p_ucs4_str <= 0x309F && *p_ucs4_str >= 0x3040) /*is hiragana*/
			|| (*p_ucs4_str <= 0x30FF && *p_ucs4_str >= 0x30A0) /*is katakana*/
			|| (*p_ucs4_str <= 0x9FBF && *p_ucs4_str >= 0x4E00)) { /*is kanji*/
			
			g_free(ucs4_str);
			return 1;
		}
	}
	g_free(ucs4_str);
	return 0;
}

static void auto_clipboard_owner_change(GtkClipboard* clipboard,
	GdkEventOwnerChange *event, gpointer pdata) {
	main_window* mw = (main_window*)pdata;
	char* text;
	
	/*
	Get the xid of the owner and our current window, assume that the
	resource_mask is 0xFFFF0000 and compare both resource_bases to determine
	whether text from our window has been copied.
	*/
	if (!mw->setting_auto_clipboard || event->owner == NULL
		|| (gdk_x11_window_get_xid(event->owner) & 0xFFFF0000) ==
		   (gdk_x11_window_get_xid(event->window) & 0xFFFF0000))
		return;
	if ((text = gtk_clipboard_wait_for_text(clipboard)) == NULL)
		return;
	
	if (string_contains_japanese(text))
		updateTextViews(mw, text);
	g_free(text);
}

static void orientation_callback(GSimpleAction* action, GVariant* parameter,
	gpointer pdata) {
	g_action_change_state(G_ACTION(action), parameter);
}

static void orientation_set_state(GSimpleAction* action, GVariant* state,
	gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	const char *orientation = g_variant_get_string(state, NULL);
	
	g_simple_action_set_state(action, state);
	
	if (strcmp(orientation, "auto") == 0)
		mw->setting_orientation = TEXT_ORIENTATION_AUTO;
	else if (strcmp(orientation, "vertical") == 0)
		mw->setting_orientation = TEXT_ORIENTATION_VERTICAL;
	else if (strcmp(orientation, "horizontal") == 0)
		mw->setting_orientation = TEXT_ORIENTATION_HORIZONTAL;
	else
		fprintf(stderr, "orientation_set_state: Unknown orientation %s\n",
			orientation);
}

static void remove_whitespaces_callback(GSimpleAction* action,
	GVariant *parameter, gpointer pdata) {
    GVariant *state = g_action_get_state(G_ACTION(action));
    g_action_change_state(G_ACTION(action),
		g_variant_new_boolean(!g_variant_get_boolean (state)));    
    g_variant_unref(state);
}

static void remove_whitespaces_set_state(GSimpleAction* action, GVariant* state,
	gpointer pdata) {
	main_window *mw = (main_window*)pdata;

	g_simple_action_set_state(action, state);
	mw->setting_remove_whitespaces = g_variant_get_boolean(state);
}

static void language_callback(GSimpleAction* action, GVariant* parameter,
	gpointer pdata) {
	g_action_change_state(G_ACTION(action), parameter);
}

static void capture_callback(GdkPixbuf* pixbuf, gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	char *processed_text;

	gtk_widget_set_sensitive(mw->button, TRUE);
	
	if (pixbuf != NULL) {
		processed_text = processPixbuf(pixbuf, mw->setting_orientation,
			mw->setting_remove_whitespaces, mw->tess_handle, mw->substitutions);
		updateTextViews(mw, processed_text);
		free(processed_text);
	}
}

static void capture_button_callback(GtkWidget* widget, gpointer pdata) {
	gtk_widget_set_sensitive(widget, FALSE);
	capture(&capture_callback, pdata);
}

static char *find_good_lookup_position(char *text, int start_pos) {
	int pos = start_pos;
	gunichar c;
	char *line_start;
	
	c = g_utf8_get_char(g_utf8_offset_to_pointer(text, pos));
	while (c == 0 || g_unichar_isspace(c) || g_unichar_ismark(c)
			|| g_unichar_ispunct(c)) {
		if (c == 0 || c == '\n') {
			/* Go to the start of the line */
			line_start = g_utf8_strrchr(text,
				g_utf8_offset_to_pointer(text, pos) - text, '\n');
			if (line_start == NULL) /* pos is in the first line */
				pos = 0;
			else
				pos = g_utf8_pointer_to_offset(text, line_start) + 1;
		} else
			++pos;
		
		/* Stop, if reached the start position after going to line start */
		if (pos == start_pos)
			break;
		c = g_utf8_get_char(g_utf8_offset_to_pointer(text, pos));
	}
	
	return g_utf8_offset_to_pointer(text, pos);
}

static void update_dict_view(GtkTextBuffer* raw_buffer, GParamSpec *pspec,
	gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	static char last_lookup[61];
	static dictionary_Language last_lang;
	GtkTextBuffer *dict_buffer;
	int pos;
	GtkTextIter start, end;
	char *text, *text_lookup, *dict_entry;
	size_t len;
	
	g_object_get(raw_buffer, "cursor-position", &pos, NULL);
	gtk_text_buffer_get_start_iter(raw_buffer, &start);
	gtk_text_buffer_get_end_iter(raw_buffer, &end);
	text = gtk_text_buffer_get_text(raw_buffer, &start, &end, TRUE);
	/* Skip characters that would yield an empty result. If pos is at line end,
	 * go back to line start */
	text_lookup = find_good_lookup_position(text, pos);
	
	len = strlen(text_lookup);
	if (*text_lookup == 0 ||
		(memcmp(text_lookup, last_lookup, 60 > len ? len : 60) == 0
		&& mw->setting_language.id == last_lang.id)) {
		g_free(text);
		return;
	}
		
	strncpy(last_lookup, text_lookup, 60);
	last_lang = mw->setting_language;
	dict_entry = dictionary_lookup(mw->dictionary, text_lookup,
		&(mw->setting_language), mw->deinflect_rules);
	g_free(text);
	if (dict_entry == NULL) {
		fprintf(stderr, "Failed to lookup word.\n");
		return;
	}
	
	dict_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mw->dict_text_view));
	gtk_text_buffer_set_text(dict_buffer, dict_entry, strlen(dict_entry));
	free(dict_entry);
}

static int find_selected_language(const void* a, const void* b, size_t n) {
	const dictionary_Language *lang = (const dictionary_Language*)a;
	const char *lang_str = (const char*)b;
	size_t len = strlen(lang->table_name);
	
	if (memcmp(lang_str, lang->table_name, len) == 0
		&& strcmp(lang_str + len, lang->column_name) == 0)
		return 0;
	return 1;
}

static void language_set_state(GSimpleAction* action, GVariant* state,
	gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	size_t pos;
	const char *language = g_variant_get_string(state, NULL);
	
	if (mw->dictionary == NULL)
		return;

	g_simple_action_set_state(action, state);
	
	pos = vector_find(mw->dictionary->languages, language,
		find_selected_language);
	if (pos == vector_length(mw->dictionary->languages))
		fprintf(stderr, "language_set_state: Unknown language %s\n",
			language);
	else
		mw->setting_language = *(const dictionary_Language*)vector_get_const(
			mw->dictionary->languages, pos);
			
	update_dict_view(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mw->raw_text_view)),
		NULL, mw);
}

static void language_set(main_window *mw, unsigned int n) {
	const dictionary_Language *lang;
	char *state_str;
	
	if (mw->dictionary == NULL)
		return;
	
	if ((lang = vector_get_const(mw->dictionary->languages, n)) == NULL)
		return;
	asprintf(&state_str, "%s%s", lang->table_name, lang->column_name);
	
	g_action_change_state(
		g_action_map_lookup_action(G_ACTION_MAP(mw->app), "language"),
		g_variant_new_string(state_str));
	free(state_str);
}

const guint F_KEYS[] = {GDK_KEY_F2, GDK_KEY_F3, GDK_KEY_F4, GDK_KEY_F5,
						GDK_KEY_F6, GDK_KEY_F7, GDK_KEY_F8, GDK_KEY_F9,
					    GDK_KEY_F10, GDK_KEY_F11, GDK_KEY_F12};
static gboolean on_key_release(GtkWidget *widget, GdkEventKey *event,
	gpointer user_data) {
	main_window *mw = (main_window*)user_data;
	unsigned int i;
	
	switch(event->keyval) {
	case GDK_KEY_F2:
	case GDK_KEY_F3:
	case GDK_KEY_F4:
	case GDK_KEY_F5:
	case GDK_KEY_F6:
	case GDK_KEY_F7:
	case GDK_KEY_F8:
	case GDK_KEY_F9:
	case GDK_KEY_F10:
	case GDK_KEY_F11:
	case GDK_KEY_F12:
		for (i = 0; F_KEYS[i] != event->keyval; i++);
		language_set(mw, i);
		return TRUE;
	default:
		return FALSE;
	}
}

void create_main_window(GtkApplication* app, gpointer pdata) {
	main_window *mw;
	GtkTextBuffer *raw_buffer;
	GtkTextBuffer *dict_buffer;
	
	size_t pos;
	const dictionary_Language *lang;
	char *detail_string;
	
	GtkStyleContext *style_context;
	GMenu *menu_auto_clipboard, *menu_orientation, *menu_remove_whitespaces,
		*menu_language, *menu;
	
	mw = (main_window*)pdata;
	mw->window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(mw->window), "JpnCap");
	gtk_window_set_icon_name(GTK_WINDOW(mw->window), "jpncap");
	gtk_window_set_default_size(GTK_WINDOW(mw->window), 300, 700);
	gtk_container_set_border_width(GTK_CONTAINER(mw->window), 10);
	g_signal_connect(mw->window, "key-release-event",
		G_CALLBACK(on_key_release), mw);
	
	mw->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(mw->window), mw->main_box);
	
	mw->button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start(GTK_BOX(mw->main_box), mw->button_box, FALSE, FALSE,
		FALSE);
	
	mw->button = gtk_button_new_with_label("Capture");
	g_signal_connect(mw->button, "clicked", G_CALLBACK(capture_button_callback),
		mw);
	gtk_widget_set_tooltip_text(mw->button,
		"Select an area on screen to capture text from.");
	gtk_widget_set_size_request(mw->button, 100, 35);
	gtk_box_pack_start(GTK_BOX(mw->button_box), mw->button, FALSE, FALSE,
		FALSE);
	if (mw->tess_handle == NULL)
		gtk_widget_set_sensitive(mw->button, FALSE);
		
	mw->history_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	style_context = gtk_widget_get_style_context(mw->history_box);
	gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_LINKED);
	gtk_box_pack_start(GTK_BOX(mw->button_box), mw->history_box, FALSE, FALSE,
		FALSE);
		
	mw->back_button = gtk_button_new();
	g_signal_connect(mw->back_button, "clicked", G_CALLBACK(history_back), mw);
	gtk_widget_set_sensitive(mw->back_button, FALSE);
	gtk_widget_set_valign(mw->back_button, GTK_ALIGN_CENTER);
	gtk_button_set_image(GTK_BUTTON(mw->back_button),
		gtk_image_new_from_icon_name("go-previous-symbolic",
			GTK_ICON_SIZE_MENU));
	gtk_widget_set_tooltip_text(mw->back_button, "Previous text");
	gtk_box_pack_start(GTK_BOX(mw->history_box), mw->back_button, FALSE, FALSE,
		FALSE);
		
	mw->forward_button = gtk_button_new();
	g_signal_connect(mw->forward_button, "clicked", G_CALLBACK(history_forward),
		mw);
	gtk_widget_set_sensitive(mw->forward_button, FALSE);
	gtk_widget_set_valign(mw->forward_button, GTK_ALIGN_CENTER);
	gtk_button_set_image(GTK_BUTTON(mw->forward_button),
		gtk_image_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_MENU));
	gtk_widget_set_tooltip_text(mw->forward_button, "Next text");
	gtk_box_pack_start(GTK_BOX(mw->history_box), mw->forward_button, FALSE,
		FALSE, FALSE);
	
	mw->menu_button = gtk_menu_button_new();
	gtk_widget_set_halign(mw->menu_button, GTK_ALIGN_END);
	gtk_button_set_image(GTK_BUTTON(mw->menu_button),
		gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_MENU));
	gtk_box_pack_start(GTK_BOX(mw->button_box), mw->menu_button, TRUE, TRUE,
		FALSE);
	
	menu = g_menu_new();
	
	menu_auto_clipboard = g_menu_new();
	g_menu_append(menu_auto_clipboard, "Auto paste clipboard",
		"app.auto-clipboard");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(menu_auto_clipboard));
	g_object_unref(menu_auto_clipboard);
	mw->setting_auto_clipboard = FALSE;
	
	menu_orientation = g_menu_new();
	g_menu_append(menu_orientation, "Auto orientation",
		"app.orientation::auto");
	g_menu_append(menu_orientation, "Vertical text",
		"app.orientation::vertical");
	g_menu_append(menu_orientation, "Horizontal text",
		"app.orientation::horizontal");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(menu_orientation));
	g_object_unref(menu_orientation);
	mw->setting_orientation = TEXT_ORIENTATION_AUTO;
	
	menu_remove_whitespaces = g_menu_new();
	g_menu_append(menu_remove_whitespaces, "Remove whitespaces",
		"app.remove-whitespaces");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(menu_remove_whitespaces));
	g_object_unref(menu_remove_whitespaces);
	mw->setting_remove_whitespaces = TRUE;
	
	menu_language = g_menu_new();
	pos = 0;
	if (mw->dictionary) {
		while ((lang = vector_get_const(mw->dictionary->languages, pos++))) {
			asprintf(&detail_string, "app.language::%s%s", lang->table_name,
				lang->column_name);
			g_menu_append(menu_language, lang->display_name, detail_string);
			free(detail_string);
		}
	}
	g_menu_append_section(menu, NULL, G_MENU_MODEL(menu_language));
	g_object_unref(menu_language);
	
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(mw->menu_button),
		G_MENU_MODEL(menu));

	mw->text_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(mw->main_box), mw->text_paned, TRUE, TRUE,
		FALSE);
	
	raw_buffer = gtk_text_buffer_new(NULL);
	g_signal_connect(raw_buffer, "notify::cursor-position",
		G_CALLBACK(update_dict_view), mw);
	mw->raw_text_view = gtk_text_view_new_with_buffer(raw_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mw->raw_text_view),
		GTK_WRAP_CHAR);
	mw->raw_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mw->raw_scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(mw->raw_scrolled_window),
		mw->raw_text_view);
	gtk_widget_set_size_request(mw->raw_scrolled_window, -1, 50);
	g_object_set(mw->raw_scrolled_window, "margin-bottom", 5, NULL);
	gtk_paned_pack1(GTK_PANED(mw->text_paned), mw->raw_scrolled_window, TRUE,
		TRUE);

	dict_buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_set_text(dict_buffer, SHORT_HELP, strlen(SHORT_HELP));
	mw->dict_text_view = gtk_text_view_new_with_buffer(dict_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mw->dict_text_view),
		GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(mw->dict_text_view), FALSE);
	mw->dict_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(mw->dict_scrolled_window, -1, 300);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(mw->dict_scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(mw->dict_scrolled_window),
		mw->dict_text_view);
	gtk_paned_pack2(GTK_PANED(mw->text_paned), mw->dict_scrolled_window, TRUE,
		TRUE);
	
	gtk_widget_show_all(mw->window);
	memset(mw->history_entries, 0, sizeof(mw->history_entries));
	mw->cur_history_entry = MAIN_WINDOW_HISTORY_ENTRIES_MAX-1;
}

void startup_main_window(GApplication* app, gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	char *state;
	
	GActionEntry entries[] = {
		{"auto-clipboard", auto_clipboard_callback, NULL, "false",
			auto_clipboard_set_state},
		{"orientation", orientation_callback, "s", "'auto'",
			orientation_set_state},
		{"remove-whitespaces", remove_whitespaces_callback, NULL, "true",
			remove_whitespaces_set_state},
		{"language", language_callback, "s", NULL,
			language_set_state}
	};
	if (mw->dictionary) {
		mw->setting_language = *(const dictionary_Language*)vector_get_const(
			mw->dictionary->languages, 0);
		asprintf(&state, "'%s%s'", mw->setting_language.table_name,
			mw->setting_language.column_name);
		entries[3].state = state;
	}
	
	g_action_map_add_action_entries(G_ACTION_MAP(app), entries,
		G_N_ELEMENTS(entries), mw);
	mw->clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	mw->clipboard_hanlder_id = g_signal_connect(mw->clipboard, "owner-change",
		G_CALLBACK(auto_clipboard_owner_change), mw);
	if (mw->dictionary)
		free(state);
}

void shutdown_main_window(GApplication *app, gpointer pdata) {
	main_window *mw = (main_window*)pdata;
	int i;
	
	for (i = 0; i < MAIN_WINDOW_HISTORY_ENTRIES_MAX; i++) {
		if (mw->history_entries[i] != NULL)
			free(mw->history_entries[i]);
	}
	/* Disconnect the owner-change event */
	g_signal_handler_disconnect(mw->clipboard, mw->clipboard_hanlder_id);
}

static void mw_update(main_window* mw) {
	char *text = mw->history_entries[mw->cur_history_entry];
	GtkTextBuffer *buffer;
	
	if (text != NULL) {
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mw->raw_text_view));
		gtk_text_buffer_set_text(buffer, text, strlen(text));
		g_signal_emit_by_name(mw->raw_text_view, "move-cursor",
			GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
	}
		
	if (mw->cur_history_entry > 0 &&
		mw->history_entries[mw->cur_history_entry-1] != NULL)
		gtk_widget_set_sensitive(mw->back_button, TRUE);
	else
		gtk_widget_set_sensitive(mw->back_button, FALSE);
	if (mw->cur_history_entry < MAIN_WINDOW_HISTORY_ENTRIES_MAX-1)
		gtk_widget_set_sensitive(mw->forward_button, TRUE);
	else
		gtk_widget_set_sensitive(mw->forward_button, FALSE);
}

static size_t strlen_ignore_whitespace(const char* str) {
	size_t i, j;
	for (i = 0, j = 0; str[i]; i++) {
		if (!isspace(str[i]))
			j++;
	}
	return j;
}

static char* mw_get_cur_raw_text(main_window* mw) {
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(
		mw->raw_text_view));
	GtkTextIter start, end;
	
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

void mw_history_move(main_window* mw, int n) {
	char *cur_raw_text = mw_get_cur_raw_text(mw);
	
	if (mw->history_entries[mw->cur_history_entry] != NULL
		&& strlen_ignore_whitespace(cur_raw_text) != 0) {
		if (strcmp(mw->history_entries[mw->cur_history_entry], cur_raw_text)
			!= 0) {
			if (strlen(cur_raw_text)
				> strlen(mw->history_entries[mw->cur_history_entry])) {
				free(mw->history_entries[mw->cur_history_entry]);
				mw->history_entries[mw->cur_history_entry] =
					malloc(strlen(cur_raw_text) + 1);
			}
			strcpy(mw->history_entries[mw->cur_history_entry], cur_raw_text);
		}
	}
	g_free(cur_raw_text);
	
	if (mw->cur_history_entry + n < 0)
		mw->cur_history_entry = 0;
	else if (mw->cur_history_entry + n > MAIN_WINDOW_HISTORY_ENTRIES_MAX-1)
		mw->cur_history_entry = MAIN_WINDOW_HISTORY_ENTRIES_MAX-1;
	else
		mw->cur_history_entry += n;
	mw_update(mw);
}

static void mw_history_add(main_window* mw, const char* text) {
	int i;
	
	if (mw->history_entries[MAIN_WINDOW_HISTORY_ENTRIES_MAX-1] != NULL &&
		(strcmp(text, mw->history_entries[MAIN_WINDOW_HISTORY_ENTRIES_MAX-1])
		== 0 || strlen(text) == 0))
		return;
	
	if (mw->history_entries[0] != NULL)
		free(mw->history_entries[0]);
	for (i = 1; i < MAIN_WINDOW_HISTORY_ENTRIES_MAX; i++) {
		mw->history_entries[i-1] = mw->history_entries[i];
	}
	mw->cur_history_entry--;
	mw->history_entries[MAIN_WINDOW_HISTORY_ENTRIES_MAX-1] =
		malloc(strlen(text) + 1);
	strcpy(mw->history_entries[MAIN_WINDOW_HISTORY_ENTRIES_MAX-1], text);
}

void updateTextViews(main_window* mw, const char* text) {
	mw_history_add(mw, text);
	mw_history_move(mw, MAIN_WINDOW_HISTORY_ENTRIES_MAX);
}

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

#include "capture.h"

/*  Code for capture taken from gnome-screenshot
	https://git.gnome.org/browse/gnome-screenshot */
typedef struct {
	gboolean aborted;
	gboolean button_pressed;
	GdkRectangle rect;
	GdkCursor *cursor;
	GdkPixbuf *pixbuf;
	GtkWidget *window;
	void (*callback)(GdkPixbuf *, gpointer);
	gpointer cb_data;
} capture_data;

static gboolean capture_crop_pixbuf(gpointer pdata) {
	capture_data *data;
	GdkPixbuf *cropped_pixbuf, * captured_pixbuf;

	data = (capture_data *) pdata;
	captured_pixbuf = data->pixbuf;
	cropped_pixbuf = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(captured_pixbuf),
	                                gdk_pixbuf_get_has_alpha(captured_pixbuf),
	                                gdk_pixbuf_get_bits_per_sample(
	                                    captured_pixbuf),
	                                data->rect.width, data->rect.height);
	/*  Takes a part of the screenshot that was made right after the mouse
		button was pressed. This isn't optimal but otherwise it is possible for
		the rubberband to appear on the capture. */
	gdk_pixbuf_copy_area(captured_pixbuf, data->rect.x,
	                     data->rect.y, data->rect.width, data->rect.height,
	                     cropped_pixbuf, 0, 0);

	g_object_unref(captured_pixbuf);
	data->callback(cropped_pixbuf, data->cb_data);

	g_object_unref(cropped_pixbuf);
	free(data);

	return FALSE;
}

static void capture_done(gpointer pdata) {
	capture_data *data;
	GdkSeat *seat;

	data = (capture_data *) pdata;
	GdkDisplay *display = gdk_display_get_default();
	seat = gdk_display_get_default_seat(display);
	gdk_seat_ungrab(seat);
	gtk_widget_destroy(data->window);
	g_object_unref(data->cursor);
	gdk_display_flush(display);

	if (!data->aborted)
		g_timeout_add(100, capture_crop_pixbuf, data);
	else {
		if (data->pixbuf != NULL)
			g_object_unref(data->pixbuf);
		free(data);
		data->callback(NULL, data->cb_data);
	}
}

static gboolean capture_key_press(GtkWidget *capture_window, GdkEventKey *event,
                                  capture_data *data) {
	if (event->keyval == GDK_KEY_Escape) {
		data->aborted = TRUE;
		capture_done(data);
	}

	return TRUE;
}

static gboolean capture_button_press(GtkWidget *capture_window,
                                     GdkEventButton *event,
                                     capture_data *data) {
	GdkWindow *root;

	if (data->button_pressed)
		return TRUE;

	data->button_pressed = TRUE;
	data->rect.x = event->x_root;
	data->rect.y = event->y_root;

	root = gdk_get_default_root_window();
	data->pixbuf = gdk_pixbuf_get_from_window(root, 0, 0,
	               gdk_window_get_width(root), gdk_window_get_height(root));

	return TRUE;
}

static gboolean capture_motion_notify(GtkWidget *capture_window,
                                      GdkEventButton *event,
                                      capture_data *data) {
	GdkRectangle draw_rect;

	if (!data->button_pressed)
		return TRUE;

	draw_rect.width = ABS(data->rect.x - event->x_root);
	draw_rect.height = ABS(data->rect.y - event->y_root);
	draw_rect.x = MIN(data->rect.x, event->x_root);
	draw_rect.y = MIN(data->rect.y, event->y_root);
	
	if (draw_rect.width <= 0 || draw_rect.height <= 0) {
		gtk_window_move(GTK_WINDOW(capture_window), -100, -100);
		gtk_window_resize(GTK_WINDOW(capture_window), 10, 10);
		return TRUE;
	}

	gtk_window_move(GTK_WINDOW(capture_window), draw_rect.x, draw_rect.y);
	gtk_window_resize(GTK_WINDOW(capture_window), draw_rect.width,
	                  draw_rect.height);

	/*  We (ab)use app-paintable to indicate if we have an RGBA window */
	if (!gtk_widget_get_app_paintable(capture_window)) {
		GdkWindow *gdkwindow = gtk_widget_get_window(capture_window);

		/*  Shape the window to make only the outline visible */
		if (draw_rect.width > 2 && draw_rect.height > 2) {
			cairo_region_t *region;
			cairo_rectangle_int_t region_rect = {
				0, 0,
				draw_rect.width, draw_rect.height
			};

			region = cairo_region_create_rectangle(&region_rect);
			region_rect.x++;
			region_rect.y++;
			region_rect.width -= 2;
			region_rect.height -= 2;
			cairo_region_subtract_rectangle(region, &region_rect);
			gdk_window_shape_combine_region(gdkwindow, region, 0, 0);

			cairo_region_destroy(region);
		} else
			gdk_window_shape_combine_region(gdkwindow, NULL, 0, 0);
	}

	return TRUE;
}

static gboolean capture_button_release(GtkWidget *capture_window,
                                       GdkEventButton *event,
                                       capture_data *data) {
	if (!data->button_pressed)
		return TRUE;

	data->rect.width = ABS(data->rect.x - event->x_root);
	data->rect.height = ABS(data->rect.y - event->y_root);
	data->rect.x = MIN(data->rect.x, event->x_root);
	data->rect.y = MIN(data->rect.y, event->y_root);

	if (data->rect.width <= 5 || data->rect.height <= 5 || data->pixbuf == NULL)
		data->aborted = TRUE;

	capture_done(data);

	return TRUE;
}

static gboolean capture_window_draw(GtkWidget *capture_window, cairo_t *cr,
                                    gpointer unused) {
	GtkStyleContext *style;

	style = gtk_widget_get_style_context(capture_window);

	if (gtk_widget_get_app_paintable(capture_window)) {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_rgba(cr, 0, 0, 0, 0);
		cairo_paint(cr);

		gtk_style_context_save(style);
		gtk_style_context_add_class(style, GTK_STYLE_CLASS_RUBBERBAND);

		gtk_render_background(style, cr, 0, 0,
		                      gtk_widget_get_allocated_width(capture_window),
		                      gtk_widget_get_allocated_height(capture_window));
		gtk_render_frame (style, cr, 0, 0,
		                  gtk_widget_get_allocated_width(capture_window),
		                  gtk_widget_get_allocated_height(capture_window));

		gtk_style_context_restore(style);
	}

	return TRUE;
}

static gboolean capture_grab(GtkWidget *capture_window, capture_data *data) {
	GdkDisplay *display;
	GdkSeat *seat;
	GdkGrabStatus res;

	display = gdk_display_get_default();
	if (display == NULL) {
		fprintf(stderr, "Could not get default display\n");
		return FALSE;
	}
	data->cursor = gdk_cursor_new_for_display(display, GDK_CROSSHAIR);
	seat = gdk_display_get_default_seat(display);

	res = gdk_seat_grab(seat,
	                    gtk_widget_get_window(capture_window),
	                    GDK_SEAT_CAPABILITY_ALL, FALSE,
	                    data->cursor, NULL, NULL, NULL);
	if (res != GDK_GRAB_SUCCESS) {
		fprintf(stderr, "Could not grab input\n");
		g_object_unref(data->cursor);
		return FALSE;
	}

	return TRUE;
}

void capture(void (*callback)(GdkPixbuf *, gpointer), gpointer cb_data) {
	capture_data *data;
	GtkWidget *capture_window;
	GdkScreen *screen;
	GdkVisual *visual;

	data = malloc(sizeof(*data));
	data->aborted = FALSE;
	data->button_pressed = FALSE;
	data->rect.x = 0;
	data->rect.y = 0;
	data->rect.width  = 0;
	data->rect.height = 0;
	data->cursor = NULL;
	data->pixbuf = NULL;
	data->window = NULL;
	data->callback = callback;
	data->cb_data = cb_data;

	capture_window = gtk_window_new(GTK_WINDOW_POPUP);
	data->window = capture_window;
	screen = gdk_screen_get_default();
	if (screen) {
		visual = gdk_screen_get_rgba_visual(screen);
		if (gdk_screen_is_composited(screen) && visual) {
			gtk_widget_set_visual(capture_window, visual);
			gtk_widget_set_app_paintable(capture_window, TRUE);
		}
	}
	g_signal_connect(capture_window, "draw", G_CALLBACK(capture_window_draw),
	                 NULL);
	gtk_window_move(GTK_WINDOW(capture_window), -100, -100);
	gtk_window_resize(GTK_WINDOW(capture_window), 10, 10);
	gtk_widget_show(capture_window);

	g_signal_connect(capture_window, "key-press-event",
	                 G_CALLBACK(capture_key_press), data);
	g_signal_connect(capture_window, "button-press-event",
	                 G_CALLBACK(capture_button_press), data);
	g_signal_connect(capture_window, "button-release-event",
	                 G_CALLBACK(capture_button_release), data);
	g_signal_connect(capture_window, "motion-notify-event",
	                 G_CALLBACK(capture_motion_notify), data);

	if (!capture_grab(capture_window, data)) {
		gtk_widget_destroy(capture_window);
		free(data);
		callback(NULL, cb_data);
	}
}


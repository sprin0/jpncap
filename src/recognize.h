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

#include <gdk/gdk.h>
#include <tesseract/capi.h>
#include "vector.h"

typedef enum {
	TEXT_ORIENTATION_AUTO,
	TEXT_ORIENTATION_VERTICAL,
	TEXT_ORIENTATION_HORIZONTAL
} text_ori;

typedef struct {
	char *from;
	char *to;
} Substitution;

Vector *substitutions_load(const char *file_name);
char *substitutions_apply(Vector *subs, const char *text);
void substitutions_destroy(Vector *substitutions);
char *processPixbuf(GdkPixbuf *pixbuf, text_ori orientation,
                    int remove_whitespaces, TessBaseAPI *tess_handle,
					Vector *substitutions);

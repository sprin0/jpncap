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

#include "vector.h"

typedef struct {
	char* from;
	size_t from_len;
	char* to;
	size_t to_len;
	int type;
	char* reason;
} jpn_Rule;

typedef struct {
	char *word;
	int type;
	char *reason;
} jpn_Variant;

Vector *jpn_deinflect_load(const char *file_path);
void jpn_rules_destroy(Vector *rules);
Vector *jpn_get_all_variants(const char *text, Vector *rules);
void jpn_variants_destroy(Vector *variants);
char *jpn_half2fullwidth(const char *str);
char *jpn_katakana2hiragana(const char *str);
int jpn_is_correctly_deinflected(int type, const char* pos);

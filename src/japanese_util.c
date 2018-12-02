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
#include "japanese_util.h"
#include "string_util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#define DEINFLECT_MAX_LINES 2000
#define DEINFLECT_MAX_LINE_LENGTH 200

typedef jpn_Rule Rule;
typedef jpn_Variant Variant;

static int rule_parse(const char *line, Vector *reasons, Rule *rule) {
	int reason_number;
	char *line2, *line3, *token;
	const char* reason;
	
	line3 = line2 = strdup(line);
	
	token = line2;
	line2 = string_split_char(line2, '\t');
	rule->from_len = strlen(token);
	rule->from = strdup(token);
	
	token = line2;
	line2 = string_split_char(line2, '\t');
	rule->to_len = strlen(token);
	rule->to = strdup(token);
	
	token = line2;
	line2 = string_split_char(line2, '\t');
	if (strlen(token) > 8) {
		free(rule->to);
		free(rule->from);
		free(line2);
		return 0;
	}
	rule->type = atoi(token);
	
	token = line2;
	line2 = string_split_char(line2, '\t');
	reason_number = atoi(token);
	if (strlen(token) > 8 || vector_length(reasons) <= reason_number) {
		free(rule->to);
		free(rule->from);
		free(line2);
		return 0;
	}
	reason = *(const char**)vector_get_const(reasons, reason_number);
	rule->reason = strdup(reason);
	
	free(line3);
	
	return 1;
}

Vector *jpn_deinflect_load(const char *file_path) {
	FILE *file;
	Vector *rules, *reasons;
	Rule rule;
	char *reason;
	char *const*r_p;
	char *line;
	int line_number;
	size_t len, pos;
	
	if ((file = fopen(file_path, "r")) == NULL) {
		perror("Unable to open deinflection file");
		return NULL;
	}
	
	rules = vector_create(sizeof(rule));
	reasons = vector_create(sizeof(reason));
	line = NULL;
	line_number = 0;
	while (getline(&line, &len, file) != -1) {
		line_number++;
		if (line_number > DEINFLECT_MAX_LINES) {
			fprintf(stderr, "Too many lines (>%i) in '%s'", DEINFLECT_MAX_LINES,
				file_path);
			break;
		}
		if (line[0] == '#')
			continue;
		if (strlen(line) > DEINFLECT_MAX_LINE_LENGTH) {
			fprintf(stderr, "Line too long on line %i in '%s'", line_number,
				file_path);
			continue;
		}
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		
		/* a line containing a rule has 3 tabs.
			If it doesn't, it's a rule reason */
		if (string_count_char(line, '\t') >= 3) {
			if (!rule_parse(line, reasons, &rule)) {
				fprintf(stderr, "Invalid rule on line %i in '%s'", line_number,
					file_path);
				continue;
			}
			vector_append(rules, &rule);
		} else {
			reason = strdup(line);
			vector_append(reasons, &reason);
		}
	}
	
	free(line);
	pos = 0;
	while ((r_p = vector_get_const(reasons, pos++)))
		free(*r_p);
	vector_destroy(reasons);
	fclose(file);
	
	return rules;
}

void jpn_rules_destroy(Vector *rules) {
	const Rule *rule;
	size_t pos = 0;
	
	while ((rule = vector_get_const(rules, pos++))) {
		free(rule->from);
		free(rule->to);
		free(rule->reason);
	}
	vector_destroy(rules);
}

static int rule_applies(const char *word, int type, const Rule *rule) {
	const char *suffix;
	size_t len = strlen(word);
	
	suffix = word + len - rule->from_len;
	return (len >= rule->from_len)
		&& (type & rule->type)
		&& (strcmp(suffix, rule->from) == 0);
}

static int find_variant(const void* a, const void* b, size_t size) {
	return strcmp(((const Variant*)a)->word, ((const Variant*)b)->word);
}

static char *concatenate_reasons(const char* old_reason, const char* reason) {
	char *new_reason;
	
	if (old_reason == NULL) {
		if (strlen(reason) == 0)
			new_reason = NULL;
		else
			new_reason = strdup(reason);
	} else {
		new_reason = malloc(strlen(reason) + strlen(old_reason) + 4);
		strcpy(new_reason, reason);
		strcat(new_reason, " < ");
		strcat(new_reason, old_reason);
	}
	
	return new_reason;
}

Vector *jpn_get_all_variants(const char *text, Vector *rules) {
	glong pos;
	gchar *text2;
	Variant variant, *old_variant;
	Vector *variants = vector_create(sizeof(variant));
	size_t old_variant_pos, pos_v, pos_r;
	const Variant *variant_p;
	const Rule *rule_p;
	int i;
	
	/* Copy the text and cut it if it's too long */
	text2 = g_utf8_substring((const gchar*)text, 0,
		g_utf8_strlen(text, 60) > 13 ? 13 : g_utf8_strlen(text, 60));
	
	for (pos = g_utf8_strlen(text2, 100); pos; pos--) {
		*g_utf8_offset_to_pointer(text2, pos) = 0;
		
		variant.word = strdup((char*)text2);
		variant.type = 0xFF;
		variant.reason = NULL;
		vector_append(variants, &variant);
		
		/* Try every rule to every variant found until now */
		pos_v = 0;
		while ((variant_p = vector_get_const(variants, pos_v++))) {
			i = 0;
			pos_r = 0;
			while ((rule_p = vector_get_const(rules, pos_r++))) {
				i++;
				if (!rule_applies(variant_p->word, variant_p->type, rule_p))
					continue;
				
				variant.word = string_suffix_replace(variant_p->word,
					rule_p->from_len, rule_p->to);
				
				old_variant_pos = vector_find(variants, &variant,
					&find_variant);
				old_variant = vector_get(variants, old_variant_pos);
				if (old_variant != NULL) {
					old_variant->type |= (rule_p->type >> 8);
					vector_set(variants, old_variant_pos, old_variant);
					free(old_variant);
					free(variant.word);
				} else {
					variant.reason = concatenate_reasons(variant_p->reason,
						rule_p->reason);
					variant.type = rule_p->type >> 8;
					
					/* Append new variant and update variant_p. */
					vector_append(variants, &variant);
					variant_p = vector_get_const(variants, pos_v - 1);
				}
			}
			
		}
	}
	
	g_free(text2);
	return variants;
}

void jpn_variants_destroy(Vector *variants) {
	const Variant *variant;
	size_t pos = 0;
	
	while ((variant = vector_get_const(variants, pos++))) {
		free(variant->reason);
		free(variant->word);
	}
	vector_destroy(variants);
}

char *jpn_half2fullwidth(const char *str) {
	const gunichar full[] = {
		0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 
		0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 
		0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 
		0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 
		0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, 0x30E0, 
		0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 
		0x30ED, 0x30EF, 0x30F3 };
	const gunichar full_dakuten[] = {
		0x30FB, 0x30FA, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 
		0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30F4, 0x30A8, 0x30AA, 0x30AC, 
		0x30AE, 0x30B0, 0x30B2, 0x30B4, 0x30B6, 0x30B8, 0x30BA, 0x30BC, 0x30BE, 
		0x30C0, 0x30C2, 0x30C5, 0x30C7, 0x30C9, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 
		0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, 0x30E0, 
		0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 
		0x30ED, 0x30F7, 0x30F3 };
	const gunichar full_hanadakuten[] = {
		0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 
		0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 
		0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 
		0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 
		0x30CE, 0x30D1, 0x30D4, 0x30D7, 0x30DA, 0x30DD, 0x30DE, 0x30DF, 0x30E0, 
		0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 
		0x30ED, 0x30EF, 0x30F3 };
	gunichar *ucs4_str, *p_ucs4_str, *p_ucs4_str2;
	char *result;
	
	ucs4_str = g_utf8_to_ucs4(str, -1, NULL, NULL, NULL);
	for(p_ucs4_str = ucs4_str; *p_ucs4_str; ++p_ucs4_str) {
		if (*p_ucs4_str <= 0x7E && *p_ucs4_str >= 0x21) /* ASCII to full-width */
			*p_ucs4_str += 0xFEE0;
		else if (*p_ucs4_str <= 0xFF9D && *p_ucs4_str >= 0xFF65) {
			if (*(p_ucs4_str + 1) == 0xFF9E) { /* next char is dakuten */
				*p_ucs4_str = full_dakuten[*p_ucs4_str - 0xFF65];
				++p_ucs4_str;
			} else if (*(p_ucs4_str + 1) == 0xFF9F) { /* next char is hanadakuten */
				*p_ucs4_str = full_hanadakuten[*p_ucs4_str - 0xFF65];
				++p_ucs4_str;
			} else {
				*p_ucs4_str = full[*p_ucs4_str - 0xFF65];
			}
		}
	}
	for(p_ucs4_str = p_ucs4_str2 = ucs4_str; *p_ucs4_str2;
		++p_ucs4_str, ++p_ucs4_str2) {
		if (*p_ucs4_str2 == 0xFF9E || *p_ucs4_str2 == 0xFF9F)
			++p_ucs4_str2;
		*p_ucs4_str = *p_ucs4_str2;
	}
	
	result = g_ucs4_to_utf8(ucs4_str, -1, NULL, NULL, NULL);
	g_free(ucs4_str);
	
	return result;
}

char *jpn_katakana2hiragana(const char *str) {
	gunichar *ucs4_str, *p_ucs4_str;
	char* result;
	
	ucs4_str = g_utf8_to_ucs4(str, -1, NULL, NULL, NULL);
	for (p_ucs4_str = ucs4_str; *p_ucs4_str; ++p_ucs4_str) {
		if (*p_ucs4_str <= 0x30F6 && *p_ucs4_str >= 0x30A1) {
			*p_ucs4_str -= 0x60;
		}
	}
	
	result = g_ucs4_to_utf8(ucs4_str, -1, NULL, NULL, NULL);
	g_free(ucs4_str);
	
	return result;
}

int jpn_is_correctly_deinflected(int type, const char* pos) {
	char *pos_, *p_pos, *p_pos2;
	int r;
	
	p_pos2 = p_pos = pos_ = strdup(pos);
	do {
		p_pos2 = string_split_string(p_pos2, "; ");
		r = (type == 0xFF)
			|| ((type & 1) && (strcmp(p_pos, "v1") == 0))
			|| ((type & 4) && (strcmp(p_pos, "adj-i") == 0))
			|| ((type & 64) && (strcmp(p_pos, "v5k-s") == 0
				|| strcmp(p_pos, "v5u-s") == 0 ))
			|| ((type & 2) && (strcmp(p_pos, "v5k-s") != 0
				&& strcmp(p_pos, "v5u-s") != 0 && strncmp(p_pos, "v5", 2) == 0))
			|| ((type & 8) && (strcmp(p_pos, "vk") == 0))
			|| ((type & 16) && (strncmp(p_pos, "vs-", 3) == 0));
		
		if (r) {
			free(pos_);
			return 1;
		}
		p_pos = p_pos2;
	} while (p_pos);
	
	free(pos_);
	return 0;
}

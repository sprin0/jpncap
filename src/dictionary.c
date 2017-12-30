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
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "dictionary.h"
#include "vector.h"
#include "japanese_util.h"
#include "string_util.h"

typedef dictionary_Language Language;

static int table_exists(sqlite3 *database, const char *table_name) {
	const char * const QRY = "SELECT COUNT(type) FROM sqlite_master WHERE "
		"type='table' and name=?;";
	sqlite3_stmt *stmt;
	int exists;

	if (sqlite3_prepare_v2(database, QRY, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "table_exists prepare failed: %s\n",
			sqlite3_errmsg(database));
		return 0;
	}
	if (sqlite3_bind_text(stmt, 1, table_name, -1,
		SQLITE_STATIC) != SQLITE_OK) {
		fprintf(stderr, "table_exists bind failed: %s\n",
			sqlite3_errmsg(database));
		sqlite3_finalize(stmt);
		return 0;
	}
	
	if (sqlite3_step(stmt) == SQLITE_ERROR) {
		fprintf(stderr, "table_exists step failed: %s\n",
			sqlite3_errmsg(database));
		sqlite3_finalize(stmt);
		return 0;
	}
	exists = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return exists;
}

static int column_exists(sqlite3 *database, const char *table_name,
	const char *column_name) {
	char *qry;
	const char * const QRY_FORMAT = "PRAGMA table_info(%s);";
	sqlite3_stmt *stmt;
	
	qry = malloc(strlen(QRY_FORMAT) + strlen(table_name));
	sprintf(qry, QRY_FORMAT, table_name);
	if (sqlite3_prepare_v2(database, qry, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "column_exists prepare failed: %s\n",
			sqlite3_errmsg(database));
		free(qry);
		return 0;
	}
	free(qry);
	
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if (strcmp(column_name, (const char*)sqlite3_column_text(stmt, 1))
			== 0) {
			sqlite3_finalize(stmt);
			return 1;
		}
	}
	sqlite3_finalize(stmt);
	return 0;
}

static int language_table_valid(sqlite3 *database, const Language *lang) {
	char *toc_name;
	int valid = 0;
	
	toc_name = malloc(strlen(lang->table_name) + 5);
	*toc_name = 0;
	strcat(toc_name, lang->table_name);
	strcat(toc_name, "_toc");
	
	if (table_exists(database, lang->table_name)
		&& table_exists(database, toc_name)
		&& column_exists(database, lang->table_name, "id")
		&& column_exists(database, lang->table_name, "japanese")
		&& column_exists(database, lang->table_name, "pos")
		&& column_exists(database, lang->table_name, lang->column_name))
		valid = 1;
		
	free(toc_name);
	return valid;
}

static void lang_vector_destroy(Vector *languages) {
	const Language *lang;
	size_t pos = 0;
	
	while ((lang = vector_get_const(languages, pos++))) {
		free(lang->display_name);
		free(lang->table_name);
		free(lang->column_name);
	}
	vector_destroy(languages);
}

void dictionary_destroy(Dictionary *dictionary) {
	sqlite3_close(dictionary->database);
	lang_vector_destroy(dictionary->languages);
	free(dictionary);
}

static Vector *lang_vector_load(sqlite3 *database) {
	const char * const QRY = "SELECT id, display_name, table_name, column_name, "
		"deinflect FROM Languages;";
	sqlite3_stmt *stmt;
	Vector *languages;
	Language lang;
	
	if (sqlite3_prepare_v2(database, QRY, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "lang_vector_load prepare failed: %s\n",
			sqlite3_errmsg(database));
		return NULL;
	}
	languages = vector_create(sizeof(lang));
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		lang.id = sqlite3_column_int(stmt, 0);
		lang.display_name = strdup((const char *)sqlite3_column_text(stmt, 1));
		lang.table_name = strdup((const char *)sqlite3_column_text(stmt, 2));
		lang.column_name = strdup((const char *)sqlite3_column_text(stmt, 3));
		lang.deinflect = sqlite3_column_int(stmt, 4);
		
		vector_append(languages, &lang);
		
		if (!language_table_valid(database, &lang)) {
			fprintf(stderr, "Tables for language '%s' are invalid.\n",
				lang.display_name);
			lang_vector_destroy(languages);
			return NULL;
		}
	}
	sqlite3_finalize(stmt);
	
	return languages;
}

Dictionary *dictionary_load(const char *dict_file_path) {
	Dictionary *dict;
	sqlite3 *database;
	Vector *languages;
	
	if (sqlite3_open(dict_file_path, &database)) {
		fprintf(stderr, "Can't open dictionary database: %s\n",
			sqlite3_errmsg(database));
		sqlite3_close(database);
		return NULL;
	}
	
	if (!table_exists(database, "Languages")) {
		fprintf(stderr, "Dictionary is missing Languages table.\n");
		sqlite3_close(database);
		return NULL;
	}
	if ((languages = lang_vector_load(database)) == NULL) {
		sqlite3_close(database);
		return NULL;
	}
	if (vector_length(languages) == 0) {
		fprintf(stderr, "No languages in dictionary.\n");
		lang_vector_destroy(languages);
		sqlite3_close(database);
		return NULL;
	}
	
	dict = malloc(sizeof(*dict));
	dict->database = database;
	dict->languages = languages;
	return dict;
}

typedef struct {
	unsigned int id;
	char *japanese;
	char *pos;
	char *translation;
} Query_result;

static Vector *query_database(Dictionary *dict, const char* word,
	const Language *lang) {
	sqlite3_stmt *stmt;
	const char * const QRY_FORMAT = "SELECT id, japanese, pos, %s FROM %s WHERE "
		"id IN (SELECT ent_id FROM %s_toc WHERE word = ?);";
	char *qry;
	const char *res;
	Vector *results;
	Query_result result;
	
	qry = malloc(strlen(QRY_FORMAT) + strlen(lang->table_name)
		+ strlen(lang->table_name) + strlen(lang->column_name) + 1);
	sprintf(qry, QRY_FORMAT, lang->column_name, lang->table_name,
		lang->table_name);
	if (sqlite3_prepare_v2(dict->database, qry, -1, &stmt, NULL) != SQLITE_OK) {
		free(qry);
		fprintf(stderr, "query_database prepare failed: %s\n",
			sqlite3_errmsg(dict->database));
		return NULL;
	}
	free(qry);
	if (sqlite3_bind_text(stmt, 1, word, -1, SQLITE_STATIC) != SQLITE_OK) {;
		fprintf(stderr, "query_database bind failed: %s\n",
			sqlite3_errmsg(dict->database));
		return NULL;
	}
	
	results = vector_create(sizeof(result));
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		result.id = sqlite3_column_int(stmt, 0);
		res = (char*)sqlite3_column_text(stmt, 1);
		result.japanese = res == NULL ? calloc(1, 1) : strdup(res);
		res = (char*)sqlite3_column_text(stmt, 2);
		result.pos = res == NULL ? calloc(1, 1) : strdup(res);
		res = (char*)sqlite3_column_text(stmt, 3);
		result.translation = res == NULL ? calloc(1, 1) : strdup(res);
		vector_append(results, &result);
	}
	sqlite3_finalize(stmt);
	
	return results;
}

static void results_destroy(Vector *results) {
	const Query_result *result;
	size_t pos = 0;
	
	while ((result = vector_get_const(results, pos++))) {
		free(result->japanese);
		free(result->pos);
		free(result->translation);
	}
	vector_destroy(results);
}

static void append_matching_results(char **buf, size_t *buf_size,
	Vector *results, const jpn_Variant *variant, Vector *previous_result_ids) {
	const Query_result *result;
	size_t pos = 0;
	
	while ((result = vector_get_const(results, pos++))) {
		if (!jpn_is_correctly_deinflected(variant->type, result->pos)
			|| vector_find(previous_result_ids, &result->id, memcmp)
			!= vector_length(previous_result_ids))
			continue;
			
		vector_append(previous_result_ids, &result->id);
		
		*buf_size = strcat_realloc(buf, result->japanese, *buf_size, "\n");
		if (variant->reason != NULL)
			*buf_size = strcat_realloc(buf, variant->reason, *buf_size, "\t");
		*buf_size = strcat_realloc(buf, result->pos, *buf_size, "\n");
		*buf_size = strcat_realloc(buf, result->translation, *buf_size, "\n");
	}
}

char* dictionary_lookup(Dictionary *dict, const char* text,
	const Language *lang, Vector *rules) {
	char *text1, *text2, *buffer;
	Vector *words_lookup, *results;
	const jpn_Variant *variant;
	size_t pos_v, buffer_size;
	Vector *result_ids;
	
	text1 = jpn_half2fullwidth(text);
	text2 = jpn_katakana2hiragana(text1);
	g_free(text1);
	
	/* If we should not deinflect, act as there were no rules to apply */
	if (!lang->deinflect)
		rules = vector_create(1);
	words_lookup = jpn_get_all_variants(text2, rules);
	g_free(text2);
	
	result_ids = vector_create(sizeof(((Query_result*)NULL)->id));
	buffer = malloc(buffer_size = 2048);
	*buffer = 0;
	pos_v = 0;
	while ((variant = vector_get_const(words_lookup, pos_v++))) {
		if ((results = query_database(dict, variant->word, lang)) == NULL)
			return NULL;
		
		append_matching_results(&buffer, &buffer_size, results, variant,
			result_ids);
		results_destroy(results);
	}
	
	vector_destroy(result_ids);
	jpn_variants_destroy(words_lookup);
	if (!lang->deinflect)
		vector_destroy(rules);
	
	return buffer;
}

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

#include <string.h>
#include <stdlib.h>

/*
static char *strdup(const char *s) {
	size_t len = strlen(s) + 1;
	char *p = malloc(len);

	return p ? memcpy(p, s, len) : NULL;
}
*/

/** Counts the number of occurences of char in s.
*/
static inline size_t string_count_char(const char *s, char c) {
	size_t count;
	for (count = 0; *s; s++) {
		if (*s == c)
			count++;
	}
	return count;
}

/** Counts the number of occurences of find in s.
*/
static inline size_t string_count_string(const char *s, const char *find) {
	size_t count, len;
	for (count = 0, len = strlen(find); *s; s++) {
		if (memcmp(s, find, len) == 0) {
			count++;
			s += len - 1;
		}
	}
	return count;
}

/** Ends string at the first occurens of delim and returns what was after delim.
*/
static inline char *string_split_char(char *string, char delim) {
	for (; *string && *string != delim; string++);
	if (*string) {
		*string = 0;
		return string+1;
	} else
		return NULL;
}

/** Ends string at the first occurens of delim and returns what was after delim.
*/
static inline char *string_split_string(char *string, const char *delim) {
	size_t len = strlen(delim);
	
	for (; *string && memcmp(string, delim, len) != 0; string++);
	if (*string) {
		*string = 0;
		return string + len;
	} else
		return NULL;
}

/** Replaces all occurences of from with to in s.
*/
static inline char *string_replace_string(const char *s, const char *from,
	const char *to) {
	size_t count = string_count_string(s, from);
	size_t from_len = strlen(from);
	size_t to_len = strlen(to);
	const char *in_p = s;
	char *output, *out_p;
	
	output = out_p = malloc((to_len - from_len) * count + strlen(s) + 1);
	while (count--) {
		size_t find_pos = strstr(in_p, from) - in_p;
		strncpy(out_p, in_p, find_pos);
		out_p += find_pos;
		in_p += find_pos + from_len;
		strcpy(out_p, to);
		out_p += to_len;
	}
	strcpy(out_p, in_p);
	
	return output;
}

/** Replace the last n characters in world with suffix.
	Return this as a newly allocated string.
*/
static inline char *string_suffix_replace(const char *word, size_t n,
	const char *suffix) {
	char *new_word;
	size_t word_len = strlen(word);
	
	new_word = malloc(word_len - n + strlen(suffix) + 1);
	memcpy(new_word, word, word_len - n);
	new_word[word_len - n] = 0;
	strcat(new_word, suffix);
	
	return new_word;
}

/** Like strcat it appens source to dest. Additionally it appends sep and
	reallocates dest if dest isn't large enough. buffer_len is the number of
	bytes currenty allocated for dest.
*/
static inline size_t strcat_realloc(char **dest, const char *source,
	size_t buffer_len, const char *sep) {
	size_t source_len = strlen(source);
	size_t dest_len = strlen(*dest);
	size_t sep_len = strlen(sep);
	
	if (dest_len + source_len + sep_len + 1 > buffer_len) {
		buffer_len += ((dest_len + source_len + sep_len + 1
			- buffer_len)/1024 + 1) * 1024;
		*dest = realloc(*dest, buffer_len);
	}
	if (dest_len > 0 && sep_len > 0)
		strcat(*dest, sep);
	strcat(*dest, source);
	return buffer_len;
}

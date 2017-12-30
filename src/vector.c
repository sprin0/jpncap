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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

Vector *vector_create(size_t size) {
	Vector *vector;
	
	vector = malloc(sizeof(*vector));
	vector->size = size;
	vector->length = 0;
	vector->capacity = 16;
	vector->data = malloc(vector->capacity * vector->size);
	
	return vector;
}

void vector_destroy(Vector *vector) {
	free(vector->data);
	free(vector);
}

size_t vector_length(Vector *vector) {
	return vector->length;
}

static void vector_set_capacity(Vector *vector, size_t new_capacity) {
	vector->capacity = new_capacity;
	vector->data = realloc(vector->data, new_capacity * vector->size);
}

void vector_set(Vector *vector, size_t pos, const void *data) {
	assert(pos < vector_length(vector));
	
	memcpy((char*)vector->data + pos * vector->size, data, vector->size);
}

void vector_push(Vector *vector, const void *data) {
	vector_insert(vector, 0, data);
}

void vector_append(Vector *vector, const void *data) {
	vector_insert(vector, vector->length, data);
}

void vector_insert(Vector *vector, size_t pos, const void *data) {
	assert(pos <= vector_length(vector));
	
	if (vector->length + 1 > vector->capacity)
		vector_set_capacity(vector, 2*vector->capacity);
	
	if (pos != vector->length)
		memmove((char*)vector->data + (pos + 1) * vector->size,
			(char*)vector->data + pos * vector->size,
			(vector->length - pos) * vector->size);
	
	memcpy((char*)vector->data + pos * vector->size, data, vector->size);
	vector->length++;
}

void vector_remove(Vector *vector, size_t pos) {
	assert(pos < vector_length(vector));
	
	if (pos + 1 != vector->length)
		memmove((char*)vector->data + pos * vector->size,
			(char*)vector->data + (pos + 1) * vector->size,
			(vector->length - pos + 1) * vector->size);
			
	vector->length--;
}

void vector_clear(Vector *vector) {
	vector->length = 0;
}

void *vector_get(Vector *vector, size_t pos) {
	void *data;
	
	if (pos >= vector->length)
		return NULL;
	
	data = malloc(vector->size);
	memcpy(data, (char*)vector->data + pos * vector->size, vector->size);
	return data;
}

const void *vector_get_const(Vector *vector, size_t pos) {
	if (pos >= vector->length)
		return NULL;
	
	return (char*)vector->data + pos * vector->size;
}

size_t vector_find(Vector *vector, const void *data,
	int (*equals)(const void*, const void*, size_t)) {
	
	return vector_find_at(vector, 0, data, equals);
}

size_t vector_find_at(Vector *vector, size_t pos, const void *data,
	int (*equals)(const void*, const void*, size_t)) {
	
	size_t i;
	
	for (i = pos; i < vector->length; i++) {
		if (equals((char*)vector->data + i * vector->size, data, vector->size)
			== 0)
			break;
	}
	return i;
}

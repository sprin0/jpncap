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

#include <stddef.h>

typedef struct {
	size_t size;
	size_t length;
	size_t capacity;
	void *data;
} Vector;

/** Create a new vector.
	size specifies the size in bytes of one element in the vector
*/
Vector *vector_create(size_t size);

/** Frees all memory of vector.
*/
void vector_destroy(Vector *vector);

/** Returns the length of vector
*/
size_t vector_length(Vector *vector);

/** Sets the element at pos in vector to data.
	pos must be less than the vector's length.
*/
void vector_set(Vector *vector, size_t pos, const void *data);

/** Copies data to the front of vector
	Exactly size bytes (as specified by the vector_create call) are being copied
	into the vector.
*/
void vector_push(Vector *vector, const void *data);

/** Copies data to the back of vector.
	Exactly size bytes (as specified by the vector_create call) are being copied
	into the vector.
*/
void vector_append(Vector *vector, const void *data);

/** Copies data to position pos in vector.
	Exactly size bytes (as specified by the vector_create call) are being copied
	into the vector.
	pos must be less or equal to the vector's length.
*/
void vector_insert(Vector *vector, size_t pos, const void *data);

/** Removes the element at pos from vector.
	pos must be less than the vector's length.
*/
void vector_remove(Vector *vector, size_t pos);

/** Removes all elements of vector.
*/
void vector_clear(Vector *vector);

/** Returns the element at pos of vector. The return value must be freed.
	If pos is equal or greater than the vector's length, it returns NULL.
*/
void *vector_get(Vector *vector, size_t pos);

/** Returns the element at pos of vector. The return value is constant and must
	not be freed. The returned pointer becomes invalid upon calling
	vector_insert, vector_push or vector_append.
	If pos is equal or greater than the vector's length, it returns NULL.
*/
const void *vector_get_const(Vector *vector, size_t pos);

/**	Returns first occurence of element data in vector. If the element
	could not be found, it returns the vector's length.
	equals is a function that returns 0 on equality of both parameters. This is
	used to find the element in the vector.
*/
size_t vector_find(Vector *vector, const void *data,
	int (*equals)(const void*, const void*, size_t));

/**	Returns first occurence from pos of element data in vector. If the element
	could not be found, it returns the vector's length.
	equals is a function that returns 0 on equality of both parameters. This is
	used to find the element in the vector.
*/
size_t vector_find_at(Vector *vector, size_t pos, const void *data,
	int (*equals)(const void*, const void*, size_t));
	

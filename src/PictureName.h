/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PICTURE_NAME_H
#define PICTURE_NAME_H


#include <String.h>


/*!	The name of the picture of a video in a folder, without ".jpg": its id,
	or, when the id is a path or an address (the files of the library,
	torrents), a hash of it, since it could not be a name.
*/
static inline BString
PictureName(const char* id)
{
	BString name(id);
	if (name.FindFirst('/') < 0 && name.FindFirst(':') < 0
		&& name.Length() < 100)
		return name;
	uint64 hash = 14695981039346656037ULL;
	for (const char* c = id; *c != '\0'; c++) {
		hash ^= (uint8)*c;
		hash *= 1099511628211ULL;
	}
	name.SetToFormat("%016" B_PRIx64, hash);
	return name;
}


#endif	// PICTURE_NAME_H

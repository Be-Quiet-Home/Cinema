/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CATEGORIES_H
#define CATEGORIES_H


#include <String.h>

#include <vector>


struct Category {
	BString	name;	// shown
	BString	query;	// searched on YouTube
};


/*!	The rows of the top bar, chosen by the parents in a text file in the
	settings folder (Cinema/categories), one per line: "Name = search". The
	file is written with a few examples the first time.
*/
std::vector<Category>	LoadCategories();


#endif	// CATEGORIES_H

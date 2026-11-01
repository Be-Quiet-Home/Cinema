/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CATEGORIES_H
#define CATEGORIES_H


#include <String.h>

#include <vector>


struct Category {
	BString	name;		// shown
	BString	query;		// searched on YouTube
	BString	channel;	// or the id of a channel, whose latest videos are
						// shown

	bool	IsChannel() const { return !channel.IsEmpty(); }
};


/*!	The tabs of the top bar, in a text file in the settings folder
	(Cinema/categories), one per line: "Name = search", or
	"Name = channel:<id>" for a channel. The file is written with a few
	examples the first time.
*/
std::vector<Category>	LoadCategories();
void					SaveCategories(
							const std::vector<Category>& categories);
								// keeps the comments of the file


#endif	// CATEGORIES_H

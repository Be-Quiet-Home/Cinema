/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef FAVORITES_H
#define FAVORITES_H


class BMessage;


/*!	What was given a star, of any provider: the TV channels one watches
	among many, a video to keep at hand. Kept in the settings folder
	(Cinema/favorites, JSON), the latest first; the helper reads the same
	file for the tab of the favorites ("@favorites channel:all").
*/
namespace Favorites {

bool		Has(const char* id);
bool		Toggle(const BMessage& item);
				// "provider", "kind", "id", "title", "description",
				// "channel", "channel id", "duration", "year", "poster",
				// "thumbnail", as the grid sends them; true when it is a
				// favorite now
bool		IsEmpty();

}


#endif	// FAVORITES_H

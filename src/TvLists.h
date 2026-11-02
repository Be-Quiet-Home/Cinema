/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef TV_LISTS_H
#define TV_LISTS_H


#include <String.h>


/*!	The public lists of free television channels, one per country, that
	the iptv-org project keeps (https://github.com/iptv-org/iptv): only
	streams that the channels put on line for everyone.
*/
namespace TvLists {

BString		Address(const char* country);
				// of the list of a country, by its ISO 3166 code
BString		SystemCountry();
				// the country of the system, or empty

}


#endif	// TV_LISTS_H

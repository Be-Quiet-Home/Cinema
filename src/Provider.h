/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PROVIDER_H
#define PROVIDER_H


#include <String.h>

#include <vector>


class BMessage;


/*!	A source of media, as cinema-helper describes it: YouTube today, other
	catalogues when there are some. Its kinds say what it can do: "search",
	"channel", "find", "download".
*/
struct Provider {
	BString					id;		// "youtube"
	BString					name;	// "YouTube"
	std::vector<BString>	kinds;

	bool					Supports(const char* kind) const;
};


/*!	The providers in a kMsgProviders, one nested "provider" message each:
	"id", "name" and a "kinds" for each kind.
*/
std::vector<Provider>		ProvidersFromMessage(const BMessage& message);


/*!	The id of the video in a YouTube address, or an empty string: watch?v=,
	&v=, youtu.be/, /shorts/, /embed/ and /live/ links.
*/
BString					YouTubeVideoId(const char* url);


#endif	// PROVIDER_H

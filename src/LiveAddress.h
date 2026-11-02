/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef LIVE_ADDRESS_H
#define LIVE_ADDRESS_H


#include <String.h>


/*!	A live stream as the helper gives it to play with FFmpeg rather than
	the Media Kit: "cinema-live:?url=...&ua=...&referer=...&height=...",
	each value percent-encoded. The user agent and the referrer are what
	the list of channels asks for; height is the highest picture wanted,
	0 for any.
*/
struct LiveAddress {
	BString		url;
	BString		agent;
	BString		referrer;
	int32		height;

				LiveAddress() : height(0) {}

	static	bool	Is(const char* address);
	static	bool	Parse(const char* address, LiveAddress& live);
	static	BString	Url(const char* address);
						// the address of the stream itself, for another
						// player; the address given when it is not live
};


#endif	// LIVE_ADDRESS_H

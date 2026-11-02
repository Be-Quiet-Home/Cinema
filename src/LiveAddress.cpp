/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "LiveAddress.h"

#include <stdlib.h>
#include <string.h>


static const char* kPrefix = "cinema-live:?";


static int
hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


static BString
decode(const char* text, int32 length)
{
	BString result;
	for (int32 i = 0; i < length; i++) {
		if (text[i] == '+')
			result << ' ';
		else if (text[i] == '%' && i + 2 < length
			&& hex_value(text[i + 1]) >= 0 && hex_value(text[i + 2]) >= 0) {
			result << (char)(hex_value(text[i + 1]) * 16
				+ hex_value(text[i + 2]));
			i += 2;
		} else
			result << text[i];
	}
	return result;
}


bool
LiveAddress::Is(const char* address)
{
	return address != NULL && strncmp(address, kPrefix, strlen(kPrefix)) == 0;
}


bool
LiveAddress::Parse(const char* address, LiveAddress& live)
{
	if (!Is(address))
		return false;
	live = LiveAddress();
	const char* field = address + strlen(kPrefix);
	while (*field != '\0') {
		const char* end = strchr(field, '&');
		int32 length = end != NULL ? end - field : strlen(field);
		const char* equal = (const char*)memchr(field, '=', length);
		if (equal != NULL) {
			BString key(field, equal - field);
			BString value = decode(equal + 1, length - (equal + 1 - field));
			if (key == "url")
				live.url = value;
			else if (key == "ua")
				live.agent = value;
			else if (key == "referer")
				live.referrer = value;
			else if (key == "height")
				live.height = atoi(value.String());
		}
		if (end == NULL)
			break;
		field = end + 1;
	}
	return !live.url.IsEmpty();
}


BString
LiveAddress::Url(const char* address)
{
	LiveAddress live;
	if (Parse(address, live))
		return live.url;
	return address;
}

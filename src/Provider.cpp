/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Provider.h"

#include <ctype.h>
#include <Message.h>
#include <string.h>


bool
Provider::Supports(const char* kind) const
{
	for (const BString& supported : kinds)
		if (supported == kind)
			return true;
	return false;
}


BString
YouTubeVideoId(const char* address)
{
	BString url(address);
	static const char* kMarkers[] = { "watch?v=", "&v=", "youtu.be/",
		"/shorts/", "/embed/", "/live/" };
	if (url.IFindFirst("youtube.com") < 0 && url.IFindFirst("youtu.be") < 0)
		return BString();
	for (const char* marker : kMarkers) {
		int32 start = url.FindFirst(marker);
		if (start < 0)
			continue;
		start += strlen(marker);
		int32 end = start;
		while (end < url.Length()
			&& (isalnum((unsigned char)url.ByteAt(end))
				|| url.ByteAt(end) == '-' || url.ByteAt(end) == '_'))
			end++;
		BString id;
		url.CopyInto(id, start, end - start);
		if (id.Length() == 11)
			return id;
	}
	return BString();
}


std::vector<Provider>
ProvidersFromMessage(const BMessage& message)
{
	std::vector<Provider> providers;
	BMessage entry;
	for (int32 i = 0; message.FindMessage("provider", i, &entry) == B_OK; i++) {
		Provider provider;
		provider.id = entry.GetString("id", "");
		provider.name = entry.GetString("name", "");
		const char* kind;
		for (int32 j = 0; entry.FindString("kinds", j, &kind) == B_OK; j++)
			provider.kinds.push_back(kind);
		providers.push_back(provider);
	}
	return providers;
}

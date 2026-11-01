/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Provider.h"

#include <Message.h>


bool
Provider::Supports(const char* kind) const
{
	for (const BString& supported : kinds)
		if (supported == kind)
			return true;
	return false;
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

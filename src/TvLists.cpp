/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "TvLists.h"

#include <FormattingConventions.h>
#include <Locale.h>
#include <LocaleRoster.h>
#include <Message.h>

#include <string.h>


BString
TvLists::Address(const char* country)
{
	BString code(country);
	code.ToLower();
	// iptv-org names the United Kingdom as its domain does
	if (code == "gb")
		code = "uk";
	BString address("https://iptv-org.github.io/iptv/countries/");
	address << code << ".m3u";
	return address;
}


BString
TvLists::SystemCountry()
{
	BFormattingConventions conventions;
	if (BLocale::Default()->GetFormattingConventions(&conventions) == B_OK) {
		BString country(conventions.CountryCode());
		if (!country.IsEmpty())
			return country;
	}

	// from the language, when it is spoken mostly in one country
	static const char* kCountries[][2] = {
		{ "it", "it" }, { "de", "de" }, { "fr", "fr" }, { "es", "es" },
		{ "pt", "pt" }, { "ja", "jp" }, { "ko", "kr" }, { "uk", "ua" },
		{ "zh", "cn" }, { "nl", "nl" }, { "pl", "pl" }, { "sv", "se" },
		{ "ru", "ru" }, { "el", "gr" }, { "tr", "tr" }, { "cs", "cz" },
		{ "hu", "hu" }, { "fi", "fi" }, { "da", "dk" }, { "nb", "no" },
	};
	BMessage languages;
	const char* language;
	if (BLocaleRoster::Default()->GetPreferredLanguages(&languages) == B_OK
		&& languages.FindString("language", &language) == B_OK) {
		for (const auto& pair : kCountries) {
			if (strncmp(language, pair[0], 2) == 0)
				return pair[1];
		}
	}
	return "";
}

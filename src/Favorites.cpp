/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Favorites.h"


#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <String.h>

#include <string>
#include <vector>

#include "Json.h"


static std::vector<Json> sFavorites;
static bool sLoaded = false;


static BString
favorites_path()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return "";
	path.Append("Cinema/favorites");
	return path.Path();
}


static void
load()
{
	if (sLoaded)
		return;
	sLoaded = true;
	BFile file(favorites_path().String(), B_READ_ONLY);
	off_t size;
	if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK
		|| size <= 0 || size > 16 * 1024 * 1024)
		return;
	std::string text(size, '\0');
	if (file.Read(&text[0], size) != size)
		return;
	bool ok = false;
	Json list = Json::Parse(text, &ok);
	if (ok && list.IsArray())
		sFavorites = list.Elements();
}


static void
save()
{
	Json list = Json::MakeArray();
	for (const Json& item : sFavorites)
		list.Add(item);
	std::string text = list.ToString();
	BString path = favorites_path();
	BString temporary(path);
	temporary << ".part";
	BFile file(temporary.String(), B_WRITE_ONLY | B_CREATE_FILE
		| B_ERASE_FILE);
	if (file.InitCheck() != B_OK
		|| file.Write(text.c_str(), text.size()) != (ssize_t)text.size()) {
		BEntry(temporary.String()).Remove();
		return;
	}
	file.Sync();
	BEntry(temporary.String()).Rename(path.String(), true);
}


bool
Favorites::Has(const char* id)
{
	load();
	for (const Json& item : sFavorites) {
		if (item.GetString("id") == id)
			return true;
	}
	return false;
}


bool
Favorites::IsEmpty()
{
	load();
	return sFavorites.empty();
}


bool
Favorites::Toggle(const BMessage& message)
{
	load();
	BString id = message.GetString("id", "");
	if (id.IsEmpty())
		return false;
	for (size_t i = 0; i < sFavorites.size(); i++) {
		if (sFavorites[i].GetString("id") == id.String()) {
			sFavorites.erase(sFavorites.begin() + i);
			save();
			return false;
		}
	}

	// as the helper gives a MediaItem
	Json item = Json::MakeObject();
	item.Set("provider", message.GetString("provider", ""));
	item.Set("kind", message.GetString("kind", "video"));
	item.Set("id", id.String());
	item.Set("title", message.GetString("title", ""));
	item.Set("description", message.GetString("description", ""));
	item.Set("channel", message.GetString("channel", ""));
	item.Set("channel_id", message.GetString("channel id", ""));
	item.Set("duration", (int)message.GetInt32("duration", 0));
	item.Set("year", (int)message.GetInt32("year", 0));
	item.Set("poster", message.GetString("poster", ""));
	item.Set("thumbnail", message.GetString("thumbnail", ""));
	sFavorites.insert(sFavorites.begin(), item);
	save();
	return true;
}

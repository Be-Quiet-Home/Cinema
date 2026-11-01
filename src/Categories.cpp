/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Categories.h"

#include <stdio.h>
#include <string.h>

#include <Catalog.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Path.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Categories"


static void
write_examples(const char* path)
{
	FILE* file = fopen(path, "w");
	if (file == NULL)
		return;
	fprintf(file, "%s\n", B_TRANSLATE("# One tab of the top bar per line: "
		"name = what to search on YouTube"));
	fprintf(file, "%s\n", B_TRANSLATE("# or name = channel:<id> for the "
		"latest videos of a channel"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("News"),
		B_TRANSLATE("news today"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Music"),
		B_TRANSLATE("official music video"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Technology"),
		B_TRANSLATE("technology review"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Science"),
		B_TRANSLATE("science explained"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Documentaries"),
		B_TRANSLATE("full documentary"));
	fclose(file);
}


static const char* kChannelPrefix = "channel:";


static bool
categories_path(BPath& path)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return false;
	path.Append("Cinema");
	create_directory(path.Path(), 0755);
	return path.Append("categories") == B_OK;
}


std::vector<Category>
LoadCategories()
{
	std::vector<Category> categories;
	BPath path;
	if (!categories_path(path))
		return categories;

	FILE* file = fopen(path.Path(), "r");
	if (file == NULL) {
		write_examples(path.Path());
		file = fopen(path.Path(), "r");
		if (file == NULL)
			return categories;
	}

	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		BString text(line);
		text.RemoveAll("\n").RemoveAll("\r");
		int32 equals = text.FindFirst('=');
		if (equals <= 0 || text.ByteAt(0) == '#')
			continue;
		Category category;
		text.CopyInto(category.name, 0, equals);
		text.CopyInto(category.query, equals + 1, text.Length() - equals - 1);
		category.name.Trim();
		category.query.Trim();
		if (category.query.StartsWith(kChannelPrefix)) {
			category.query.CopyInto(category.channel, strlen(kChannelPrefix),
				category.query.Length() - strlen(kChannelPrefix));
			category.channel.Trim();
			category.query = "";
			if (category.channel.IsEmpty())
				continue;
		}
		if (!category.name.IsEmpty()
			&& (!category.query.IsEmpty() || category.IsChannel()))
			categories.push_back(category);
	}
	fclose(file);
	return categories;
}


void
SaveCategories(const std::vector<Category>& categories)
{
	BPath path;
	if (!categories_path(path))
		return;

	// the comments stay at the top
	BString comments;
	FILE* file = fopen(path.Path(), "r");
	if (file != NULL) {
		char line[512];
		while (fgets(line, sizeof(line), file) != NULL) {
			if (line[0] == '#')
				comments << line;
		}
		fclose(file);
	}

	BString temporary(path.Path());
	temporary << ".part";
	file = fopen(temporary.String(), "w");
	if (file == NULL)
		return;
	fputs(comments.String(), file);
	for (const Category& category : categories) {
		BString name(category.name);
		name.ReplaceAll("=", "-");
		if (category.IsChannel()) {
			fprintf(file, "%s = %s%s\n", name.String(), kChannelPrefix,
				category.channel.String());
		} else
			fprintf(file, "%s = %s\n", name.String(), category.query.String());
	}
	if (fclose(file) == 0)
		rename(temporary.String(), path.Path());
	else
		remove(temporary.String());
}

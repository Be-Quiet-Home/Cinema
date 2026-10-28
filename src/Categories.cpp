/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Categories.h"

#include <stdio.h>

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
	fprintf(file, "%s\n", B_TRANSLATE("# One row of the top bar per line: "
		"name = what to search on YouTube"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Cartoons"),
		B_TRANSLATE("cartoons for children full episodes"));
	fprintf(file, "%s = %s\n", "Peppa Pig",
		B_TRANSLATE("Peppa Pig full episodes"));
	fprintf(file, "%s = %s\n", "Bluey", B_TRANSLATE("Bluey full episodes"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Songs"),
		B_TRANSLATE("songs for children"));
	fprintf(file, "%s = %s\n", B_TRANSLATE("Animals"),
		B_TRANSLATE("animals documentary for children"));
	fclose(file);
}


std::vector<Category>
LoadCategories()
{
	std::vector<Category> categories;
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return categories;
	path.Append("Cinema");
	create_directory(path.Path(), 0755);
	path.Append("categories");

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
		if (!category.name.IsEmpty() && !category.query.IsEmpty())
			categories.push_back(category);
	}
	fclose(file);
	return categories;
}

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Positions.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <Autolock.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Locker.h>
#include <Path.h>

#include <vector>


namespace Positions {

static const int32 kMost = 500;
static const bigtime_t kNotWorth = 30000000;
	// the first and the last 30 seconds: starting again there is pointless
static const double kWatched = 0.92;

static BLocker sLock("positions");


struct Entry {
	BString		id;
	Position	position;
};


static bool
positions_path(BPath& path)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return false;
	path.Append("Cinema");
	create_directory(path.Path(), 0755);
	return path.Append("positions") == B_OK;
}


static std::vector<Entry>
load()
{
	std::vector<Entry> entries;
	BPath path;
	if (!positions_path(path))
		return entries;
	FILE* file = fopen(path.Path(), "r");
	if (file == NULL)
		return entries;

	// "position duration watched when id", the id last as it may hold
	// anything but a new line
	char* line = NULL;
	size_t size = 0;
	while (getline(&line, &size, file) >= 0) {
		long long position, duration, when;
		int watched, length = 0;
		if (sscanf(line, "%lld %lld %d %lld %n", &position, &duration,
				&watched, &when, &length) < 4 || length <= 0)
			continue;
		Entry entry;
		entry.id = line + length;
		entry.id.RemoveAll("\n").RemoveAll("\r");
		if (entry.id.IsEmpty())
			continue;
		entry.position.position = position;
		entry.position.duration = duration;
		entry.position.watched = watched != 0;
		entry.position.when = when;
		entries.push_back(entry);
	}
	free(line);
	fclose(file);
	return entries;
}


static void
save(const std::vector<Entry>& entries)
{
	BPath path;
	if (!positions_path(path))
		return;
	BString temporary(path.Path());
	temporary << ".part";
	FILE* file = fopen(temporary.String(), "w");
	if (file == NULL)
		return;
	for (const Entry& entry : entries) {
		BString id(entry.id);
		id.ReplaceAll("\n", " ");
		fprintf(file, "%lld %lld %d %lld %s\n",
			(long long)entry.position.position,
			(long long)entry.position.duration,
			entry.position.watched ? 1 : 0, (long long)entry.position.when,
			id.String());
	}
	if (fclose(file) == 0)
		rename(temporary.String(), path.Path());
	else
		remove(temporary.String());
}


Position
Get(const char* id)
{
	BAutolock locker(sLock);
	for (const Entry& entry : load()) {
		if (entry.id == id)
			return entry.position;
	}
	return Position();
}


void
Set(const char* id, bigtime_t position, bigtime_t duration)
{
	if (id == NULL || id[0] == '\0')
		return;
	BAutolock locker(sLock);
	std::vector<Entry> entries = load();
	Entry entry;
	entry.id = id;
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].id == id) {
			entry = entries[i];
			entries.erase(entries.begin() + i);
			break;
		}
	}
	entry.position.position = position;
	if (duration > 0)
		entry.position.duration = duration;
	if (entry.position.duration > 0
		&& position >= entry.position.duration * kWatched)
		entry.position.watched = true;
	entry.position.when = time(NULL);
	entries.insert(entries.begin(), entry);
	if ((int32)entries.size() > kMost)
		entries.resize(kMost);
	save(entries);
}


bigtime_t
ResumeAt(const char* id)
{
	Position position = Get(id);
	if (position.position < kNotWorth)
		return 0;
	if (position.duration > 0
		&& position.position > position.duration - kNotWorth)
		return 0;
	return position.position;
}

}	// namespace Positions

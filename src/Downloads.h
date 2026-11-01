/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef DOWNLOADS_H
#define DOWNLOADS_H


#include <String.h>

#include <map>
#include <vector>

#include "MediaItem.h"


class BMessage;


/*!	The videos downloaded to watch offline, in the Cinema folder of the home
	folder. What they are (provider, kind, id, title, channel, length) is kept
	in attributes of the files, their pictures in its .pictures folder.
*/
namespace Downloads {

BString				Folder();
						// made if needed; empty if it can't be
std::vector<MediaItem>	Load(std::map<BString, BString>* files);
						// the newest first, with their files and pictures;
						// files: their paths, by id
void				KeepPicture(const char* cache, const char* id);
void				Describe(const char* path, const BMessage& video);
						// "id", "title", "channel", "channel id", "duration"
status_t			MoveToTrash(const char* path);

}	// namespace Downloads


#endif	// DOWNLOADS_H

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Downloads.h"

#include <algorithm>

#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Mime.h>
#include <Node.h>
#include <Path.h>
#include <TranslationUtils.h>
#include <TypeConstants.h>
#include <Volume.h>


namespace Downloads {

static const char* kIdAttribute = "Cinema:id";
static const char* kChannelAttribute = "Cinema:channel";
static const char* kChannelIdAttribute = "Cinema:channel_id";
static const char* kDurationAttribute = "Cinema:duration";
static const char* kTitleAttribute = "Media:Title";


BString
Folder()
{
	BPath path;
	if (find_directory(B_USER_DIRECTORY, &path) != B_OK
		|| path.Append("Cinema") != B_OK
		|| create_directory(path.Path(), 0755) != B_OK)
		return "";
	return path.Path();
}


static BString
picture_path(const char* id)
{
	BString path(Folder());
	path << "/.pictures/" << id << ".jpg";
	return path;
}


std::vector<Video>
Load(std::map<BString, BString>* files)
{
	struct Found {
		Video	video;
		time_t	time;
	};
	std::vector<Found> found;

	BDirectory folder(Folder().String());
	BEntry entry;
	while (folder.GetNextEntry(&entry) == B_OK) {
		BNode node(&entry);
		BString id;
		if (!entry.IsFile() || node.ReadAttrString(kIdAttribute, &id) != B_OK)
			continue;
		BPath path;
		entry.GetPath(&path);

		Found file;
		file.video.id = id;
		file.video.path = path.Path();
		if (node.ReadAttrString(kTitleAttribute, &file.video.title) != B_OK)
			file.video.title = path.Leaf();
		node.ReadAttrString(kChannelAttribute, &file.video.channel);
		node.ReadAttrString(kChannelIdAttribute, &file.video.channelId);
		node.ReadAttr(kDurationAttribute, B_INT32_TYPE, 0,
			&file.video.duration, sizeof(int32));
		file.video.downloaded = true;
		file.time = 0;
		entry.GetModificationTime(&file.time);
		found.push_back(file);
		if (files != NULL)
			(*files)[id] = path.Path();
	}

	std::sort(found.begin(), found.end(),
		[](const Found& a, const Found& b) { return a.time > b.time; });
	std::vector<Video> videos;
	for (Found& file : found) {
		file.video.picture = BTranslationUtils::GetBitmap(
			picture_path(file.video.id).String());
		videos.push_back(file.video);
	}
	return videos;
}


void
KeepPicture(const char* cache, const char* id)
{
	BString to = picture_path(id);
	if (BEntry(to.String()).Exists())
		return;
	BString pictures(Folder());
	pictures << "/.pictures";
	create_directory(pictures.String(), 0755);

	BString from(cache);
	from << "/" << id << ".jpg";
	BFile source(from.String(), B_READ_ONLY);
	off_t size;
	if (source.InitCheck() != B_OK || source.GetSize(&size) != B_OK
		|| size <= 0 || size > 1024 * 1024)
		return;
	char* data = new char[size];
	BFile target(to.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	bool copied = source.Read(data, size) == size
		&& target.Write(data, size) == size;
	delete[] data;
	if (!copied)
		BEntry(to.String()).Remove();
}


void
Describe(const char* path, const BMessage& video)
{
	BNode node(path);
	if (node.InitCheck() != B_OK)
		return;
	BString text = video.GetString("id", "");
	node.WriteAttrString(kIdAttribute, &text);
	text = video.GetString("title", "");
	node.WriteAttrString(kTitleAttribute, &text);
	text = video.GetString("channel", "");
	node.WriteAttrString(kChannelAttribute, &text);
	text = video.GetString("channel id", "");
	node.WriteAttrString(kChannelIdAttribute, &text);
	int32 duration = video.GetInt32("duration", 0);
	node.WriteAttr(kDurationAttribute, B_INT32_TYPE, 0, &duration,
		sizeof(duration));
	update_mime_info(path, false, true, B_UPDATE_MIME_INFO_NO_FORCE);
}


status_t
MoveToTrash(const char* path)
{
	BEntry entry(path);
	struct stat info;
	status_t status = entry.GetStat(&info);
	if (status != B_OK)
		return status;
	BVolume volume(info.st_dev);
	BPath trash;
	status = find_directory(B_TRASH_DIRECTORY, &trash, true, &volume);
	if (status != B_OK)
		return status;
	BDirectory directory(trash.Path());

	// as Tracker does, so that it can be put back from the Trash
	BNode node(&entry);
	BString original(path);
	node.WriteAttrString("_trk/original_path", &original);
	return entry.MoveTo(&directory, NULL, false);
}

}	// namespace Downloads

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Worker.h"

#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <MessageQueue.h>
#include <Path.h>

#include "Messages.h"
#include "Process.h"
#include "PictureName.h"


Worker::Worker(BMessenger target)
	:
	BLooper("worker"),
	fTarget(target)
{
	BPath path;
	if (find_directory(B_USER_CACHE_DIRECTORY, &path) == B_OK) {
		path.Append("Cinema");
		create_directory(path.Path(), 0755);
		fCache = path.Path();
	}
}


void
Worker::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgThumbnails:
			_Thumbnails(message);
			break;
		case kMsgFetchPicture:
			_Picture(message);
			break;
		case kMsgFollow:
			_Follow(message);
			break;
		default:
			BLooper::MessageReceived(message);
	}
}


/*!	One picture, to the file asked; false if it could not be got. */
bool
Worker::_Get(const char* url, const char* path)
{
	BString temporary(path);
	temporary << ".part";
	if (RunProgram({ "curl", "-sfL", "--max-time", "30", "-o", temporary,
			url }) != 0 || BEntry(temporary.String()).Rename(path, true)
				!= B_OK) {
		BEntry(temporary.String()).Remove();
		return false;
	}
	return true;
}


/*!	The pictures, one by one, as long as the pictures of another search do
	not wait.
*/
void
Worker::_Thumbnails(BMessage* message)
{
	int32 generation = message->GetInt32("generation", 0);
	const char* id;
	for (int32 i = 0; message->FindString("id", i, &id) == B_OK; i++) {
		if (MessageQueue()->FindMessage(kMsgThumbnails, 0) != NULL)
			return;

		BString path(fCache);
		path << "/" << PictureName(id) << ".jpg";
		if (!BEntry(path.String()).Exists()) {
			// the address the provider gave, or YouTube's from the id
			BString url = message->GetString("url", i, "");
			if (url.IsEmpty())
				url.SetToFormat("https://i.ytimg.com/vi/%s/mqdefault.jpg", id);
			if (!_Get(url.String(), path.String()))
				continue;
		}

		BMessage thumbnail(kMsgThumbnail);
		thumbnail.AddInt32("generation", generation);
		thumbnail.AddString("id", id);
		thumbnail.AddString("path", path);
		fTarget.SendMessage(&thumbnail);
	}
}


/*!	The big picture of a video, for the detail view. It goes to a file of its
	own, so that the small one in the grid is not replaced.
*/
void
Worker::_Picture(BMessage* message)
{
	BString id = message->GetString("id", "");
	BString url = message->GetString("url", "");
	if (id.IsEmpty() || url.IsEmpty())
		return;
	// a path or an address gives a hash, as for the small pictures
	BString path(fCache);
	path << "/poster-" << PictureName(id) << ".jpg";
	if (!BEntry(path.String()).Exists()) {
		if (!_Get(url.String(), path.String())) {
			// the biggest picture may not be there for an old video; the
			// one every video has is
			BString fallback;
			fallback.SetToFormat("https://i.ytimg.com/vi/%s/hqdefault.jpg",
				id.String());
			if (!_Get(fallback.String(), path.String()))
				return;
		}
	}

	BMessage picture(kMsgPicture);
	picture.AddString("id", id);
	picture.AddString("path", path);
	fTarget.SendMessage(&picture);
}


/*!	The addresses of YouTube's files go through two redirects, which
	MediaPlayer's HTTP streaming does not follow.
*/
void
Worker::_Follow(BMessage* message)
{
	BString url = message->GetString("url", "");
	BString final;
	// the relay of the helper starts a stream for each request
	if (!url.StartsWith("http://127.0.0.1:")
		&& RunProgram({ "curl", "-s", "-o", "/dev/null", "-L", "-r", "0-0",
			"--max-time", "20", "-w", "%{url_effective}", url }, &final) == 0
		&& final.StartsWith("http"))
		url = final;

	BMessage ready(kMsgStreamReady);
	ready.AddString("id", message->GetString("id", ""));
	ready.AddString("url", url);
	fTarget.SendMessage(&ready);
}

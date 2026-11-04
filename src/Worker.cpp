/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Worker.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <MessageQueue.h>
#include <Path.h>

#include <algorithm>
#include <vector>

#include "Messages.h"
#include "Pictures.h"
#include "Process.h"
#include "PictureName.h"


Worker::Worker(BMessenger target)
	:
	BLooper("worker"),
	fTarget(target),
	fStopping(false),
	fPictureLock("worker pictures")
{
	BPath path;
	if (find_directory(B_USER_CACHE_DIRECTORY, &path) == B_OK) {
		path.Append("Cinema");
		create_directory(path.Path(), 0755);
		fCache = path.Path();
	}
}


void
Worker::Stop()
{
	fStopping = true;
	// kept until the team goes, so that no other picture is begun
	fPictureLock.Lock();
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


/*!	The pictures, a few at a time: one curl for several, so that the
	connection to the server and its TLS are made once. Each is read and
	made as small as it is shown ("largest") here rather than in the
	window, which then only shows it ("picture", its own to delete). When
	the pictures of another search wait, those not got yet go after them:
	the shelves of the home all ask at once.
*/
void
Worker::_Thumbnails(BMessage* message)
{
	static const int32 kBatch = 6;
	int32 generation = message->GetInt32("generation", 0);
	float largest = message->GetFloat("largest", Pictures::kTile);
	int32 count = 0;
	type_code type;
	message->GetInfo("id", &type, &count);

	for (int32 first = 0; first < count && !fStopping; first += kBatch) {
		if (first > 0 && MessageQueue()->FindMessage(kMsgThumbnails, 0)
				!= NULL) {
			BMessage rest(kMsgThumbnails);
			rest.AddInt32("generation", generation);
			rest.AddFloat("largest", largest);
			for (int32 i = first; i < count; i++) {
				rest.AddString("id", message->GetString("id", i, ""));
				rest.AddString("url", message->GetString("url", i, ""));
			}
			PostMessage(&rest);
			return;
		}

		int32 last = std::min(count, first + kBatch);
		std::vector<BString> paths;
		std::vector<BString> arguments = { "curl", "-sfL", "--max-time", "60" };
		for (int32 i = first; i < last; i++) {
			const char* id = message->GetString("id", i, "");
			BString path(fCache);
			path << "/" << PictureName(id) << ".jpg";
			paths.push_back(path);
			if (BEntry(path.String()).Exists())
				continue;
			// the address the provider gave, or YouTube's from the id
			BString url = message->GetString("url", i, "");
			if (url.IsEmpty())
				url.SetToFormat("https://i.ytimg.com/vi/%s/mqdefault.jpg", id);
			BString temporary(path);
			temporary << ".part";
			arguments.push_back("-o");
			arguments.push_back(temporary);
			arguments.push_back(url);
		}
		if (arguments.size() > 4) {
			// curl goes on after a picture that fails: each is looked at
			RunProgram(arguments);
			for (const BString& path : paths) {
				BString temporary(path);
				temporary << ".part";
				BEntry part(temporary.String());
				if (part.Exists() && part.Rename(path.String(), true) != B_OK)
					part.Remove();
			}
		}

		for (int32 i = first; i < last && !fStopping; i++) {
			const BString& path = paths[i - first];
			if (!BEntry(path.String()).Exists())
				continue;
			BMessage thumbnail(kMsgThumbnail);
			thumbnail.AddInt32("generation", generation);
			thumbnail.AddString("id", message->GetString("id", i, ""));
			thumbnail.AddString("path", path);
			BBitmap* picture = NULL;
			{
				BAutolock locker(fPictureLock);
				if (fStopping)
					return;
				picture = Pictures::Load(path.String(), largest);
			}
			if (picture != NULL)
				thumbnail.AddPointer("picture", picture);
			if (fTarget.SendMessage(&thumbnail) != B_OK)
				delete picture;
		}
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
	if (url.StartsWith("http") && !url.StartsWith("http://127.0.0.1:")
		&& RunProgram({ "curl", "-s", "-o", "/dev/null", "-L", "-r", "0-0",
			"--max-time", "20", "-w", "%{url_effective}", url }, &final) == 0
		&& final.StartsWith("http"))
		url = final;

	BMessage ready(kMsgStreamReady);
	ready.AddString("id", message->GetString("id", ""));
	ready.AddString("url", url);
	fTarget.SendMessage(&ready);
}

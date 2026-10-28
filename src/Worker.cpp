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
		case kMsgFollow:
			_Follow(message);
			break;
		default:
			BLooper::MessageReceived(message);
	}
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
		path << "/" << id << ".jpg";
		if (!BEntry(path.String()).Exists()) {
			BString temporary(path);
			temporary << ".part";
			BString url;
			url.SetToFormat("https://i.ytimg.com/vi/%s/mqdefault.jpg", id);
			if (RunProgram({ "curl", "-sfL", "--max-time", "20", "-o",
					temporary, url }) != 0
				|| BEntry(temporary.String()).Rename(path.String(), true)
					!= B_OK) {
				BEntry(temporary.String()).Remove();
				continue;
			}
		}

		BMessage thumbnail(kMsgThumbnail);
		thumbnail.AddInt32("generation", generation);
		thumbnail.AddString("id", id);
		thumbnail.AddString("path", path);
		fTarget.SendMessage(&thumbnail);
	}
}


/*!	The addresses of YouTube's files go through two redirects, which
	MediaPlayer's HTTP streaming does not follow.
*/
void
Worker::_Follow(BMessage* message)
{
	BString url = message->GetString("url", "");
	BString final;
	if (RunProgram({ "curl", "-s", "-o", "/dev/null", "-L", "-r", "0-0",
			"--max-time", "20", "-w", "%{url_effective}", url }, &final) == 0
		&& final.StartsWith("http"))
		url = final;

	BMessage ready(kMsgStreamReady);
	ready.AddString("id", message->GetString("id", ""));
	ready.AddString("url", url);
	fTarget.SendMessage(&ready);
}

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Worker.h"

#include <stdlib.h>
#include <time.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <MessageQueue.h>
#include <Path.h>
#include <StringList.h>

#include "Messages.h"
#include "Process.h"


static const int32 kResultCount = 24;
static const time_t kCacheTime = 6 * 60 * 60;
	// YouTube changes, but not that fast
static const char* kFormat = "18/b[height<=360][vcodec^=avc1]";
	// 640x360 H.264 and AAC in one MP4 file, that MediaPlayer plays as it is


/*!	The yt-dlp arguments common to every call: QuickJS solves YouTube's
	JavaScript challenges when they are needed.
*/
static std::vector<BString>
yt_dlp(std::initializer_list<BString> arguments)
{
	std::vector<BString> all = { "yt-dlp", "--no-warnings", "--no-js-runtimes",
		"--js-runtimes", "quickjs" };
	all.insert(all.end(), arguments);
	return all;
}


// YouTube's Android client gives the file without any JavaScript to run:
// about 20 s on the AC100, against nearly two minutes for the web client
// and its challenges in QuickJS. Its downloads are slowed down, but still
// faster than these videos play.
static const char* kAndroidClient = "youtube:player_client=android";


static BString
cache_name(const BString& query)
{
	// FNV-1a: a file name for the search
	uint32 hash = 2166136261u;
	for (int32 i = 0; i < query.Length(); i++)
		hash = (hash ^ (uint8)query.ByteAt(i)) * 16777619u;
	BString name;
	name.SetToFormat("search-%08" B_PRIx32 ".txt", hash);
	return name;
}


Worker::Worker(const char* name, BMessenger target)
	:
	BLooper(name),
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
		case kMsgSearch:
			_Search(message);
			break;
		case kMsgResolve:
			_Resolve(message);
			break;
		default:
			BLooper::MessageReceived(message);
	}
}


void
Worker::_Search(BMessage* message)
{
	BString query = message->GetString("query", "");
	int32 generation = message->GetInt32("generation", 0);
	if (query.IsEmpty() || _NewerSearchWaiting())
		return;

	BString results;
	if (!_RunSearch(query, results)) {
		BMessage failed(kMsgSearchFailed);
		failed.AddInt32("generation", generation);
		fTarget.SendMessage(&failed);
		return;
	}

	// one line per video: id, title and duration, separated by tabs
	BMessage reply(kMsgResults);
	reply.AddInt32("generation", generation);
	BStringList lines;
	results.Split("\n", true, lines);
	for (int32 i = 0; i < lines.CountStrings(); i++) {
		BStringList fields;
		lines.StringAt(i).Split("\t", false, fields);
		if (fields.CountStrings() < 3)
			continue;
		// a live stream has no duration, and no file to play
		int32 duration = atoi(fields.StringAt(2).String());
		if (duration <= 0)
			continue;
		reply.AddString("id", fields.StringAt(0));
		reply.AddString("title", fields.StringAt(1));
		reply.AddInt32("duration", duration);
	}
	fTarget.SendMessage(&reply);

	_Thumbnails(results, generation);
}


bool
Worker::_RunSearch(const BString& query, BString& results)
{
	BPath cached(fCache.String(), cache_name(query).String());
	BFile file(cached.Path(), B_READ_ONLY);
	time_t modified;
	if (file.InitCheck() == B_OK && file.GetModificationTime(&modified) == B_OK
		&& time(NULL) - modified < kCacheTime) {
		off_t size;
		if (file.GetSize(&size) == B_OK && size > 0 && size < 1024 * 1024) {
			char* buffer = results.LockBuffer(size);
			ssize_t bytes = file.Read(buffer, size);
			results.UnlockBuffer(bytes > 0 ? bytes : 0);
			if (bytes > 0)
				return true;
		}
	}
	file.Unset();

	BString search;
	search.SetToFormat("ytsearch%" B_PRId32 ":%s", kResultCount,
		query.String());
	results.Truncate(0);
	if (RunProgram(yt_dlp({ "--flat-playlist", "--print",
			"%(id)s\t%(title)s\t%(duration)s", search }), &results) != 0
		|| results.IsEmpty())
		return false;

	BFile out(cached.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	out.Write(results.String(), results.Length());
	return true;
}


/*!	The pictures, one by one, as long as no newer search is waiting. */
void
Worker::_Thumbnails(const BString& results, int32 generation)
{
	BStringList lines;
	results.Split("\n", true, lines);
	for (int32 i = 0; i < lines.CountStrings(); i++) {
		if (_NewerSearchWaiting())
			return;
		BString id = lines.StringAt(i);
		int32 tab = id.FindFirst('\t');
		if (tab <= 0)
			continue;
		id.Truncate(tab);

		BString path(fCache);
		path << "/" << id << ".jpg";
		if (!BEntry(path.String()).Exists()) {
			BString temporary(path);
			temporary << ".part";
			BString url;
			url.SetToFormat("https://i.ytimg.com/vi/%s/mqdefault.jpg",
				id.String());
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


void
Worker::_Resolve(BMessage* message)
{
	BString id = message->GetString("id", "");
	BString watch("https://www.youtube.com/watch?v=");
	watch << id;

	BString output;
	int status = RunProgram(yt_dlp({ "--extractor-args", kAndroidClient,
		"-f", kFormat, "-g", watch }), &output);
	if (status != 0 || !output.StartsWith("http")) {
		output.Truncate(0);
		status = RunProgram(yt_dlp({ "-f", kFormat, "-g", watch }), &output);
	}
	int32 end = output.FindFirst('\n');
	if (end >= 0)
		output.Truncate(end);

	if (status == 0 && output.StartsWith("http")) {
		// MediaPlayer's HTTP streaming does not follow redirects, and the
		// addresses of YouTube's files go through two of them
		BString final;
		if (RunProgram({ "curl", "-s", "-o", "/dev/null", "-L", "-r", "0-0",
				"--max-time", "20", "-w", "%{url_effective}", output },
				&final) == 0 && final.StartsWith("http"))
			output = final;
	}

	BMessage reply(status == 0 && output.StartsWith("http")
		? kMsgStreamReady : kMsgStreamFailed);
	reply.AddString("id", id);
	reply.AddString("title", message->GetString("title", ""));
	reply.AddString("url", output);
	fTarget.SendMessage(&reply);
}


bool
Worker::_NewerSearchWaiting()
{
	return MessageQueue()->FindMessage(kMsgSearch, 0) != NULL;
}

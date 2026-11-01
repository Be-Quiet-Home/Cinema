/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Helper.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <Autolock.h>

#include "Json.h"
#include "Messages.h"


Helper::Helper(BMessenger target)
	:
	fTarget(target),
	fLock("helper"),
	fProvider("youtube"),
	fInput(-1),
	fOutput(-1),
	fProcess(-1),
	fReader(-1)
{
}


Helper::~Helper()
{
	if (fInput >= 0)
		close(fInput);
	if (fProcess > 0) {
		kill(fProcess, SIGTERM);
		waitpid(fProcess, NULL, 0);
	}
	if (fReader >= 0) {
		status_t status;
		wait_for_thread(fReader, &status);
	}
	if (fOutput >= 0)
		close(fOutput);
}


status_t
Helper::Start(const char* script, const char* cache)
{
	int toHelper[2];
	int fromHelper[2];
	if (pipe(toHelper) != 0)
		return B_ERROR;
	if (pipe(fromHelper) != 0) {
		close(toHelper[0]);
		close(toHelper[1]);
		return B_ERROR;
	}

	fProcess = fork();
	if (fProcess < 0)
		return B_ERROR;
	if (fProcess == 0) {
		dup2(toHelper[0], STDIN_FILENO);
		dup2(fromHelper[1], STDOUT_FILENO);
		int null = open("/dev/null", O_WRONLY);
		if (null >= 0)
			dup2(null, STDERR_FILENO);
		close(toHelper[0]);
		close(toHelper[1]);
		close(fromHelper[0]);
		close(fromHelper[1]);
		execlp("python3", "python3", script, cache, (char*)NULL);
		_exit(127);
	}

	close(toHelper[0]);
	close(fromHelper[1]);
	fInput = toHelper[1];
	fOutput = fromHelper[0];

	fReader = spawn_thread(&_ReaderEntry, "cinema-helper reader",
		B_NORMAL_PRIORITY, this);
	if (fReader < 0)
		return fReader;
	return resume_thread(fReader);
}


void
Helper::SetProvider(const char* id)
{
	BAutolock locker(fLock);
	if (id != NULL && id[0] != '\0')
		fProvider = id;
}


BString
Helper::_Provider(const char* provider)
{
	if (provider != NULL && provider[0] != '\0')
		return provider;
	BAutolock locker(fLock);
	return fProvider;
}


void
Helper::Search(int32 generation, int32 count, const char* query,
	const char* provider)
{
	Json request = Json::MakeObject();
	request.Set("type", "search");
	request.Set("tag", generation);
	request.Set("provider", _Provider(provider).String());
	request.Set("count", count);
	request.Set("query", query);
	_Send(request);
}


void
Helper::Channel(int32 generation, int32 count, const char* id,
	const char* provider)
{
	Json request = Json::MakeObject();
	request.Set("type", "channel");
	request.Set("tag", generation);
	request.Set("provider", _Provider(provider).String());
	request.Set("count", count);
	request.Set("channel", id);
	_Send(request);
}


void
Helper::Details(const char* provider, const char* id, const char* kind)
{
	Json request = Json::MakeObject();
	request.Set("type", "details");
	request.Set("provider", _Provider(provider).String());
	request.Set("id", id);
	request.Set("kind", kind != NULL && kind[0] != '\0' ? kind : "video");
	_Send(request);
}


void
Helper::Find(const char* id, const char* provider)
{
	Json request = Json::MakeObject();
	request.Set("type", "find");
	request.Set("provider", _Provider(provider).String());
	request.Set("id", id);
	request.Set("kind", "video");
	_Send(request);
}


void
Helper::Download(const char* id, const char* folder, const char* provider)
{
	Json request = Json::MakeObject();
	request.Set("type", "download");
	request.Set("provider", _Provider(provider).String());
	request.Set("id", id);
	request.Set("folder", folder);
	_Send(request);
}


void
Helper::Cancel(const char* id)
{
	Json request = Json::MakeObject();
	request.Set("type", "cancel");
	request.Set("id", id);
	_Send(request);
}


void
Helper::_Send(const Json& request)
{
	std::string line = request.ToString();
	line += '\n';
	BAutolock locker(fLock);
	if (fInput >= 0)
		write(fInput, line.c_str(), line.size());
}


status_t
Helper::_ReaderEntry(void* data)
{
	((Helper*)data)->_Read();
	return B_OK;
}


void
Helper::_Read()
{
	BString pending;
	char buffer[4096];
	ssize_t bytes;
	while ((bytes = read(fOutput, buffer, sizeof(buffer))) > 0) {
		pending.Append(buffer, bytes);
		int32 end;
		while ((end = pending.FindFirst('\n')) >= 0) {
			BString line;
			pending.MoveInto(line, 0, end + 1);
			line.RemoveLast("\n");
			_Handle(line);
		}
	}
	// it is gone: nothing more will be found
	fTarget.SendMessage(kMsgHelperGone);
}


static std::string
text_of(const Json& value, const char* key, const char* fallback)
{
	return value.GetString(key, fallback);
}


/*!	The fields of one MediaItem, as the window reads them back. */
static void
add_item(BMessage& message, const Json& item, const char* fallbackProvider)
{
	message.AddString("provider",
		text_of(item, "provider", fallbackProvider).c_str());
	message.AddString("kind", text_of(item, "kind", "video").c_str());
	message.AddString("id", text_of(item, "id", "").c_str());
	message.AddString("title", text_of(item, "title", "").c_str());
	message.AddString("description",
		text_of(item, "description", "").c_str());
	message.AddString("channel", text_of(item, "channel", "").c_str());
	message.AddString("channel id",
		text_of(item, "channel_id", "").c_str());
	message.AddInt32("duration", item.GetInt("duration", 0));
	message.AddInt32("year", item.GetInt("year", 0));
	message.AddString("date", text_of(item, "date", "").c_str());
	message.AddInt64("views", (int64)item.GetDouble("views", 0));
	message.AddDouble("rating", item.GetDouble("rating", 0));
	message.AddString("poster", text_of(item, "poster", "").c_str());
	message.AddString("thumbnail", text_of(item, "thumbnail", "").c_str());
}


void
Helper::_Handle(const BString& line)
{
	bool ok = false;
	Json answer = Json::Parse(std::string(line.String()), &ok);
	if (!ok)
		return;

	BString type(answer.GetString("type", "").c_str());

	if (type == "providers") {
		BMessage message(kMsgProviders);
		const Json* providers = answer.Find("providers");
		if (providers != NULL && providers->IsArray()) {
			for (const Json& provider : providers->Elements()) {
				BMessage one;
				one.AddString("id", provider.GetString("id", "").c_str());
				one.AddString("name",
					provider.GetString("name", "").c_str());
				const Json* kinds = provider.Find("kinds");
				if (kinds != NULL && kinds->IsArray())
					for (const Json& kind : kinds->Elements())
						one.AddString("kinds", kind.String().c_str());
				message.AddMessage("provider", &one);
			}
		}
		fTarget.SendMessage(&message);
		return;
	}

	if (type == "results") {
		BMessage results(kMsgResults);
		results.AddInt32("generation", answer.GetInt("tag", -1));
		const Json* items = answer.Find("items");
		if (items != NULL && items->IsArray())
			for (const Json& item : items->Elements())
				add_item(results, item, fProvider.String());
		fTarget.SendMessage(&results);
		return;
	}

	if (type == "details") {
		BMessage details(kMsgDetails);
		details.AddString("id", answer.GetString("id", "").c_str());
		const Json* item = answer.Find("item");
		bool found = item != NULL && item->IsObject();
		details.AddBool("found", found);
		if (found)
			add_item(details, *item, fProvider.String());
		fTarget.SendMessage(&details);
		return;
	}

	if (type == "nothing") {
		BMessage failed(kMsgSearchFailed);
		failed.AddInt32("generation", answer.GetInt("tag", -1));
		fTarget.SendMessage(&failed);
		return;
	}

	if (type == "found") {
		BMessage found(kMsgFound);
		found.AddString("id", answer.GetString("id", "").c_str());
		found.AddString("url", answer.GetString("url", "").c_str());
		fTarget.SendMessage(&found);
		return;
	}

	if (type == "lost") {
		BMessage lost(kMsgLost);
		lost.AddString("id", answer.GetString("id", "").c_str());
		fTarget.SendMessage(&lost);
		return;
	}

	if (type == "progress") {
		BMessage progress(kMsgDownloadProgress);
		progress.AddString("id", answer.GetString("id", "").c_str());
		progress.AddInt32("percent", answer.GetInt("percent", 0));
		for (const char* field : { "done", "total", "speed", "eta" })
			progress.AddString(field, answer.GetString(field, "").c_str());
		progress.AddInt32("peers", answer.GetInt("peers", -1));
		fTarget.SendMessage(&progress);
		return;
	}

	if (type == "cancelled") {
		BMessage cancelled(kMsgDownloadCancelled);
		cancelled.AddString("id", answer.GetString("id", "").c_str());
		fTarget.SendMessage(&cancelled);
		return;
	}

	if (type == "saved") {
		BMessage saved(kMsgDownloaded);
		saved.AddString("id", answer.GetString("id", "").c_str());
		saved.AddString("path", answer.GetString("path", "").c_str());
		fTarget.SendMessage(&saved);
		return;
	}

	if (type == "failed") {
		BMessage failed(kMsgDownloadFailed);
		failed.AddString("id", answer.GetString("id", "").c_str());
		fTarget.SendMessage(&failed);
	}
}

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Helper.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <Autolock.h>
#include <StringList.h>

#include "Messages.h"


Helper::Helper(BMessenger target)
	:
	fTarget(target),
	fLock("helper"),
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
Helper::Search(int32 generation, int32 count, const char* query)
{
	BString line;
	line.SetToFormat("search\t%" B_PRId32 "\t%" B_PRId32 "\t%s", generation,
		count, query);
	line.ReplaceAll("\n", " ");
	_Send(line);
}


void
Helper::Channel(int32 generation, int32 count, const char* id)
{
	BString line;
	line.SetToFormat("channel\t%" B_PRId32 "\t%" B_PRId32 "\t%s", generation,
		count, id);
	_Send(line);
}


void
Helper::Find(const char* id)
{
	BString line("find\t");
	line << id;
	_Send(line);
}


void
Helper::Download(const char* id, const char* folder)
{
	BString line("download\t");
	line << id << "\t" << folder;
	_Send(line);
}


void
Helper::_Send(const BString& line)
{
	BString text(line);
	text << "\n";
	BAutolock locker(fLock);
	if (fInput >= 0)
		write(fInput, text.String(), text.Length());
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


void
Helper::_Handle(const BString& line)
{
	BStringList fields;
	line.Split("\t", false, fields);
	BString kind = fields.StringAt(0);

	if (kind == "video" && fields.CountStrings() >= 5) {
		int32 generation = atoi(fields.StringAt(1).String());
		BMessage& results = fSearches[generation];
		results.AddString("id", fields.StringAt(2));
		results.AddString("title", fields.StringAt(3));
		results.AddInt32("duration", atoi(fields.StringAt(4).String()));
		results.AddString("channel", fields.StringAt(5));
		results.AddString("channel id", fields.StringAt(6));
	} else if ((kind == "done" || kind == "nothing")
		&& fields.CountStrings() >= 2) {
		int32 generation = atoi(fields.StringAt(1).String());
		BMessage results = fSearches[generation];
		fSearches.erase(generation);
		results.what = kind == "done" ? kMsgResults : kMsgSearchFailed;
		results.AddInt32("generation", generation);
		fTarget.SendMessage(&results);
	} else if (kind == "found" && fields.CountStrings() >= 3) {
		BMessage found(kMsgFound);
		found.AddString("id", fields.StringAt(1));
		found.AddString("url", fields.StringAt(2));
		fTarget.SendMessage(&found);
	} else if (kind == "progress" && fields.CountStrings() >= 3) {
		BMessage progress(kMsgDownloadProgress);
		progress.AddString("id", fields.StringAt(1));
		progress.AddInt32("percent", atoi(fields.StringAt(2).String()));
		fTarget.SendMessage(&progress);
	} else if (kind == "saved" && fields.CountStrings() >= 3) {
		BMessage saved(kMsgDownloaded);
		saved.AddString("id", fields.StringAt(1));
		saved.AddString("path", fields.StringAt(2));
		fTarget.SendMessage(&saved);
	} else if (kind == "failed" && fields.CountStrings() >= 2) {
		BMessage failed(kMsgDownloadFailed);
		failed.AddString("id", fields.StringAt(1));
		fTarget.SendMessage(&failed);
	} else if (kind == "lost" && fields.CountStrings() >= 2) {
		BMessage lost(kMsgLost);
		lost.AddString("id", fields.StringAt(1));
		fTarget.SendMessage(&lost);
	}
}

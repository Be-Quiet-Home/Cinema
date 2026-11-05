/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Watchdog.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <OS.h>
#include <String.h>

#include <atomic>


namespace {

std::atomic<uint32>		sWhat(0);
std::atomic<bigtime_t>	sSince(0);
std::atomic<thread_id>	sWindow(-1);
std::atomic<bool>		sStop(false);
std::atomic<bigtime_t>	sAlive(0);
BMessenger				sWindow2;
thread_id				sThread = -1;
BString					sPath;

const bigtime_t kHeld = 3000000;
const off_t kMostReports = 256 * 1024;


BString
four_letters(uint32 what)
{
	BString text;
	for (int shift = 24; shift >= 0; shift -= 8) {
		char c = (char)((what >> shift) & 0xff);
		text << (c >= 32 && c < 127 ? c : '.');
	}
	text << " (0x";
	char hex[16];
	snprintf(hex, sizeof(hex), "%08" B_PRIx32, what);
	text << hex << ")";
	return text;
}


const char*
state_name(thread_state state)
{
	switch (state) {
		case B_THREAD_RUNNING:		return "running";
		case B_THREAD_READY:		return "ready";
		case B_THREAD_RECEIVING:	return "receiving";
		case B_THREAD_ASLEEP:		return "asleep";
		case B_THREAD_SUSPENDED:	return "suspended";
		case B_THREAD_WAITING:		return "waiting";
		default:					return "?";
	}
}


void
report(uint32 what, bigtime_t held, bool pings = false)
{
	BString text;
	time_t now = time(NULL);
	char date[64];
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));
	if (pings) {
		text << "=== " << date << ": the window took no message for "
			<< held / 1000 << " ms";
		if (what != 0)
			text << ", within " << four_letters(what).String();
		text << "\n";
	} else {
		text << "=== " << date << ": the window held " << held / 1000
			<< " ms by message " << four_letters(what).String() << "\n";
	}

	thread_id window = sWindow;
	team_id team = getpid();
	int32 cookie = 0;
	thread_info info;
	while (get_next_thread_info(team, &cookie, &info) == B_OK) {
		char line[256];
		snprintf(line, sizeof(line), "%s%6" B_PRId32 " %-32s %-9s user %"
			B_PRId64 " ms kernel %" B_PRId64 " ms",
			info.thread == window ? "* " : "  ", info.thread, info.name,
			state_name(info.state), info.user_time / 1000,
			info.kernel_time / 1000);
		text << line;
		if (info.sem >= 0) {
			sem_info sem;
			if (get_sem_info(info.sem, &sem) == B_OK) {
				snprintf(line, sizeof(line), " on sem %" B_PRId32 " \"%s\""
					" (count %" B_PRId32 ", held by %" B_PRId32 ")",
					info.sem, sem.name, sem.count, sem.latest_holder);
				text << line;
			} else
				text << " on sem " << info.sem;
		}
		text << "\n";
	}

	fputs(text.String(), stderr);
	if (sPath.IsEmpty())
		return;
	// the latest reports only
	struct stat status;
	const char* mode = stat(sPath.String(), &status) == 0
		&& status.st_size > kMostReports ? "w" : "a";
	FILE* file = fopen(sPath.String(), mode);
	if (file != NULL) {
		fputs(text.String(), file);
		fclose(file);
	}
}


status_t
watch(void*)
{
	bigtime_t reported = 0;
	bigtime_t silentReported = 0;
	bigtime_t lastPing = 0;
	sAlive = system_time();
	while (!sStop) {
		snooze(500000);
		bigtime_t now = system_time();
		if (now - lastPing >= 1000000) {
			// without waiting: a full queue is a sign too
			BMessage ping(kMsgWatchdogPing);
			sWindow2.SendMessage(&ping, (BHandler*)NULL, 0);
			lastPing = now;
		}
		uint32 what = sWhat;
		bigtime_t since = sSince;
		if (what != 0 && since != 0 && since != reported
			&& now - since >= kHeld) {
			reported = since;
			report(what, now - since);
			continue;
		}
		bigtime_t alive = sAlive;
		if (now - alive >= 4000000 && alive != silentReported) {
			silentReported = alive;
			report(what, now - alive, true);
		}
	}
	return B_OK;
}

}


void
Watchdog::Start(const char* folder, const BMessenger& window)
{
	if (sThread >= 0)
		return;
	sWindow2 = window;
	if (folder != NULL && folder[0] != '\0') {
		sPath = folder;
		sPath << "/freeze-reports.txt";
	}
	sStop = false;
	sThread = spawn_thread(&watch, "watchdog", B_LOW_PRIORITY, NULL);
	if (sThread >= 0)
		resume_thread(sThread);
}


void
Watchdog::Stop()
{
	if (sThread < 0)
		return;
	sStop = true;
	status_t status;
	wait_for_thread(sThread, &status);
	sThread = -1;
}


void
Watchdog::Enter(uint32 what)
{
	sWindow = find_thread(NULL);
	bigtime_t now = system_time();
	sAlive = now;
	sSince = now;
	sWhat = what;
}


void
Watchdog::Held(uint32* what, bigtime_t* since)
{
	*what = sWhat;
	*since = sSince;
}


void
Watchdog::Restore(uint32 what, bigtime_t since)
{
	sWhat = 0;
	sSince = since;
	sWhat = what;
}


void
Watchdog::Leave()
{
	sWhat = 0;
	sSince = 0;
}

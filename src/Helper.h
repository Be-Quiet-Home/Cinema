/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef HELPER_H
#define HELPER_H


#include <Locker.h>
#include <Messenger.h>
#include <String.h>


class Json;


/*!	cinema-helper, the Python program next to Cinema that keeps the providers
	loaded: today yt-dlp. It searches and finds the files of the videos, and
	talks one JSON object per line. Its answers are sent to the target as
	kMsgProviders, kMsgResults or kMsgSearchFailed, kMsgFound or kMsgLost, and
	kMsgDownloadProgress then kMsgDownloaded or kMsgDownloadFailed.
*/
class Helper {
public:
								Helper(BMessenger target);
								~Helper();

			status_t			Start(const char* script, const char* cache);
			void				SetProvider(const char* id);
									// the one the next requests are for
			void				Search(int32 generation, int32 count,
									const char* query);
			void				Channel(int32 generation, int32 count,
									const char* id);
			void				Find(const char* id);
			void				Download(const char* id, const char* folder);

private:
	static	status_t			_ReaderEntry(void* data);
			void				_Read();
			void				_Handle(const BString& line);
			void				_Send(const Json& request);

			BMessenger			fTarget;
			BLocker				fLock;
			BString				fProvider;	// "youtube" until it says
			int					fInput;		// its standard input
			int					fOutput;	// its standard output
			pid_t				fProcess;
			thread_id			fReader;
};


#endif	// HELPER_H

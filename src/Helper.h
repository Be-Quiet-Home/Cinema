/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef HELPER_H
#define HELPER_H


#include <Locker.h>
#include <Messenger.h>
#include <String.h>

#include <map>


/*!	cinema-helper, the Python program next to Cinema that keeps yt-dlp loaded:
	it searches and finds the files of the videos. Its answers are sent to the
	target as kMsgResults or kMsgSearchFailed, and kMsgFound or kMsgLost.
*/
class Helper {
public:
								Helper(BMessenger target);
								~Helper();

			status_t			Start(const char* script, const char* cache);
			void				Search(int32 generation, int32 count,
									const char* query);
			void				Find(const char* id);

private:
	static	status_t			_ReaderEntry(void* data);
			void				_Read();
			void				_Handle(const BString& line);
			void				_Send(const BString& line);

			BMessenger			fTarget;
			BLocker				fLock;
			int					fInput;		// its standard input
			int					fOutput;	// its standard output
			pid_t				fProcess;
			thread_id			fReader;
			std::map<int32, BMessage> fSearches;	// by generation
};


#endif	// HELPER_H

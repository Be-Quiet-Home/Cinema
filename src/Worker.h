/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef WORKER_H
#define WORKER_H


#include <Looper.h>
#include <Messenger.h>
#include <String.h>


/*!	Does the slow work on its own thread, with yt-dlp and curl: searches
	(kMsgSearch), and the pictures of the videos found, or finds the address
	of the video file to play (kMsgResolve). The answers go to the target.
	Searches are kept for a few hours in the cache folder.
*/
class Worker : public BLooper {
public:
								Worker(const char* name, BMessenger target);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Search(BMessage* message);
			bool				_RunSearch(const BString& query,
									BString& results);
			void				_Thumbnails(const BString& results,
									int32 generation);
			void				_Resolve(BMessage* message);
			bool				_NewerSearchWaiting();

			BMessenger			fTarget;
			BString				fCache;
};


#endif	// WORKER_H

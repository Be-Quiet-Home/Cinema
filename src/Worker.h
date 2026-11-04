/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef WORKER_H
#define WORKER_H


#include <Locker.h>
#include <Looper.h>
#include <Messenger.h>
#include <String.h>

#include <atomic>


/*!	Does the network work that is not yt-dlp's on its own thread, with curl:
	gets the pictures of the videos (kMsgThumbnails), the big picture of one
	of them for the detail view (kMsgFetchPicture), and follows the redirects
	of the address of a video file (kMsgFollow), as MediaPlayer does not. The
	answers go to the target.
*/
class Worker : public BLooper {
public:
								Worker(BMessenger target);

	virtual	void				MessageReceived(BMessage* message);

			const char*			Cache() const { return fCache.String(); }
			void				Stop();
									// before the application goes: once
									// it is gone, a picture made here (a
									// BBitmap) calls the debugger and the
									// team stays stopped. Waits for the
									// picture being made, not for curl.

private:
			void				_Thumbnails(BMessage* message);
			void				_Picture(BMessage* message);
			void				_Follow(BMessage* message);
			bool				_Get(const char* url, const char* path);

			BMessenger			fTarget;
			BString				fCache;
			std::atomic<bool>	fStopping;
			BLocker				fPictureLock;
};


#endif	// WORKER_H

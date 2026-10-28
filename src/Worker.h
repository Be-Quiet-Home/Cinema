/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef WORKER_H
#define WORKER_H


#include <Looper.h>
#include <Messenger.h>
#include <String.h>


/*!	Does the network work that is not yt-dlp's on its own thread, with curl:
	gets the pictures of the videos (kMsgThumbnails), and follows the
	redirects of the address of a video file (kMsgFollow), as MediaPlayer
	does not. The answers go to the target.
*/
class Worker : public BLooper {
public:
								Worker(BMessenger target);

	virtual	void				MessageReceived(BMessage* message);

			const char*			Cache() const { return fCache.String(); }

private:
			void				_Thumbnails(BMessage* message);
			void				_Follow(BMessage* message);

			BMessenger			fTarget;
			BString				fCache;
};


#endif	// WORKER_H

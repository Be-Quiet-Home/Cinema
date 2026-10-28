/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include <ctype.h>
#include <string.h>

#include <Application.h>
#include <String.h>

#include "CinemaWindow.h"
#include "Messages.h"


static const char* kSignature = "application/x-vnd.atomozero-Cinema";


class App : public BApplication {
public:
	App()
		:
		BApplication(kSignature)
	{
	}

	virtual void ArgvReceived(int32 argc, char** argv)
	{
		// a YouTube address: play it right away
		for (int32 i = 1; i < argc; i++) {
			BString id = _VideoId(argv[i]);
			if (id.IsEmpty())
				continue;
			BMessage play(kMsgPlay);
			play.AddString("id", id);
			play.AddString("title", argv[i]);
			_Window()->PostMessage(&play);
		}
	}

	virtual void ReadyToRun()
	{
		_Window()->Show();
	}

private:
	CinemaWindow* _Window()
	{
		if (fWindow == NULL)
			fWindow = new CinemaWindow;
		return fWindow;
	}

	/*!	The id of the video in youtube.com/watch?v=ID, youtu.be/ID,
		youtube.com/shorts/ID or youtube.com/embed/ID, or nothing.
	*/
	static BString _VideoId(const char* address)
	{
		BString url(address);
		static const char* kMarkers[] = { "watch?v=", "&v=", "youtu.be/",
			"/shorts/", "/embed/", "/live/" };
		if (url.IFindFirst("youtube.com") < 0 && url.IFindFirst("youtu.be") < 0)
			return BString();
		for (const char* marker : kMarkers) {
			int32 start = url.FindFirst(marker);
			if (start < 0)
				continue;
			start += strlen(marker);
			int32 end = start;
			while (end < url.Length() && (isalnum(url.ByteAt(end))
					|| url.ByteAt(end) == '-' || url.ByteAt(end) == '_'))
				end++;
			BString id;
			url.CopyInto(id, start, end - start);
			if (id.Length() == 11)
				return id;
		}
		return BString();
	}

	CinemaWindow*	fWindow = NULL;
};


int
main()
{
	App app;
	app.Run();
	return 0;
}

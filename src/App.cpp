/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include <Application.h>
#include <String.h>

#include "CinemaWindow.h"
#include "Messages.h"
#include "Provider.h"


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
			BString id = YouTubeVideoId(argv[i]);
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

	CinemaWindow*	fWindow = NULL;
};


int
main()
{
	App app;
	app.Run();
	return 0;
}

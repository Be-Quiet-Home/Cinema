/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include <Application.h>

#include "CinemaWindow.h"


static const char* kSignature = "application/x-vnd.atomozero-Cinema";


class App : public BApplication {
public:
	App()
		:
		BApplication(kSignature)
	{
	}

	virtual void ReadyToRun()
	{
		(new CinemaWindow)->Show();
	}
};


int
main()
{
	App app;
	app.Run();
	return 0;
}

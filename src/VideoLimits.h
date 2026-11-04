/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef VIDEO_LIMITS_H
#define VIDEO_LIMITS_H


#include <Screen.h>
#include <SupportDefs.h>

#include <stdlib.h>


static inline int32
VideoHeightLimit()
{
	int32 limit = 1080;
#if defined(__arm__) && !defined(__aarch64__)
	limit = 480;
#endif
	BScreen screen(B_MAIN_SCREEN_ID);
	if (screen.IsValid()) {
		int32 screenHeight = (int32)screen.Frame().Height() + 1;
		if (screenHeight > 0 && screenHeight < limit)
			limit = screenHeight;
	}
	const char* wanted = getenv("CINEMA_MAX_HEIGHT");
	if (wanted != NULL) {
		int32 height = atoi(wanted);
		if (height > 0 && height < limit)
			limit = height;
	}
	return limit;
}


static inline int32
LimitVideoHeight(int32 height)
{
	if (height <= 0)
		return height;
	int32 limit = VideoHeightLimit();
	return height < limit ? height : limit;
}


#endif	// VIDEO_LIMITS_H

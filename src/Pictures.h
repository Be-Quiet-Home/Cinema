/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PICTURES_H
#define PICTURES_H


class BBitmap;


namespace Pictures {

// the largest a picture is shown, in pixels before Style::S()
static const float kShelf = 240;	// the shelves of the home, 210 wide
static const float kTile = 400;		// the grid, the downloads
static const float kDetail = 640;	// the page of a video

BBitmap*	Read(const char* path, float largest = kTile);
						// the picture of the file, made smaller when it is
						// bigger than largest (times Style::Scale()) on a
						// side: a channel logo of 960x960 took 3.5 MB of
						// app_server memory for a tile of 200 pixels. A
						// bitmap without a link to app_server, so any
						// thread can read one: the worker does. NULL when
						// the file is not a picture.
BBitmap*	ForScreen(BBitmap* read);
						// a picture of Read() as a bitmap app_server
						// keeps, to be drawn; deletes read. In the thread
						// of a window only: bitmaps of app_server made by
						// two threads at once broke the worker's.
BBitmap*	Load(const char* path, float largest = kTile);
						// ForScreen(Read()), for a window

}	// namespace Pictures


#endif	// PICTURES_H

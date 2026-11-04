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

BBitmap*	Load(const char* path, float largest = kTile);
						// the picture of the file, made smaller when it is
						// bigger than largest (times Style::Scale()) on a
						// side: a channel logo of 960x960 took 3.5 MB of
						// app_server memory for a tile of 200 pixels. NULL
						// when the file is not a picture.

}	// namespace Pictures


#endif	// PICTURES_H

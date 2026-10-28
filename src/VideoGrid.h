/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef VIDEO_GRID_H
#define VIDEO_GRID_H


#include <String.h>
#include <View.h>

#include <vector>


class BBitmap;
class BMessageRunner;


struct Video {
	BString		id;
	BString		title;
	int32		duration;	// in seconds
	BBitmap*	picture;	// may be NULL
};


/*!	The videos found, as big pictures in rows. A click, or Enter on the chosen
	one, sends kMsgPlay to the window. It can also show a message instead
	(while searching, when nothing was found), and a "getting it ready" banner
	over the pictures.
*/
class VideoGrid : public BView {
public:
								VideoGrid();
	virtual						~VideoGrid();

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MessageReceived(BMessage* message);

			void				SetVideos(const std::vector<Video>& videos);
			void				SetPicture(const char* id, BBitmap* picture);
			void				SetMessage(const char* message);
			void				SetBusy(const char* title);
									// NULL when no longer busy

private:
			void				_Clear();
			int32				_Columns() const;
			BRect				_TileFrame(int32 index) const;
			int32				_TileAt(BPoint where) const;
			void				_UpdateScrollBar();
			void				_Select(int32 index);
			void				_Play(int32 index);
			void				_DrawTile(int32 index, BRect frame);
			void				_DrawBusy();

			std::vector<Video>	fVideos;
			BString				fMessage;
			BString				fBusy;
			int32				fSelected;
			int32				fHover;
			int32				fDots;
			BMessageRunner*		fAnimation;
};


#endif	// VIDEO_GRID_H

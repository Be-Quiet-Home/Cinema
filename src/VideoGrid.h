/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef VIDEO_GRID_H
#define VIDEO_GRID_H


#include <Message.h>
#include <String.h>
#include <View.h>

#include <vector>

#include "MediaItem.h"


class BBitmap;
class BMessageRunner;


/*!	The videos found, as big pictures in rows. A click, or Enter on the chosen
	one, sends kMsgPlay to the window; a click on the name of its channel
	sends kMsgChannel. D, or the menu of a right click, sends kMsgDownload. It can also show a message instead
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
	virtual	void				MakeFocus(bool focus = true);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MessageReceived(BMessage* message);

			void				SetVideos(const std::vector<MediaItem>& videos);
			void				SetShelfMode(float tileWidth);
			float				ShelfHeight() const;
			static float			ShelfHeightFor(float tileWidth);
			bool				HasVideo(const char* id) const;
			bool				IsEmpty() const { return fVideos.empty(); }
			void				SetPicture(const char* id, BBitmap* picture);
			void				SetProgress(const char* id, int32 progress);
			void				SetStatus(const char* id, const char* status);
									// -1: not downloading
			void				SetDownloaded(const char* id,
									bool downloaded);
			void				SetMessage(const char* message);
			void				SetBusy(const char* title);
									// NULL when no longer busy

private:
			void				_Clear();
			int32				_Columns() const;
			int32				_VisibleCount() const;
			BRect				_TileFrame(int32 index) const;
			int32				_TileAt(BPoint where) const;
			BRect				_ChannelFrame(int32 index) const;
			int32				_ChannelAt(BPoint where) const;
			void				_UpdateScrollBar();
			void				_Select(int32 index);
			void				_Play(int32 index);
			void				_Download(int32 index);
			void				_ShowDetails(int32 index);
			BMessage			_DownloadMessage(int32 index) const;
			BMessage			_DetailsMessage(int32 index) const;
			void				_ShowMenu(int32 index, BPoint where);
			void				_DrawTile(int32 index, BRect frame);
			void				_DrawBusy();

			std::vector<MediaItem>	fVideos;
			BString				fMessage;
			BString				fBusy;
			int32				fSelected;
			int32				fHover;
			int32				fHoverChannel;
			int32				fDots;
			bool				fShelf;
			float				fShelfTileWidth;
			BMessageRunner*		fAnimation;
};


#endif	// VIDEO_GRID_H

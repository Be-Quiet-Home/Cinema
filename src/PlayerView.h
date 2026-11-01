/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PLAYER_VIEW_H
#define PLAYER_VIEW_H


#include <String.h>
#include <View.h>


class BMessageRunner;
class Player;


/*!	The picture of the player, fitted to the view, and its controls over
	it: the title at the top, a bar with the time and where it is at the
	bottom. They show with the mouse or a key, and hide while it plays.

	Space or Enter pauses, the arrows go back and on by 10 seconds (a minute
	with Shift) and change the volume, M plays it in MediaPlayer instead; a
	click on the bar goes there, a click elsewhere pauses. The window closes
	it, with Esc.
*/
class PlayerView : public BView {
public:
								PlayerView(Player* player);
	virtual						~PlayerView();

	virtual	void				AttachedToWindow();
	virtual	void				DetachedFromWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MessageReceived(BMessage* message);

			void				Start(const char* title);
			void				SetOpened(bool opened);

private:
			BRect				_PictureFrame(const BBitmap* picture) const;
			BRect				_BarFrame() const;
			BRect				_TrackFrame() const;
			void				_ShowControls();
			void				_TogglePause();
			void				_SeekBy(bigtime_t delta);
			void				_DrawControls();

			Player*				fPlayer;
			BString				fTitle;
			bool				fOpened;
			bool				fControls;
			bigtime_t			fControlsUntil;
			BPoint				fLastMouse;
			BMessageRunner*		fTicker;
};


#endif	// PLAYER_VIEW_H

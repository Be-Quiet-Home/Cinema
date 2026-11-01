/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "PlayerView.h"

#include <algorithm>

#include <Bitmap.h>
#include <Catalog.h>
#include <MessageRunner.h>
#include <Window.h>

#include "Messages.h"
#include "Player.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Player"


using namespace Style;


static const bigtime_t kControlsTime = 3000000;
static const bigtime_t kStep = 10000000;
static const bigtime_t kBigStep = 60000000;
static const float kBarHeight = 56;
static const float kTitleHeight = 44;


static BString
time_text(bigtime_t time)
{
	int32 seconds = (int32)(std::max((bigtime_t)0, time) / 1000000);
	BString text;
	if (seconds >= 3600) {
		text.SetToFormat("%" B_PRId32 ":%02" B_PRId32 ":%02" B_PRId32,
			seconds / 3600, seconds / 60 % 60, seconds % 60);
	} else {
		text.SetToFormat("%" B_PRId32 ":%02" B_PRId32, seconds / 60,
			seconds % 60);
	}
	return text;
}


PlayerView::PlayerView(Player* player)
	:
	BView("player", B_WILL_DRAW | B_NAVIGABLE | B_FULL_UPDATE_ON_RESIZE),
	fPlayer(player),
	fOpened(false),
	fControls(true),
	fControlsUntil(0),
	fLastMouse(-1, -1),
	fTicker(NULL)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


PlayerView::~PlayerView()
{
	delete fTicker;
}


void
PlayerView::AttachedToWindow()
{
	BView::AttachedToWindow();
	delete fTicker;
	BMessage tick(kMsgPlayerTick);
	fTicker = new BMessageRunner(BMessenger(this), &tick, 500000);
}


void
PlayerView::DetachedFromWindow()
{
	delete fTicker;
	fTicker = NULL;
	BView::DetachedFromWindow();
}


void
PlayerView::Start(const char* title)
{
	fTitle = title;
	fOpened = false;
	_ShowControls();
	Invalidate();
}


void
PlayerView::SetOpened(bool opened)
{
	fOpened = opened;
	_ShowControls();
	Invalidate();
}


BRect
PlayerView::_PictureFrame(const BBitmap* picture) const
{
	BRect bounds = Bounds();
	BRect source = picture->Bounds();
	float scale = std::min((bounds.Width() + 1) / (source.Width() + 1),
		(bounds.Height() + 1) / (source.Height() + 1));
	float width = (source.Width() + 1) * scale;
	float height = (source.Height() + 1) * scale;
	BRect frame(0, 0, width - 1, height - 1);
	frame.OffsetTo(bounds.left + roundf((bounds.Width() + 1 - width) / 2),
		bounds.top + roundf((bounds.Height() + 1 - height) / 2));
	return frame;
}


BRect
PlayerView::_BarFrame() const
{
	BRect bounds = Bounds();
	return BRect(bounds.left, bounds.bottom - kBarHeight + 1, bounds.right,
		bounds.bottom);
}


BRect
PlayerView::_TrackFrame() const
{
	// where the time goes, between the button and the times
	BRect bar = _BarFrame();
	return BRect(bar.left + 56, bar.top + 22, bar.right - 140, bar.top + 30);
}


void
PlayerView::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(0, 0, 0);

	BBitmap* picture = fPlayer->LockFrame();
	if (picture != NULL) {
		BRect frame = _PictureFrame(picture);
		// the black around the picture only
		BRect bounds = Bounds();
		FillRect(BRect(bounds.left, bounds.top, bounds.right, frame.top - 1));
		FillRect(BRect(bounds.left, frame.bottom + 1, bounds.right,
			bounds.bottom));
		FillRect(BRect(bounds.left, frame.top, frame.left - 1, frame.bottom));
		FillRect(BRect(frame.right + 1, frame.top, bounds.right,
			frame.bottom));
		DrawBitmap(picture, picture->Bounds(), frame,
			B_FILTER_BITMAP_BILINEAR);
	} else
		FillRect(Bounds());
	fPlayer->UnlockFrame();

	if (picture == NULL) {
		DrawCentered(this, B_TRANSLATE("Getting the video ready"), Bounds(),
			Font(18), kTextDim);
	}
	if (fControls || fPlayer->IsPaused() || picture == NULL)
		_DrawControls();
}


void
PlayerView::_DrawControls()
{
	BRect bounds = Bounds();

	// the title, at the top
	BRect top(bounds.left, bounds.top, bounds.right,
		bounds.top + kTitleHeight - 1);
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	SetHighColor(0, 0, 0, 170);
	FillRect(top);
	BString title(fTitle);
	BFont font = Font(16, true);
	font.TruncateString(&title, B_TRUNCATE_END, bounds.Width() - 32);
	font_height height;
	font.GetHeight(&height);
	DrawText(this, title.String(), BPoint(top.left + 16,
		top.top + (kTitleHeight + height.ascent - height.descent) / 2),
		font, kText);

	// the bar: play or pause, where it is, the times
	BRect bar = _BarFrame();
	SetDrawingMode(B_OP_ALPHA);
	SetHighColor(0, 0, 0, 170);
	FillRect(bar);

	BRect button(bar.left + 18, bar.top + 16, bar.left + 38, bar.top + 36);
	SetHighColor(kText);
	if (fPlayer->IsPaused())
		DrawPlay(this, button, kText);
	else {
		FillRect(BRect(button.left + 2, button.top, button.left + 7,
			button.bottom));
		FillRect(BRect(button.right - 7, button.top, button.right - 2,
			button.bottom));
	}

	bigtime_t position = fPlayer->Position();
	bigtime_t duration = fPlayer->Duration();
	BRect track = _TrackFrame();
	FillRound(this, track, 4, (rgb_color){ 255, 255, 255, 60 });
	if (duration > 0) {
		BRect done = track;
		done.right = track.left + track.Width()
			* std::min(1.0, (double)position / duration);
		if (done.right > done.left)
			FillRound(this, done, 4, kAccent);
	}

	BString times = time_text(position);
	if (duration > 0)
		times << " / " << time_text(duration);
	BFont small = Font(14);
	small.GetHeight(&height);
	DrawText(this, times.String(), BPoint(track.right + 16,
		track.top + (track.Height() + height.ascent - height.descent) / 2),
		small, kText);
}


void
PlayerView::_ShowControls()
{
	fControls = true;
	fControlsUntil = system_time() + kControlsTime;
	Invalidate();
}


void
PlayerView::_TogglePause()
{
	fPlayer->SetPaused(!fPlayer->IsPaused());
	_ShowControls();
}


void
PlayerView::_SeekBy(bigtime_t delta)
{
	fPlayer->Seek(fPlayer->Position() + delta);
	_ShowControls();
}


void
PlayerView::KeyDown(const char* bytes, int32 count)
{
	int32 modifiers = 0;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("modifiers", &modifiers);
	bigtime_t step = (modifiers & B_SHIFT_KEY) != 0 ? kBigStep : kStep;

	switch (bytes[0]) {
		case B_SPACE:
		case B_ENTER:
			_TogglePause();
			break;
		case B_LEFT_ARROW:
			_SeekBy(-step);
			break;
		case B_RIGHT_ARROW:
			_SeekBy(step);
			break;
		case B_UP_ARROW:
			fPlayer->SetVolume(fPlayer->Volume() + 0.1f);
			_ShowControls();
			break;
		case B_DOWN_ARROW:
			fPlayer->SetVolume(fPlayer->Volume() - 0.1f);
			_ShowControls();
			break;
		case 'f':
		case 'F':
			Window()->PostMessage(kMsgFullScreen);
			break;
		case 'm':
		case 'M':
			Window()->PostMessage(kMsgPlayInMediaPlayer);
			break;
		default:
			BView::KeyDown(bytes, count);
	}
}


void
PlayerView::MouseDown(BPoint where)
{
	MakeFocus(true);
	int32 clicks = 1;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("clicks", &clicks);

	BRect track = _TrackFrame().InsetByCopy(0, -10);
	if ((fControls || fPlayer->IsPaused()) && track.Contains(where)
		&& fPlayer->Duration() > 0) {
		double fraction = (where.x - track.left) / track.Width();
		fPlayer->Seek((bigtime_t)(fPlayer->Duration()
			* std::max(0.0, std::min(1.0, fraction))));
		_ShowControls();
		return;
	}
	if (clicks >= 2) {
		Window()->PostMessage(kMsgFullScreen);
		// the first click paused it
		_TogglePause();
		return;
	}
	_TogglePause();
}


void
PlayerView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (where != fLastMouse) {
		fLastMouse = where;
		_ShowControls();
	}
}


void
PlayerView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPlayerFrame:
			Invalidate();
			break;
		case kMsgPlayerTick:
			// the time goes on, and the controls hide while it plays
			if (fControls && !fPlayer->IsPaused()
				&& system_time() > fControlsUntil && fOpened) {
				// once, to take them away
				fControls = false;
				Invalidate();
			} else if (fControls || fPlayer->IsPaused())
				Invalidate(_BarFrame());
			break;
		default:
			BView::MessageReceived(message);
	}
}

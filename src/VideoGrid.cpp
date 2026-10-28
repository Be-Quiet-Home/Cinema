/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "VideoGrid.h"

#include <algorithm>

#include <Bitmap.h>
#include <Catalog.h>
#include <MessageRunner.h>
#include <ScrollBar.h>
#include <Window.h>

#include "Messages.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Grid"


using namespace Style;


static const float kMinTileWidth = 280;
static const float kGap = 24;
static const float kTitleHeight = 58;


VideoGrid::VideoGrid()
	:
	BView("videos", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE
		| B_FULL_UPDATE_ON_RESIZE),
	fSelected(0),
	fHover(-1),
	fDots(0),
	fAnimation(NULL)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


VideoGrid::~VideoGrid()
{
	delete fAnimation;
	_Clear();
}


void
VideoGrid::AttachedToWindow()
{
	BView::AttachedToWindow();
	MakeFocus(true);
	_UpdateScrollBar();
}


void
VideoGrid::_Clear()
{
	for (Video& video : fVideos)
		delete video.picture;
	fVideos.clear();
}


void
VideoGrid::SetVideos(const std::vector<Video>& videos)
{
	_Clear();
	fVideos = videos;
	fSelected = 0;
	fHover = -1;
	fMessage = "";
	ScrollTo(0, 0);
	_UpdateScrollBar();
	Invalidate();
}


void
VideoGrid::SetPicture(const char* id, BBitmap* picture)
{
	for (size_t i = 0; i < fVideos.size(); i++) {
		if (fVideos[i].id == id) {
			delete fVideos[i].picture;
			fVideos[i].picture = picture;
			Invalidate(_TileFrame(i));
			return;
		}
	}
	delete picture;
}


void
VideoGrid::SetMessage(const char* message)
{
	_Clear();
	fMessage = message;
	ScrollTo(0, 0);
	_UpdateScrollBar();
	Invalidate();
}


void
VideoGrid::SetBusy(const char* title)
{
	fBusy = title != NULL ? title : "";
	delete fAnimation;
	fAnimation = NULL;
	if (title != NULL) {
		fAnimation = new BMessageRunner(BMessenger(this),
			new BMessage(kMsgAnimate), 400000);
	}
	Invalidate();
}


int32
VideoGrid::_Columns() const
{
	float width = Bounds().Width() + 1 - kGap;
	return std::max<int32>(1, (int32)(width / (kMinTileWidth + kGap)));
}


BRect
VideoGrid::_TileFrame(int32 index) const
{
	int32 columns = _Columns();
	float tileWidth = (Bounds().Width() + 1 - kGap) / columns - kGap;
	float pictureHeight = roundf(tileWidth * 9 / 16);
	float tileHeight = pictureHeight + kTitleHeight;
	float left = kGap + (index % columns) * (tileWidth + kGap);
	float top = kGap + (index / columns) * (tileHeight + kGap);
	return BRect(left, top, left + tileWidth, top + tileHeight);
}


int32
VideoGrid::_TileAt(BPoint where) const
{
	for (size_t i = 0; i < fVideos.size(); i++) {
		if (_TileFrame(i).Contains(where))
			return i;
	}
	return -1;
}


void
VideoGrid::_UpdateScrollBar()
{
	BScrollBar* bar = ScrollBar(B_VERTICAL);
	if (bar == NULL)
		return;
	float content = fVideos.empty() ? 0
		: _TileFrame(fVideos.size() - 1).bottom + kGap;
	float visible = Bounds().Height() + 1;
	float range = std::max(0.0f, content - visible);
	bar->SetRange(0, range);
	bar->SetProportion(content > 0 ? std::min(1.0f, visible / content) : 1);
	bar->SetSteps(40, visible * 0.9f);
}


void
VideoGrid::FrameResized(float width, float height)
{
	_UpdateScrollBar();
}


void
VideoGrid::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kNight);
	FillRect(updateRect);

	BRect bounds = Bounds();
	if (fVideos.empty()) {
		DrawCentered(this, fMessage.String(), BRect(bounds.left,
			bounds.top + bounds.Height() / 2 - 40, bounds.right,
			bounds.top + bounds.Height() / 2), Font(26), kMist);
	} else {
		for (size_t i = 0; i < fVideos.size(); i++) {
			BRect frame = _TileFrame(i);
			if (frame.Intersects(updateRect))
				_DrawTile(i, frame);
		}
	}

	if (!fBusy.IsEmpty())
		_DrawBusy();
}


static BString
duration_text(int32 seconds)
{
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


void
VideoGrid::_DrawTile(int32 index, BRect frame)
{
	const Video& video = fVideos[index];
	bool selected = index == fSelected;
	bool hover = index == fHover;

	BRect picture(frame.left, frame.top, frame.right,
		frame.bottom - kTitleHeight);
	if (selected) {
		FillRound(this, picture.InsetByCopy(-5, -5), 16, kSun);
	}
	FillRound(this, picture, 12, kNightLight);
	if (video.picture != NULL) {
		SetDrawingMode(B_OP_COPY);
		DrawBitmap(video.picture, video.picture->Bounds(), picture,
			B_FILTER_BITMAP_BILINEAR);
	} else {
		BRect play(0, 0, 36, 36);
		play.OffsetTo(picture.left + (picture.Width() - 36) / 2,
			picture.top + (picture.Height() - 36) / 2);
		DrawPlay(this, play, kMist);
	}
	if (hover || selected) {
		// a big play button in the middle
		float size = std::min(picture.Width(), picture.Height()) * 0.32f;
		BRect circle(0, 0, size, size);
		circle.OffsetTo(picture.left + (picture.Width() - size) / 2,
			picture.top + (picture.Height() - size) / 2);
		FillRound(this, circle, size / 2, (rgb_color){ 232, 64, 64, 230 });
		DrawPlay(this, circle.InsetByCopy(size * 0.3f, size * 0.28f), kWhite);
	}

	// the length, in a corner of the picture
	BFont small = Font(14);
	BString length = duration_text(video.duration);
	float width = small.StringWidth(length.String()) + 14;
	BRect badge(picture.right - 8 - width, picture.bottom - 30,
		picture.right - 8, picture.bottom - 8);
	FillRound(this, badge, 6, (rgb_color){ 0, 0, 0, 190 });
	DrawCentered(this, length.String(), badge, small, kWhite);

	// the title, on two lines at most
	BFont font = Font(17, selected);
	font_height height;
	font.GetHeight(&height);
	float lineHeight = ceilf(height.ascent + height.descent + height.leading);
	BString rest(video.title);
	float y = picture.bottom + 8 + height.ascent;
	for (int line = 0; line < 2 && !rest.IsEmpty(); line++) {
		BString text(rest);
		// break after the last word that fits, or cut the line short
		int32 cut = text.Length();
		while (cut > 0 && font.StringWidth(text.String(), cut)
				> frame.Width())
			cut = text.FindLast(' ', cut - 1);
		if (line == 1 || cut <= 0 || cut == text.Length()) {
			font.TruncateString(&text, B_TRUNCATE_END, frame.Width());
			rest = "";
		} else {
			text.Truncate(cut);
			rest.Remove(0, cut);
			rest.Trim();
		}
		DrawText(this, text.String(), BPoint(frame.left, y), font,
			selected ? kWhite : kMist);
		y += lineHeight;
	}
}


void
VideoGrid::_DrawBusy()
{
	BRect bounds = Bounds();
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	SetHighColor(10, 11, 20, 200);
	FillRect(bounds);

	float centerY = bounds.top + bounds.Height() / 2;
	BString dots;
	for (int i = 0; i < fDots % 4; i++)
		dots << ".";
	BString text(B_TRANSLATE("Getting the video ready"));
	text << dots;
	DrawCentered(this, text.String(), BRect(bounds.left, centerY - 60,
		bounds.right, centerY - 20), Font(30), kWhite);
	BString title(fBusy);
	BFont font = Font(20, false);
	font.TruncateString(&title, B_TRUNCATE_END, bounds.Width() - 80);
	DrawCentered(this, title.String(), BRect(bounds.left, centerY,
		bounds.right, centerY + 30), font, kSun);
}


void
VideoGrid::_Select(int32 index)
{
	if (fVideos.empty())
		return;
	index = std::max<int32>(0, std::min<int32>(index, fVideos.size() - 1));
	if (index == fSelected)
		return;
	Invalidate(_TileFrame(fSelected).InsetByCopy(-6, -6));
	fSelected = index;
	BRect frame = _TileFrame(fSelected).InsetByCopy(-6, -6);
	Invalidate(frame);

	BMessage selected(kMsgSelected);
	selected.AddString("id", fVideos[fSelected].id);
	Window()->PostMessage(&selected);

	// keep it in sight
	BRect bounds = Bounds();
	if (frame.top < bounds.top)
		ScrollTo(0, std::max(0.0f, frame.top - kGap));
	else if (frame.bottom > bounds.bottom)
		ScrollTo(0, frame.bottom + kGap - bounds.Height());
}


void
VideoGrid::_Play(int32 index)
{
	if (index < 0 || index >= (int32)fVideos.size() || !fBusy.IsEmpty())
		return;
	BMessage play(kMsgPlay);
	play.AddString("id", fVideos[index].id);
	play.AddString("title", fVideos[index].title);
	Window()->PostMessage(&play);
}


void
VideoGrid::KeyDown(const char* bytes, int32 count)
{
	int32 columns = _Columns();
	switch (bytes[0]) {
		case B_LEFT_ARROW:
			_Select(fSelected - 1);
			break;
		case B_RIGHT_ARROW:
			_Select(fSelected + 1);
			break;
		case B_UP_ARROW:
			_Select(fSelected - columns);
			break;
		case B_DOWN_ARROW:
			_Select(fSelected + columns);
			break;
		case B_ENTER:
		case B_SPACE:
			_Play(fSelected);
			break;
		default:
			BView::KeyDown(bytes, count);
	}
}


void
VideoGrid::MouseDown(BPoint where)
{
	MakeFocus(true);
	int32 index = _TileAt(where);
	if (index < 0)
		return;
	_Select(index);
	_Play(index);
}


void
VideoGrid::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	int32 index = transit == B_EXITED_VIEW ? -1 : _TileAt(where);
	if (index == fHover)
		return;
	if (fHover >= 0)
		Invalidate(_TileFrame(fHover));
	fHover = index;
	if (fHover >= 0)
		Invalidate(_TileFrame(fHover));
}


void
VideoGrid::MessageReceived(BMessage* message)
{
	if (message->what == kMsgAnimate) {
		fDots++;
		Invalidate();
		return;
	}
	BView::MessageReceived(message);
}

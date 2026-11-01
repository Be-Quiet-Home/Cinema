/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "VideoGrid.h"

#include <algorithm>

#include <Bitmap.h>
#include <Cursor.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <Window.h>

#include "Messages.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Grid"


using namespace Style;


static const float kMinTileWidth = 260;
static const float kGap = 20;
static const float kTitleHeight = 74;
	// two lines of title, one with the channel
static const float kTitleSize = 15;
static const float kChannelSize = 13;
static const float kRadius = 6;


VideoGrid::VideoGrid()
	:
	BView("videos", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE
		| B_FULL_UPDATE_ON_RESIZE | B_SCROLL_VIEW_AWARE),
	fSelected(0),
	fHover(-1),
	fHoverChannel(-1),
	fDots(0),
	fShelf(false),
	fShelfTileWidth(0),
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
	if (!fShelf)
		MakeFocus(true);
	_UpdateScrollBar();
}


void
VideoGrid::_Clear()
{
	for (MediaItem& video : fVideos)
		delete video.picture;
	fVideos.clear();
}


void
VideoGrid::SetVideos(const std::vector<MediaItem>& videos)
{
	_Clear();
	fVideos = videos;
	fSelected = 0;
	fHover = -1;
	fHoverChannel = -1;
	fMessage = "";
	ScrollTo(0, 0);
	_UpdateScrollBar();
	Invalidate();
}


void
VideoGrid::SetShelfMode(float tileWidth)
{
	fShelf = true;
	fShelfTileWidth = tileWidth;
	_UpdateScrollBar();
	Invalidate();
}


float
VideoGrid::ShelfHeightFor(float tileWidth)
{
	float pictureHeight = roundf(tileWidth * 9 / 16);
	return 2 * kGap + pictureHeight + kTitleHeight;
}


float
VideoGrid::ShelfHeight() const
{
	return ShelfHeightFor(fShelfTileWidth);
}


bool
VideoGrid::HasVideo(const char* id) const
{
	for (const MediaItem& video : fVideos)
		if (video.id == id)
			return true;
	return false;
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
VideoGrid::SetProgress(const char* id, int32 progress)
{
	for (size_t i = 0; i < fVideos.size(); i++) {
		if (fVideos[i].id == id) {
			if (fVideos[i].progress != progress) {
				fVideos[i].progress = progress;
				Invalidate(_TileFrame(i));
			}
			return;
		}
	}
}


void
VideoGrid::SetStatus(const char* id, const char* status)
{
	for (size_t i = 0; i < fVideos.size(); i++) {
		if (fVideos[i].id == id) {
			if (fVideos[i].status != status) {
				fVideos[i].status = status;
				Invalidate(_TileFrame(i));
			}
			return;
		}
	}
}


void
VideoGrid::SetDownloaded(const char* id, bool downloaded)
{
	for (size_t i = 0; i < fVideos.size(); i++) {
		if (fVideos[i].id == id) {
			fVideos[i].downloaded = downloaded;
			fVideos[i].progress = -1;
			Invalidate(_TileFrame(i));
			return;
		}
	}
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
	if (fShelf) {
		float width = Bounds().Width() - 2 * kGap + kGap;
		return std::max<int32>(1,
			(int32)(width / (fShelfTileWidth + kGap)));
	}
	float width = Bounds().Width() + 1 - kGap;
	return std::max<int32>(1, (int32)(width / (kMinTileWidth + kGap)));
}


int32
VideoGrid::_VisibleCount() const
{
	if (!fShelf)
		return fVideos.size();
	return std::min<int32>(fVideos.size(), _Columns());
}


BRect
VideoGrid::_TileFrame(int32 index) const
{
	if (fShelf) {
		float pictureHeight = roundf(fShelfTileWidth * 9 / 16);
		float tileHeight = pictureHeight + kTitleHeight;
		float left = kGap + (index % _Columns()) * (fShelfTileWidth + kGap);
		return BRect(left, kGap, left + fShelfTileWidth,
			kGap + tileHeight);
	}
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
	for (int32 i = 0; i < _VisibleCount(); i++) {
		if (_TileFrame(i).Contains(where))
			return i;
	}
	return -1;
}


static float
line_height(const BFont& font)
{
	font_height height;
	font.GetHeight(&height);
	return ceilf(height.ascent + height.descent + height.leading);
}


BRect
VideoGrid::_ChannelFrame(int32 index) const
{
	const MediaItem& video = fVideos[index];
	if (video.channel.IsEmpty() || video.channelId.IsEmpty())
		return BRect();
	BRect frame = _TileFrame(index);
	BFont font = Font(kChannelSize);
	float top = frame.bottom - kTitleHeight + 8
		+ 2 * line_height(Font(kTitleSize)) + 4;
	float width = std::min(frame.Width(),
		font.StringWidth(video.channel.String()));
	return BRect(frame.left, top, frame.left + width,
		top + line_height(font));
}


int32
VideoGrid::_ChannelAt(BPoint where) const
{
	int32 index = _TileAt(where);
	if (index >= 0 && _ChannelFrame(index).Contains(where))
		return index;
	return -1;
}


void
VideoGrid::_UpdateScrollBar()
{
	BScrollBar* bar = ScrollBar(B_VERTICAL);
	if (bar == NULL)
		return;
	if (fShelf) {
		bar->SetRange(0, 0);
		bar->SetProportion(1);
		return;
	}
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
VideoGrid::MakeFocus(bool focus)
{
	BView::MakeFocus(focus);
	Invalidate();
	if (focus && fShelf && Window() != NULL)
		Window()->PostMessage(kMsgShelfFocus);
}


void
VideoGrid::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kBackground);
	FillRect(updateRect);

	BRect bounds = Bounds();
	if (fVideos.empty()) {
		DrawCentered(this, fMessage.String(), BRect(bounds.left,
			bounds.top + bounds.Height() / 2 - 40, bounds.right,
			bounds.top + bounds.Height() / 2), Font(18), kTextDim);
	} else {
		for (int32 i = 0; i < _VisibleCount(); i++) {
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
	const MediaItem& video = fVideos[index];
	bool selected = index == fSelected && (!fShelf || IsFocus());
	bool hover = index == fHover;

	BRect picture(frame.left, frame.top, frame.right,
		frame.bottom - kTitleHeight);
	FillRound(this, picture, kRadius, kCard);
	if (video.picture != NULL) {
		SetDrawingMode(B_OP_COPY);
		DrawBitmap(video.picture, video.picture->Bounds(), picture,
			B_FILTER_BITMAP_BILINEAR);
	} else {
		BRect play(0, 0, 36, 36);
		play.OffsetTo(picture.left + (picture.Width() - 36) / 2,
			picture.top + (picture.Height() - 36) / 2);
		DrawPlay(this, play, kTextDim);
	}
	if (hover) {
		// a play button in the middle, under the mouse only
		float size = std::min(picture.Width(), picture.Height()) * 0.22f;
		BRect circle(0, 0, size, size);
		circle.OffsetTo(picture.left + (picture.Width() - size) / 2,
			picture.top + (picture.Height() - size) / 2);
		FillRound(this, circle, size / 2, (rgb_color){ 0, 0, 0, 150 });
		DrawPlay(this, circle.InsetByCopy(size * 0.32f, size * 0.3f), kText);
	}
	if (selected)
		StrokeRound(this, picture.InsetByCopy(-3, -3), kRadius + 2, 2, kAccent);

	if (video.progress >= 0) {
		// the download, as a bar along the bottom of the picture
		BRect bar(picture.left, picture.bottom - 3, picture.right,
			picture.bottom);
		SetDrawingMode(B_OP_ALPHA);
		SetHighColor(0, 0, 0, 160);
		FillRect(bar);
		bar.right = bar.left + bar.Width() * std::min<int32>(video.progress,
			100) / 100;
		SetDrawingMode(B_OP_COPY);
		SetHighColor(kAccent);
		FillRect(bar);
	}
	if (video.downloaded && video.path.IsEmpty()) {
		// a mark in the other corner: it can be watched offline
		BRect mark(picture.left + 6, picture.bottom - 24, picture.left + 24,
			picture.bottom - 6);
		FillRound(this, mark, 3, (rgb_color){ 0, 0, 0, 200 });
		SetDrawingMode(B_OP_COPY);
		SetHighColor(kText);
		SetPenSize(2);
		BPoint center(mark.left + mark.Width() / 2, mark.top + 4);
		StrokeLine(center, center + BPoint(0, 8));
		StrokeLine(center + BPoint(-4, 4), center + BPoint(0, 8));
		StrokeLine(center + BPoint(4, 4), center + BPoint(0, 8));
		StrokeLine(BPoint(mark.left + 4, mark.bottom - 3),
			BPoint(mark.right - 4, mark.bottom - 3));
		SetPenSize(1);
	}

	// the length, in a corner of the picture, when it is known
	if (video.duration > 0) {
		BFont small = Font(12, true);
		BString length = duration_text(video.duration);
		float width = small.StringWidth(length.String()) + 10;
		BRect badge(picture.right - 6 - width, picture.bottom - 24,
			picture.right - 6, picture.bottom - 6);
		FillRound(this, badge, 3, (rgb_color){ 0, 0, 0, 200 });
		DrawCentered(this, length.String(), badge, small, kText);
	}

	// the title, on two lines at most
	BFont font = Font(kTitleSize);
	font_height height;
	font.GetHeight(&height);
	float lineHeight = line_height(font);
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
		DrawText(this, text.String(), BPoint(frame.left, y), font, kText);
		y += lineHeight;
	}

	// a download on its way: how far it is, in place of the channel
	if (!video.status.IsEmpty() && video.progress >= 0) {
		BFont small = Font(kChannelSize);
		font_height smallHeight;
		small.GetHeight(&smallHeight);
		BString status(video.status);
		small.TruncateString(&status, B_TRUNCATE_END, frame.Width());
		float baseline = frame.bottom - kTitleHeight + 8
			+ 2 * line_height(Font(kTitleSize)) + 4 + smallHeight.ascent;
		DrawText(this, status.String(), BPoint(frame.left, baseline), small,
			kAccent);
		return;
	}

	// the channel, below: a link to its latest videos
	BRect channel = _ChannelFrame(index);
	if (channel.IsValid()) {
		BFont small = Font(kChannelSize);
		font_height smallHeight;
		small.GetHeight(&smallHeight);
		BString name(video.channel);
		small.TruncateString(&name, B_TRUNCATE_END, frame.Width());
		bool link = index == fHoverChannel;
		float baseline = channel.top + smallHeight.ascent;
		DrawText(this, name.String(), BPoint(channel.left, baseline), small,
			link ? kText : kTextDim);
		if (link) {
			StrokeLine(BPoint(channel.left, baseline + 2),
				BPoint(channel.right, baseline + 2));
		}
	}
}


void
VideoGrid::_DrawBusy()
{
	BRect bounds = Bounds();
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	SetHighColor(0, 0, 0, 200);
	FillRect(bounds);

	float centerY = bounds.top + bounds.Height() / 2;
	BString dots;
	for (int i = 0; i < fDots % 4; i++)
		dots << ".";
	BString text(B_TRANSLATE("Getting the video ready"));
	text << dots;
	DrawCentered(this, text.String(), BRect(bounds.left, centerY - 60,
		bounds.right, centerY - 20), Font(22, true), kText);
	BString title(fBusy);
	BFont font = Font(16);
	font.TruncateString(&title, B_TRUNCATE_END, bounds.Width() - 80);
	DrawCentered(this, title.String(), BRect(bounds.left, centerY,
		bounds.right, centerY + 30), font, kTextDim);
}


void
VideoGrid::_Select(int32 index)
{
	int32 count = _VisibleCount();
	if (count <= 0)
		return;
	index = std::max<int32>(0, std::min<int32>(index, count - 1));
	if (index == fSelected)
		return;
	Invalidate(_TileFrame(fSelected).InsetByCopy(-6, -6));
	fSelected = index;
	BRect frame = _TileFrame(fSelected).InsetByCopy(-6, -6);
	Invalidate(frame);

	BMessage selected(kMsgSelected);
	selected.AddString("id", fVideos[fSelected].id);
	Window()->PostMessage(&selected);

	if (!fShelf) {
		_UpdateScrollBar();
		BRect bounds = Bounds();
		if (frame.top < bounds.top)
			ScrollTo(0, std::max(0.0f, frame.top - kGap));
		else if (frame.bottom > bounds.bottom) {
			ScrollTo(0, std::max(0.0f,
				frame.bottom + kGap - bounds.Height()));
		}
	}
}


void
VideoGrid::_Play(int32 index)
{
	if (index < 0 || index >= (int32)fVideos.size() || !fBusy.IsEmpty())
		return;
	BMessage play(kMsgPlay);
	play.AddString("id", fVideos[index].id);
	play.AddString("title", fVideos[index].title);
	play.AddString("provider", fVideos[index].provider);
	play.AddString("kind", fVideos[index].kind);
	play.AddString("channel", fVideos[index].channel);
	play.AddString("channel id", fVideos[index].channelId);
	play.AddInt32("duration", fVideos[index].duration);
	if (!fVideos[index].path.IsEmpty())
		play.AddString("path", fVideos[index].path);
	Window()->PostMessage(&play);
}


void
VideoGrid::_Download(int32 index)
{
	if (index < 0 || index >= (int32)fVideos.size())
		return;
	const MediaItem& video = fVideos[index];
	if (video.downloaded || video.progress >= 0 || !video.path.IsEmpty())
		return;
	BMessage download = _DownloadMessage(index);
	Window()->PostMessage(&download);
}


void
VideoGrid::_ShowDetails(int32 index)
{
	if (index < 0 || index >= (int32)fVideos.size())
		return;
	BMessage details = _DetailsMessage(index);
	Window()->PostMessage(&details);
}


BMessage
VideoGrid::_DetailsMessage(int32 index) const
{
	const MediaItem& video = fVideos[index];
	BMessage details(kMsgShowDetails);
	details.AddString("provider", video.provider);
	details.AddString("kind", video.kind);
	details.AddString("id", video.id);
	details.AddString("title", video.title);
	details.AddString("description", video.description);
	details.AddString("channel", video.channel);
	details.AddString("channel id", video.channelId);
	details.AddInt32("duration", video.duration);
	details.AddInt32("year", video.year);
	details.AddString("date", video.date);
	details.AddInt64("views", video.views);
	details.AddDouble("rating", video.rating);
	details.AddString("poster", video.poster);
	details.AddString("thumbnail", video.thumbnail);
	details.AddString("path", video.path);
	details.AddBool("downloaded", video.downloaded);
	details.AddInt32("progress", video.progress);
	return details;
}


BMessage
VideoGrid::_DownloadMessage(int32 index) const
{
	const MediaItem& video = fVideos[index];
	BMessage download(kMsgDownload);
	download.AddString("provider", video.provider);
	download.AddString("kind", video.kind);
	download.AddString("id", video.id);
	download.AddString("title", video.title);
	download.AddString("channel", video.channel);
	download.AddString("channel id", video.channelId);
	download.AddInt32("duration", video.duration);
	return download;
}


void
VideoGrid::_ShowMenu(int32 index, BPoint where)
{
	const MediaItem& video = fVideos[index];
	BPopUpMenu* menu = new BPopUpMenu("video", false, false);
	BMessage* details = new BMessage(_DetailsMessage(index));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Details"), details));
	menu->AddSeparatorItem();
	BMessage* play = new BMessage(kMsgPlay);
	play->AddString("id", video.id);
	play->AddString("title", video.title);
	play->AddString("provider", video.provider);
	play->AddString("kind", video.kind);
	play->AddString("channel", video.channel);
	play->AddString("channel id", video.channelId);
	play->AddInt32("duration", video.duration);
	if (!video.path.IsEmpty())
		play->AddString("path", video.path);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Play"), play));

	if (video.path.IsEmpty()) {
		BMessage* download = new BMessage(_DownloadMessage(index));
		const char* label = B_TRANSLATE("Download");
		if (video.downloaded)
			label = B_TRANSLATE("Already downloaded");
		else if (video.progress >= 0)
			label = B_TRANSLATE("Downloading" B_UTF8_ELLIPSIS);
		BMenuItem* item = new BMenuItem(label, download);
		item->SetEnabled(!video.downloaded && video.progress < 0);
		menu->AddItem(item);
		if (video.progress >= 0) {
			BMessage* stop = new BMessage(kMsgCancelDownload);
			stop->AddString("id", video.id);
			menu->AddItem(new BMenuItem(B_TRANSLATE("Stop the download"),
				stop));
		}
	} else {
		BMessage* trash = new BMessage(kMsgTrash);
		trash->AddString("id", video.id);
		trash->AddString("path", video.path);
		menu->AddItem(new BMenuItem(B_TRANSLATE("Move to Trash"), trash));
	}
	menu->SetTargetForItems(Window());
	menu->Go(ConvertToScreen(where), true, false, true);
}


void
VideoGrid::KeyDown(const char* bytes, int32 count)
{
	int32 columns = _Columns();
	if (fShelf && (bytes[0] == B_UP_ARROW || bytes[0] == B_DOWN_ARROW)) {
		if (_VisibleCount() <= 0)
			return;
		BMessage move(kMsgShelfMove);
		move.AddString("id", fVideos[fSelected].id);
		move.AddInt32("delta", bytes[0] == B_UP_ARROW ? -1 : 1);
		Window()->PostMessage(&move);
		return;
	}
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
		case 'd':
		case 'D':
			_Download(fSelected);
			break;
		case 'i':
		case 'I':
			_ShowDetails(fSelected);
			break;
		case 'f':
		case 'F':
			Window()->PostMessage(kMsgFullScreen);
			break;
		default:
			BView::KeyDown(bytes, count);
	}
}


void
VideoGrid::MouseDown(BPoint where)
{
	MakeFocus(true);
	int32 channel = _ChannelAt(where);
	if (channel >= 0) {
		BMessage show(kMsgChannel);
		show.AddString("channel", fVideos[channel].channel);
		show.AddString("channel id", fVideos[channel].channelId);

		int32 buttons = B_PRIMARY_MOUSE_BUTTON;
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
		if ((buttons & B_SECONDARY_MOUSE_BUTTON) == 0) {
			Window()->PostMessage(&show);
			return;
		}
		BMessage* add = new BMessage(show);
		add->what = kMsgAddChannel;
		BPopUpMenu* menu = new BPopUpMenu("channel", false, false);
		menu->AddItem(new BMenuItem(B_TRANSLATE("Show the latest videos"),
			new BMessage(show)));
		menu->AddItem(new BMenuItem(B_TRANSLATE("Add to the tabs"), add));
		menu->SetTargetForItems(Window());
		menu->Go(ConvertToScreen(where), true, false, true);
		return;
	}
	int32 index = _TileAt(where);
	if (index < 0)
		return;
	_Select(index);
	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	Window()->CurrentMessage()->FindInt32("buttons", &buttons);
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0)
		_ShowMenu(index, where);
	else
		_Play(index);
}


void
VideoGrid::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	int32 channel = transit == B_EXITED_VIEW ? -1 : _ChannelAt(where);
	if (channel != fHoverChannel) {
		if (fHoverChannel >= 0)
			Invalidate(_ChannelFrame(fHoverChannel).InsetByCopy(-1, -4));
		fHoverChannel = channel;
		if (fHoverChannel >= 0)
			Invalidate(_ChannelFrame(fHoverChannel).InsetByCopy(-1, -4));
		BCursor cursor(channel >= 0
			? B_CURSOR_ID_FOLLOW_LINK : B_CURSOR_ID_SYSTEM_DEFAULT);
		SetViewCursor(&cursor);
	}

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

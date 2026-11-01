/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "CategoryBar.h"

#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#include "Messages.h"
#include "Style.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Categories"


using namespace Style;


static const float kHeight = 44;
static const float kPadding = 14;
static const float kGap = 2;
static const float kFontSize = 15;
static const float kChannelGap = 16;
static const float kAddSize = 22;


CategoryBar::CategoryBar(const std::vector<Category>& categories)
	:
	BView("categories", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fCategories(categories),
	fSelected(categories.empty() ? -1 : 0),
	fAddHover(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


BSize
CategoryBar::MinSize()
{
	return BSize(100, kHeight);
}


BSize
CategoryBar::PreferredSize()
{
	float width = 0;
	if (!fChannel.IsEmpty())
		width = _AddFrame().right;
	else if (_Count() > 0)
		width = _ButtonFrame(_Count() - 1).right;
	return BSize(width, kHeight);
}


int32
CategoryBar::_Count() const
{
	return fCategories.size() + 1 + (fChannel.IsEmpty() ? 0 : 1);
}


const char*
CategoryBar::_Name(int32 index) const
{
	// the downloads, and the channel shown, come after the categories
	if (index < (int32)fCategories.size())
		return fCategories[index].name.String();
	if (index == DownloadsIndex())
		return B_TRANSLATE("Downloaded");
	return fChannel.String();
}


BRect
CategoryBar::_ButtonFrame(int32 index) const
{
	BFont font = Font(kFontSize, true);
	float left = 0;
	for (int32 i = 0; i <= index; i++) {
		// a gap with a line before those that are not categories
		if (i >= DownloadsIndex())
			left += kChannelGap;
		if (i < index)
			left += font.StringWidth(_Name(i)) + 2 * kPadding + kGap;
	}
	float width = font.StringWidth(_Name(index)) + 2 * kPadding;
	return BRect(left, 0, left + width, kHeight - 1);
}


BRect
CategoryBar::_AddFrame() const
{
	if (fChannel.IsEmpty())
		return BRect();
	BRect tab = _ButtonFrame(DownloadsIndex() + 1);
	float top = (kHeight - kAddSize) / 2;
	return BRect(tab.right, top, tab.right + kAddSize, top + kAddSize);
}


void
CategoryBar::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kBar);
	FillRect(updateRect);

	// tabs: the chosen one is brighter and underlined
	for (int32 i = 0; i < _Count(); i++) {
		BRect frame = _ButtonFrame(i);
		if (frame.left > Bounds().right)
			break;
		bool channel = i == DownloadsIndex() + 1;
		bool selected = i == fSelected || channel;
		if (i >= DownloadsIndex()) {
			// a line apart from the categories
			SetDrawingMode(B_OP_COPY);
			SetHighColor(kCard);
			float x = frame.left - kChannelGap / 2;
			StrokeLine(BPoint(x, frame.top + 12), BPoint(x, frame.bottom - 12));
		}
		DrawCentered(this, _Name(i), frame, Font(kFontSize, selected),
			selected ? kText : kTextDim);
		if (selected) {
			SetDrawingMode(B_OP_COPY);
			SetHighColor(kAccent);
			FillRect(BRect(frame.left + kPadding / 2, frame.bottom - 2,
				frame.right - kPadding / 2, frame.bottom));
		}
	}

	// the button that keeps the channel among the tabs
	BRect add = _AddFrame();
	if (add.IsValid()) {
		if (fAddHover)
			FillRound(this, add, add.Height() / 2, kCard);
		SetDrawingMode(B_OP_COPY);
		SetHighColor(fAddHover ? kText : kTextDim);
		SetPenSize(2);
		BPoint center(add.left + add.Width() / 2, add.top + add.Height() / 2);
		StrokeLine(center - BPoint(5, 0), center + BPoint(5, 0));
		StrokeLine(center - BPoint(0, 5), center + BPoint(0, 5));
		SetPenSize(1);
	}
}


void
CategoryBar::MouseDown(BPoint where)
{
	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	Window()->CurrentMessage()->FindInt32("buttons", &buttons);

	if (_AddFrame().Contains(where)) {
		Window()->PostMessage(kMsgAddChannel);
		return;
	}
	if (_ButtonFrame(DownloadsIndex()).Contains(where)) {
		Window()->PostMessage(kMsgShowDownloads);
		return;
	}
	for (size_t i = 0; i < fCategories.size(); i++) {
		if (_ButtonFrame(i).Contains(where)) {
			if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
				_ShowMenu(i, where);
				return;
			}
			BMessage message(kMsgCategory);
			message.AddInt32("index", i);
			Window()->PostMessage(&message);
			return;
		}
	}
}


void
CategoryBar::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	bool hover = transit != B_EXITED_VIEW && _AddFrame().Contains(where);
	if (hover == fAddHover)
		return;
	fAddHover = hover;
	SetToolTip(hover ? B_TRANSLATE("Add this channel to the tabs") : NULL);
	if (hover)
		ShowToolTip(ToolTip());
	Invalidate(_AddFrame());
}


void
CategoryBar::_ShowMenu(int32 index, BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("tab", false, false);
	BMessage* message = new BMessage(kMsgRemoveCategory);
	message->AddInt32("index", index);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Remove from the tabs"),
		message));
	menu->SetTargetForItems(Window());
	menu->Go(ConvertToScreen(where), true, false, true);
}


void
CategoryBar::SetCategories(const std::vector<Category>& categories)
{
	fCategories = categories;
	InvalidateLayout();
	Invalidate();
}


void
CategoryBar::SetSelected(int32 index)
{
	fSelected = index;
	SetChannel(NULL);
	Invalidate();
}


void
CategoryBar::SetChannel(const char* name)
{
	BString channel(name);
	if (channel == fChannel)
		return;
	fChannel = channel;
	fAddHover = false;
	if (!fChannel.IsEmpty())
		fSelected = -1;
	InvalidateLayout();
	Invalidate();
}

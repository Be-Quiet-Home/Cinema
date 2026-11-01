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
static const float kMoreWidth = 26;
static const float kMoreGap = 6;


CategoryBar::CategoryBar(const std::vector<Category>& categories)
	:
	BView("categories", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fCategories(categories),
	fSelected(categories.empty() ? -1 : 0),
	fAddHover(false),
	fMoreHover(false)
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
	return fCategories.size() + 2 + (fChannel.IsEmpty() ? 0 : 1);
}


const char*
CategoryBar::_Name(int32 index) const
{
	if (index == HomeIndex())
		return B_TRANSLATE("Home");
	if (index == DownloadsIndex())
		return B_TRANSLATE("Downloaded");
	if (index >= 2 && index < (int32)fCategories.size() + 2)
		return fCategories[index - 2].name.String();
	return fChannel.String();
}


BRect
CategoryBar::_ButtonFrame(int32 index) const
{
	BFont font = Font(kFontSize, true);
	float left = 0;
	for (int32 i = 0; i <= index; i++) {
		// a gap with a line before the categories and before the channel
		if (i == 2 && !fCategories.empty())
			left += kChannelGap;
		else if (!fChannel.IsEmpty() && i == ChannelTab())
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
	BRect tab = _ButtonFrame(ChannelTab());
	float top = (kHeight - kAddSize) / 2;
	return BRect(tab.right, top, tab.right + kAddSize, top + kAddSize);
}


BRect
CategoryBar::_MoreFrame() const
{
	float top = (kHeight - kMoreWidth) / 2;
	return BRect(Bounds().right - kMoreWidth, top, Bounds().right,
		top + kMoreWidth);
}


bool
CategoryBar::_HasMore() const
{
	if (_Count() <= 0)
		return false;
	return _ButtonFrame(_Count() - 1).right > Bounds().right;
}


float
CategoryBar::_FitLimit() const
{
	if (!_HasMore())
		return Bounds().right;
	return Bounds().right - kMoreWidth - kMoreGap;
}


void
CategoryBar::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kBar);
	FillRect(updateRect);

	float limit = _FitLimit();
	bool more = _HasMore();

	// tabs: the chosen one is brighter and underlined
	for (int32 i = 0; i < _Count(); i++) {
		BRect frame = _ButtonFrame(i);
		if (frame.right > limit)
			break;
		bool channel = !fChannel.IsEmpty() && i == ChannelTab();
		bool selected = i == fSelected || channel;
		bool separated = (i == 2 && !fCategories.empty()) || channel;
		if (separated) {
			// a line apart from the other groups
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

	// the tabs that did not fit
	if (more) {
		BRect frame = _MoreFrame();
		bool hiddenSelected = fSelected >= 0 && fSelected < _Count()
			&& _ButtonFrame(fSelected).right > limit;
		if (fMoreHover)
			FillRound(this, frame, frame.Height() / 2, kCard);
		DrawCentered(this, "\xC2\xBB", frame, Font(20, true),
			fMoreHover ? kText : (hiddenSelected ? kAccent : kTextDim));
	}

	// the button that keeps the channel among the tabs
	BRect add = _AddFrame();
	if (add.IsValid() && add.right <= limit) {
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

	float limit = _FitLimit();
	if (_HasMore() && _MoreFrame().Contains(where)) {
		_ShowMore(where);
		return;
	}
	BRect add = _AddFrame();
	if (add.IsValid() && add.right <= limit && add.Contains(where)) {
		Window()->PostMessage(kMsgAddChannel);
		return;
	}
	BRect home = _ButtonFrame(HomeIndex());
	if (home.right <= limit && home.Contains(where)) {
		Window()->PostMessage(kMsgHome);
		return;
	}
	BRect downloads = _ButtonFrame(DownloadsIndex());
	if (downloads.right <= limit && downloads.Contains(where)) {
		Window()->PostMessage(kMsgShowDownloads);
		return;
	}
	for (size_t i = 0; i < fCategories.size(); i++) {
		BRect frame = _ButtonFrame(CategoryTab(i));
		if (frame.right > limit)
			break;
		if (frame.Contains(where)) {
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
	float limit = _FitLimit();
	BRect add = _AddFrame();
	bool addHover = transit != B_EXITED_VIEW && add.IsValid()
		&& add.right <= limit && add.Contains(where);
	if (addHover != fAddHover) {
		fAddHover = addHover;
		SetToolTip(addHover
			? B_TRANSLATE("Add this channel to the tabs") : NULL);
		if (addHover)
			ShowToolTip(ToolTip());
		Invalidate(add);
	}

	bool moreHover = transit != B_EXITED_VIEW && _HasMore()
		&& _MoreFrame().Contains(where);
	if (moreHover != fMoreHover) {
		fMoreHover = moreHover;
		Invalidate(_MoreFrame());
	}
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
CategoryBar::_ShowMore(BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("more", false, false);
	BMenuItem* item = new BMenuItem(_Name(HomeIndex()),
		new BMessage(kMsgHome));
	item->SetMarked(fSelected == HomeIndex());
	menu->AddItem(item);
	item = new BMenuItem(_Name(DownloadsIndex()),
		new BMessage(kMsgShowDownloads));
	item->SetMarked(fSelected == DownloadsIndex());
	menu->AddItem(item);
	if (!fCategories.empty())
		menu->AddSeparatorItem();
	for (size_t i = 0; i < fCategories.size(); i++) {
		BMessage* message = new BMessage(kMsgCategory);
		message->AddInt32("index", i);
		item = new BMenuItem(fCategories[i].name.String(), message);
		item->SetMarked(fSelected == CategoryTab(i));
		menu->AddItem(item);
	}
	if (!fCategories.empty()) {
		menu->AddSeparatorItem();
		BPopUpMenu* remove = new BPopUpMenu(
			B_TRANSLATE("Remove from the tabs"));
		for (size_t i = 0; i < fCategories.size(); i++) {
			BMessage* message = new BMessage(kMsgRemoveCategory);
			message->AddInt32("index", i);
			remove->AddItem(new BMenuItem(
				fCategories[i].name.String(), message));
		}
		remove->SetTargetForItems(Window());
		menu->AddItem(new BMenuItem(remove));
	}
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
	fMoreHover = false;
	if (!fChannel.IsEmpty())
		fSelected = -1;
	InvalidateLayout();
	Invalidate();
}

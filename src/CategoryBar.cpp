/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "CategoryBar.h"

#include <Window.h>

#include "Messages.h"
#include "Style.h"


using namespace Style;


static const float kHeight = 52;
static const float kPadding = 22;
static const float kGap = 10;


CategoryBar::CategoryBar(const std::vector<Category>& categories)
	:
	BView("categories", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fCategories(categories),
	fSelected(categories.empty() ? -1 : 0)
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
	if (!fCategories.empty())
		width = _ButtonFrame(fCategories.size() - 1).right;
	return BSize(width, kHeight);
}


BRect
CategoryBar::_ButtonFrame(int32 index) const
{
	BFont font = Font(19);
	float left = 0;
	for (int32 i = 0; i < index; i++) {
		left += font.StringWidth(fCategories[i].name.String()) + 2 * kPadding
			+ kGap;
	}
	float width = font.StringWidth(fCategories[index].name.String())
		+ 2 * kPadding;
	return BRect(left, 6, left + width, kHeight - 6);
}


void
CategoryBar::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kNightDark);
	FillRect(updateRect);

	for (size_t i = 0; i < fCategories.size(); i++) {
		BRect frame = _ButtonFrame(i);
		if (frame.left > Bounds().right)
			break;
		bool selected = (int32)i == fSelected;
		FillRound(this, frame, frame.Height() / 2,
			selected ? kSun : kNightLight);
		DrawCentered(this, fCategories[i].name.String(), frame, Font(19),
			selected ? kNightDark : kWhite);
	}
}


void
CategoryBar::MouseDown(BPoint where)
{
	for (size_t i = 0; i < fCategories.size(); i++) {
		if (_ButtonFrame(i).Contains(where)) {
			BMessage message(kMsgCategory);
			message.AddInt32("index", i);
			Window()->PostMessage(&message);
			return;
		}
	}
}


void
CategoryBar::SetSelected(int32 index)
{
	fSelected = index;
	Invalidate();
}

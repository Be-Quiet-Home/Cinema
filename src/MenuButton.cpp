/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "MenuButton.h"

#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#include "Messages.h"
#include "Style.h"

#include <initializer_list>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Window"


using namespace Style;


static const float kBaseSize = 32;


MenuButton::MenuButton()
	:
	BView("menu", B_WILL_DRAW),
	fHover(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetExplicitAlignment(BAlignment(B_ALIGN_HORIZONTAL_UNSET,
		B_ALIGN_VERTICAL_CENTER));
}


BSize
MenuButton::MinSize()
{
	return BSize(S(kBaseSize), S(kBaseSize));
}


BSize
MenuButton::MaxSize()
{
	return BSize(S(kBaseSize), S(kBaseSize));
}


void
MenuButton::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetDrawingMode(B_OP_COPY);
	SetHighColor(kBar);
	FillRect(bounds);
	if (fHover)
		FillRound(this, bounds.InsetByCopy(S(2), S(2)), S(6), kCard);
	SetDrawingMode(B_OP_COPY);
	SetHighColor(fHover ? kText : kTextDim);
	float left = bounds.left + S(9);
	float right = bounds.right - S(9);
	float middle = bounds.top + bounds.Height() / 2;
	for (float y : { middle - S(6), middle, middle + S(6) })
		FillRect(BRect(left, y - S(1), right, y));
}


void
MenuButton::MouseDown(BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("main", false, false);
	BMenuItem* settings = new BMenuItem(
		B_TRANSLATE("Settings" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowSettings), ',');
	menu->AddItem(settings);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("About Cinema"),
		new BMessage(kMsgShowAbout)));
	menu->SetTargetForItems(Window());
	ScaleMenu(menu);
	menu->Go(ConvertToScreen(BPoint(Bounds().right, Bounds().bottom)), true,
		false, true);
}


void
MenuButton::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	bool hover = transit != B_EXITED_VIEW && transit != B_OUTSIDE_VIEW;
	if (hover != fHover) {
		fHover = hover;
		Invalidate();
	}
}

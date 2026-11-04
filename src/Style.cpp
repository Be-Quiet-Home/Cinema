/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Style.h"

#include <MenuItem.h>
#include <Menu.h>
#include <Screen.h>
#include <View.h>

#include <math.h>
#include <stdlib.h>


namespace Style {

const rgb_color kBackground = { 24, 24, 27, 255 };
const rgb_color kCard = { 42, 42, 46, 255 };
const rgb_color kBar = { 17, 17, 19, 255 };
const rgb_color kAccent = { 82, 148, 232, 255 };
const rgb_color kText = { 236, 236, 240, 255 };
const rgb_color kTextDim = { 150, 150, 160, 255 };


static float
_ScreenScale()
{
	// for trying the sizes of a big screen on a small one
	const char* wanted = getenv("CINEMA_SCALE");
	if (wanted != NULL) {
		float scale = atof(wanted);
		if (scale > 0)
			return scale;
	}

	BScreen screen(B_MAIN_SCREEN_ID);
	if (!screen.IsValid())
		return 1;

	BRect frame = screen.Frame();
	// a 4K screen is twice the 1920x1080 the sizes were made for
	float factor = fminf((frame.Width() + 1) / 1920.0f,
		(frame.Height() + 1) / 1080.0f);
	if (factor >= 1.75f)
		return floorf(factor);
	return 1;
}


float
Scale()
{
	static float scale = _ScreenScale();
	return scale;
}


float
S(float value)
{
	return value * Scale();
}


static void
_UseAlpha(BView* view, bool composite = false)
{
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA,
		composite ? B_ALPHA_COMPOSITE : B_ALPHA_OVERLAY);
}


BFont
Font(float size, bool bold)
{
	BFont font(bold ? be_bold_font : be_plain_font);
	font.SetSize(size * Scale());
	return font;
}


void
ScaleView(BView* view)
{
	BFont font(be_plain_font);
	font.SetSize(font.Size() * Scale());
	view->SetFont(&font);
	for (int32 i = 0; i < view->CountChildren(); i++)
		ScaleView(view->ChildAt(i));
}


void
ScaleMenu(BMenu* menu)
{
	BFont font(be_plain_font);
	font.SetSize(font.Size() * Scale());
	menu->SetFont(&font);
	for (int32 i = 0; i < menu->CountItems(); i++) {
		BMenu* submenu = menu->SubmenuAt(i);
		if (submenu != NULL)
			ScaleMenu(submenu);
	}
}


void
FillRound(BView* view, BRect frame, float radius, rgb_color color,
	bool composite)
{
	_UseAlpha(view, composite);
	view->SetHighColor(color);
	view->FillRoundRect(frame, radius, radius);
}


void
StrokeRound(BView* view, BRect frame, float radius, float width,
	rgb_color color)
{
	_UseAlpha(view);
	view->SetHighColor(color);
	view->SetPenSize(width);
	view->StrokeRoundRect(frame, radius, radius);
	view->SetPenSize(1);
}


void
DrawCentered(BView* view, const char* text, BRect frame, const BFont& font,
	rgb_color color, bool composite)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	font_height height;
	font.GetHeight(&height);
	float x = frame.left + (frame.Width() - font.StringWidth(text)) / 2;
	float y = frame.top + (frame.Height() + height.ascent - height.descent) / 2;
	_UseAlpha(view, composite);
	view->DrawString(text, BPoint(x, y));
}


void
DrawText(BView* view, const char* text, BPoint leftBaseline,
	const BFont& font, rgb_color color, bool composite)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	_UseAlpha(view, composite);
	view->DrawString(text, leftBaseline);
}


void
DrawPlay(BView* view, BRect frame, rgb_color color, bool composite)
{
	_UseAlpha(view, composite);
	view->SetHighColor(color);
	view->FillTriangle(BPoint(frame.left + frame.Width() * 0.2f, frame.top),
		BPoint(frame.left + frame.Width() * 0.2f, frame.bottom),
		BPoint(frame.right, frame.top + frame.Height() / 2));
}

}	// namespace Style

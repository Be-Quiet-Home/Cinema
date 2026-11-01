/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Style.h"

#include <View.h>


namespace Style {

const rgb_color kBackground = { 24, 24, 27, 255 };
const rgb_color kCard = { 42, 42, 46, 255 };
const rgb_color kBar = { 17, 17, 19, 255 };
const rgb_color kAccent = { 82, 148, 232, 255 };
const rgb_color kText = { 236, 236, 240, 255 };
const rgb_color kTextDim = { 150, 150, 160, 255 };


static void
_UseAlpha(BView* view)
{
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
}


BFont
Font(float size, bool bold)
{
	BFont font(bold ? be_bold_font : be_plain_font);
	font.SetSize(size);
	return font;
}


void
FillRound(BView* view, BRect frame, float radius, rgb_color color)
{
	_UseAlpha(view);
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
	rgb_color color)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	font_height height;
	font.GetHeight(&height);
	float x = frame.left + (frame.Width() - font.StringWidth(text)) / 2;
	float y = frame.top + (frame.Height() + height.ascent - height.descent) / 2;
	_UseAlpha(view);
	view->DrawString(text, BPoint(x, y));
}


void
DrawText(BView* view, const char* text, BPoint leftBaseline,
	const BFont& font, rgb_color color)
{
	view->SetFont(&font);
	view->SetHighColor(color);
	_UseAlpha(view);
	view->DrawString(text, leftBaseline);
}


void
DrawPlay(BView* view, BRect frame, rgb_color color)
{
	_UseAlpha(view);
	view->SetHighColor(color);
	view->FillTriangle(BPoint(frame.left + frame.Width() * 0.2f, frame.top),
		BPoint(frame.left + frame.Width() * 0.2f, frame.bottom),
		BPoint(frame.right, frame.top + frame.Height() / 2));
}

}	// namespace Style

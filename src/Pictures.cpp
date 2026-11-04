/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Pictures.h"

#include <Bitmap.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <math.h>
#include <string.h>

#include "Style.h"


/*!	Each pixel of the smaller picture is the average of the pixels of the
	big one it covers: no aliasing, and cheap enough for pictures of a few
	hundred pixels. Both in 32 bits, B_RGBA32 or B_RGB32.
*/
static void
shrink(const BBitmap* from, BBitmap* to)
{
	int32 fromWidth = from->Bounds().IntegerWidth() + 1;
	int32 fromHeight = from->Bounds().IntegerHeight() + 1;
	int32 toWidth = to->Bounds().IntegerWidth() + 1;
	int32 toHeight = to->Bounds().IntegerHeight() + 1;
	const uint8* fromBits = (const uint8*)from->Bits();
	uint8* toBits = (uint8*)to->Bits();
	int32 fromRow = from->BytesPerRow();
	int32 toRow = to->BytesPerRow();

	for (int32 y = 0; y < toHeight; y++) {
		int32 top = (int32)((int64)y * fromHeight / toHeight);
		int32 bottom = std::max(top + 1,
			(int32)((int64)(y + 1) * fromHeight / toHeight));
		uint8* out = toBits + y * toRow;
		for (int32 x = 0; x < toWidth; x++) {
			int32 left = (int32)((int64)x * fromWidth / toWidth);
			int32 right = std::max(left + 1,
				(int32)((int64)(x + 1) * fromWidth / toWidth));
			uint32 sum[4] = { 0, 0, 0, 0 };
			for (int32 sy = top; sy < bottom; sy++) {
				const uint8* in = fromBits + sy * fromRow + left * 4;
				for (int32 sx = left; sx < right; sx++, in += 4) {
					sum[0] += in[0];
					sum[1] += in[1];
					sum[2] += in[2];
					sum[3] += in[3];
				}
			}
			uint32 count = (uint32)(bottom - top) * (right - left);
			for (int32 c = 0; c < 4; c++)
				out[c] = (uint8)((sum[c] + count / 2) / count);
			out += 4;
		}
	}
}


BBitmap*
Pictures::Load(const char* path, float largest)
{
	if (path == NULL || path[0] == '\0')
		return NULL;
	BBitmap* picture = BTranslationUtils::GetBitmap(path);
	if (picture == NULL || picture->InitCheck() != B_OK) {
		delete picture;
		return NULL;
	}

	float limit = Style::S(largest);
	float width = picture->Bounds().Width() + 1;
	float height = picture->Bounds().Height() + 1;
	if (width <= limit && height <= limit)
		return picture;

	// in 32 bits first, should the translator have given something else
	color_space space = picture->ColorSpace();
	if (space != B_RGBA32 && space != B_RGB32) {
		BBitmap* converted = new BBitmap(picture->Bounds(),
			B_BITMAP_NO_SERVER_LINK, B_RGBA32);
		if (converted->InitCheck() != B_OK
			|| converted->ImportBits(picture) != B_OK) {
			// not smaller then, but still shown
			delete converted;
			return picture;
		}
		delete picture;
		picture = converted;
		space = B_RGBA32;
	}

	float scale = std::min(limit / width, limit / height);
	BRect bounds(0, 0, std::max(1.0f, floorf(width * scale + 0.5f)) - 1,
		std::max(1.0f, floorf(height * scale + 0.5f)) - 1);
	BBitmap* small = new BBitmap(bounds, space);
	if (small->InitCheck() != B_OK) {
		delete small;
		return picture;
	}
	shrink(picture, small);
	delete picture;
	return small;
}

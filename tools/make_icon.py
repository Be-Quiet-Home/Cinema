#!/usr/bin/env python3
# Copyright 2026, Andrea Bernardi
# SPDX-License-Identifier: MIT
#
# make_icon.py [--preview DIR]
#
# Writes the vector icon of Cinema, a clapperboard with a play button, into
# Cinema.rdef.
# With --preview, also draws it as PNG files (needs PIL) to look at.

import math
import re
import sys


# #pragma mark - HVIF

def _coord(v):
	"""The two-byte form: value * 102 + 128 * 102, top bit set."""
	value = int(round((v + 128.0) * 102.0))
	if not 0 <= value <= 0x7fff:
		raise ValueError("coordinate %r out of range" % v)
	return bytes([0x80 | (value >> 8), value & 0xff])


def build_hvif(styles, paths, shapes):
	"""styles: (r, g, b, a) colors. paths: closed polygons. shapes: (style,
	[paths]) drawn in order."""
	out = bytearray(b"ncif")
	out.append(len(styles))
	for r, g, b, a in styles:
		out.append(0x01)					# solid color, RGBA
		out += bytes([r, g, b, a])
	out.append(len(paths))
	for points in paths:
		out.append(0x02 | 0x08)				# closed, no curves
		out.append(len(points))
		for x, y in points:
			out += _coord(x) + _coord(y)
	out.append(len(shapes))
	for style, indices in shapes:
		out.append(0x0a)					# path source
		out.append(style)
		out.append(len(indices))
		out += bytes(indices)
		out.append(0x00)					# no transform, no flags
	return bytes(out)


def rdef_block(data):
	lines = ["resource vector_icon {"]
	text = "".join("%02X" % b for b in data)
	for i in range(0, len(text), 64):
		lines.append('\t$"%s"' % text[i:i + 64])
	lines.append("};")
	return "\n".join(lines) + "\n"


# #pragma mark - shapes

def circle(cx, cy, r, steps=32):
	return [(cx + r * math.cos(2 * math.pi * i / steps),
		cy + r * math.sin(2 * math.pi * i / steps)) for i in range(steps)]


def rounded_rect(left, top, right, bottom, r, steps=8):
	points = []
	corners = [(right - r, top + r, -90), (right - r, bottom - r, 0),
		(left + r, bottom - r, 90), (left + r, top + r, 180)]
	for cx, cy, start in corners:
		for i in range(steps + 1):
			angle = math.radians(start + 90 * i / steps)
			points.append((cx + r * math.cos(angle), cy + r * math.sin(angle)))
	return points


def arc_band(cx, cy, r, width, start, end, steps=12):
	"""A thick arc, for the smile."""
	outer = [(cx + (r + width / 2) * math.cos(math.radians(start + (end - start)
		* i / steps)), cy + (r + width / 2) * math.sin(math.radians(start
		+ (end - start) * i / steps))) for i in range(steps + 1)]
	inner = [(cx + (r - width / 2) * math.cos(math.radians(start + (end - start)
		* i / steps)), cy + (r - width / 2) * math.sin(math.radians(start
		+ (end - start) * i / steps))) for i in range(steps + 1)]
	return outer + inner[::-1]


def grown(points, cx, cy, amount):
	"""Pushed out from (cx, cy): a rim around a round shape."""
	result = []
	for x, y in points:
		dx, dy = x - cx, y - cy
		length = math.hypot(dx, dy) or 1
		result.append((x + dx / length * amount, y + dy / length * amount))
	return result



NIGHT = (22, 24, 38, 255)
OUTLINE = (10, 11, 20, 255)
BODY = (46, 52, 86, 255)
BODY_LIGHT = (62, 70, 112, 255)
WHITE = (240, 240, 245, 255)
SUN = (255, 201, 60, 255)
SUN_DARK = (214, 150, 20, 255)


def quad(points):
	return list(points)


def build():
	styles = []
	paths = []
	shapes = []

	def add(color, polygons):
		if color not in styles:
			styles.append(color)
		indices = []
		for polygon in polygons:
			indices.append(len(paths))
			paths.append(polygon)
		shapes.append((styles.index(color), indices))

	# the board
	add(OUTLINE, [rounded_rect(4.5, 22.5, 59.5, 59.5, 6.5)])
	add(BODY, [rounded_rect(6, 24, 58, 58, 5)])
	add(BODY_LIGHT, [rounded_rect(8, 26, 56, 33, 3)])

	# the clapper on top, open, with its stripes
	def turned(points, angle, cx, cy):
		a = math.radians(angle)
		c, s = math.cos(a), math.sin(a)
		return [(cx + (x - cx) * c - (y - cy) * s,
			cy + (x - cx) * s + (y - cy) * c) for x, y in points]

	angle = -14
	stick = [(5, 10), (59, 10), (59, 20), (5, 20)]
	add(OUTLINE, [turned([(3.5, 8.5), (60.5, 8.5), (60.5, 21.5), (3.5, 21.5)],
		angle, 6, 21)])
	add(NIGHT, [turned(stick, angle, 6, 21)])
	stripes = []
	for i in range(5):
		x = 9 + i * 11
		stripes.append(turned([(x, 10), (x + 6, 10), (x + 2, 20), (x - 4, 20)],
			angle, 6, 21))
	add(WHITE, [turned([(p[0], p[1]) for p in s], 0, 0, 0) for s in stripes])
	add(OUTLINE, [circle(6.5, 21, 3.2, 16)])
	add(SUN, [circle(6.5, 21, 2, 16)])

	# the play button
	add(SUN_DARK, [circle(32, 44, 11.5)])
	add(SUN, [circle(32, 43.3, 10.5)])
	add(NIGHT, [[(28.5, 37.5), (28.5, 49), (38.5, 43.3)]])

	return styles, paths, shapes


def write(rdef, data):
	text = open(rdef).read()
	block = rdef_block(data)
	pattern = re.compile(r"resource vector_icon \{.*?\};\n", re.S)
	if pattern.search(text):
		text = pattern.sub(lambda match: block, text)
	else:
		text = text.rstrip("\n") + "\n\n" + block
	open(rdef, "w").write(text)


def preview(styles, paths, shapes, path, size):
	from PIL import Image, ImageDraw
	scale = 8
	image = Image.new("RGBA", (64 * scale, 64 * scale), (0, 0, 0, 0))
	for style, indices in shapes:
		layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
		draw = ImageDraw.Draw(layer)
		for i in indices:
			draw.polygon([(x * scale, y * scale) for x, y in paths[i]],
				fill=styles[style])
		image = Image.alpha_composite(image, layer)
	image.resize((size, size), Image.LANCZOS).save(path)


styles, paths, shapes = build()
data = build_hvif(styles, paths, shapes)
write("Cinema.rdef", data)
print("Cinema.rdef: %d bytes" % len(data))
if "--preview" in sys.argv:
	directory = sys.argv[sys.argv.index("--preview") + 1]
	for size in (16, 32, 64, 128):
		preview(styles, paths, shapes, "%s/cinema-%d.png" % (directory, size),
			size)

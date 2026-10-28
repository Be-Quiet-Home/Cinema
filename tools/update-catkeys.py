#!/usr/bin/env python3
# Copyright 2026, Andrea Bernardi
# SPDX-License-Identifier: MIT
#
# update-catkeys.py <language> [source=translation ...]
#
# Brings locales/<language>.catkeys up to date with locales/en.catkeys (run
# "make catkeys" first): keeps the translations it has, takes new ones from
# the command line, and lists the strings still missing a translation.

import sys

LANGUAGE_NAMES = {"it": "italian", "de": "german", "fr": "french",
	"es": "spanish"}


def read(path):
	try:
		lines = open(path, encoding="utf-8").read().splitlines()
	except FileNotFoundError:
		return None, {}
	entries = {}
	for line in lines[1:]:
		source, context, comment, translation = line.split("\t")
		entries[(source, context, comment)] = translation
	return lines[0].split("\t"), entries


language = sys.argv[1]
header, english = read("locales/en.catkeys")
_, translated = read("locales/%s.catkeys" % language)
given = dict(argument.split("=", 1) for argument in sys.argv[2:])

header[1] = LANGUAGE_NAMES.get(language, language)
output = ["\t".join(header)]
missing = []
for key in english:
	translation = translated.get(key, given.get(key[0]))
	if translation is None:
		missing.append(key[0])
		translation = key[0]
	output.append("\t".join(list(key) + [translation]))

open("locales/%s.catkeys" % language, "w", encoding="utf-8").write(
	"\n".join(output) + "\n")
for source in missing:
	print("not translated:", source)

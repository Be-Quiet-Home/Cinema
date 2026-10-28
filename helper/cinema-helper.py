#!/usr/bin/env python3
# Copyright 2026, Andrea Bernardi
# SPDX-License-Identifier: MIT
#
# cinema-helper: keeps yt-dlp loaded for Cinema, which talks to it through
# its standard input and output, one request or answer per line, with the
# fields separated by tabs:
#
#   search <tag> <count> <query>  ->  video <tag> <id> <title> <seconds> ...
#                                     done <tag>   (or nothing <tag>)
#   find <id>                     ->  found <id> <url>   (or lost <id>)
#
# Loading yt-dlp takes about 9 s on the AC100; this way it happens once.
# Each request runs on its own thread, so a search does not wait for a video.
# Searches are kept for a few hours in the folder given as argument.

import hashlib
import os
import sys
import threading
import time


def load_yt_dlp():
	try:
		import yt_dlp
		return yt_dlp
	except ImportError:
		pass
	# the single-file release: a zip archive Python can import from
	for directory in os.environ.get("PATH", "").split(os.pathsep):
		candidate = os.path.join(directory, "yt-dlp")
		if os.path.isfile(candidate):
			sys.path.insert(0, candidate)
			import yt_dlp
			return yt_dlp
	raise ImportError("yt-dlp not found")


yt_dlp = load_yt_dlp()
output = threading.Lock()

# YouTube's Android client needs no JavaScript to give the file; the web
# client with QuickJS is much slower on the AC100 and only used if it fails
FORMAT = "18/b[height<=360][vcodec^=avc1]"
OPTIONS = {"quiet": True, "no_warnings": True, "noprogress": True}
CACHE_TIME = 6 * 60 * 60
cache = sys.argv[1] if len(sys.argv) > 1 else None


def clean(text):
	return str(text).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def answer(*fields):
	with output:
		sys.stdout.write("\t".join(clean(field) for field in fields) + "\n")
		sys.stdout.flush()


def cached_search(count, query):
	"""The videos of a search: (id, title, seconds), from the cache when it
	is recent enough."""
	path = None
	if cache:
		key = hashlib.sha1(("%d %s" % (count, query)).encode()).hexdigest()
		path = os.path.join(cache, "search-%s.txt" % key[:16])
		try:
			if time.time() - os.path.getmtime(path) < CACHE_TIME:
				with open(path, encoding="utf-8") as file:
					return [line.rstrip("\n").split("\t") for line in file]
		except (OSError, ValueError):
			pass

	with yt_dlp.YoutubeDL(dict(OPTIONS, extract_flat="in_playlist")) as ydl:
		info = ydl.extract_info("ytsearch%d:%s" % (count, query),
			download=False)
	videos = []
	for entry in info.get("entries") or []:
		duration = entry.get("duration")
		# a live stream has no duration, and no file to play
		if entry.get("id") and duration:
			videos.append([entry["id"], clean(entry.get("title") or ""),
				str(int(duration))])
	if path and videos:
		with open(path + ".part", "w", encoding="utf-8") as file:
			for video in videos:
				file.write("\t".join(video) + "\n")
		os.replace(path + ".part", path)
	return videos


def search(tag, count, query):
	try:
		for video in cached_search(count, query):
			answer("video", tag, *video)
		answer("done", tag)
	except Exception:
		answer("nothing", tag)


def find(video):
	url = "https://www.youtube.com/watch?v=" + video
	clients = [
		{"youtube": {"player_client": ["android"]}},
		{"youtube": {}},
	]
	for arguments in clients:
		options = dict(OPTIONS, format=FORMAT, extractor_args=arguments)
		if not arguments["youtube"]:
			options["js_runtimes"] = {"quickjs": {}}
		try:
			with yt_dlp.YoutubeDL(options) as ydl:
				info = ydl.extract_info(url, download=False)
			if info.get("url"):
				answer("found", video, info["url"])
				return
		except Exception:
			pass
	answer("lost", video)


def serve():
	answer("ready")
	for line in sys.stdin:
		fields = line.rstrip("\n").split("\t")
		if fields[0] == "search" and len(fields) >= 4:
			work = (search, (fields[1], int(fields[2]), fields[3]))
		elif fields[0] == "find" and len(fields) >= 2:
			work = (find, (fields[1],))
		else:
			continue
		threading.Thread(target=work[0], args=work[1], daemon=True).start()


if __name__ == "__main__":
	serve()

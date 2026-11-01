#!/usr/bin/env python3
# Copyright 2026, Andrea Bernardi
# SPDX-License-Identifier: MIT
#
# cinema-helper: keeps the providers loaded for Cinema, which talks to it
# through its standard input and output, one JSON object per line.
#
# A request names a provider and what it wants:
#
#   {"type": "search",   "tag": 1, "provider": "youtube", "count": 24,
#    "query": "..."}
#   {"type": "channel",  "tag": 1, "provider": "youtube", "count": 24,
#    "channel": "UC..."}
#   {"type": "find",     "id": "...", "provider": "youtube", "kind": "video"}
#   {"type": "download", "id": "...", "provider": "youtube", "folder": "/..."}
#
# The answers:
#
#   {"type": "providers", "providers": [{"id", "name", "kinds"}, ...]}
#   {"type": "results", "tag": 1, "items": [MediaItem, ...]}
#   {"type": "nothing", "tag": 1}
#   {"type": "found", "id": "...", "url": "..."}
#   {"type": "lost", "id": "..."}
#   {"type": "progress", "id": "...", "percent": 42}
#   {"type": "saved", "id": "...", "path": "/..."}
#   {"type": "failed", "id": "..."}
#
# A MediaItem is {"provider", "kind", "id", "title", "description",
# "channel", "channel_id", "duration", "year", "rating", "poster",
# "thumbnail", "season", "episode"}. Other providers add more kinds later.
#
# Loading yt-dlp takes about 9 s on the AC100; this way it happens once.
# Each request runs on its own thread, so a search does not wait for a video.
# Searches are kept for a few hours in the folder given as argument.

import hashlib
import json
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

CACHE_TIME = 6 * 60 * 60
cache = sys.argv[1] if len(sys.argv) > 1 else None

DEFAULT_PROVIDER = "youtube"
PROVIDERS = {}


def answer(message):
	line = json.dumps(message, ensure_ascii=False)
	with output:
		sys.stdout.write(line + "\n")
		sys.stdout.flush()


def as_int(value, fallback):
	try:
		return int(value)
	except (TypeError, ValueError):
		return fallback


def clean(text):
	"""The text of a title or a channel: one line, as the grid shows it."""
	return str(text).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def cached(name, fetch):
	"""The videos fetch() gives, from the cache when it is recent enough."""
	path = None
	if cache:
		key = hashlib.sha1(name.encode()).hexdigest()
		path = os.path.join(cache, "videos-%s.json" % key[:16])
		try:
			if time.time() - os.path.getmtime(path) < CACHE_TIME:
				with open(path, encoding="utf-8") as file:
					return [json.loads(line) for line in file if line.strip()]
		except (OSError, ValueError):
			pass

	videos = fetch()
	if path and videos:
		temporary = path + ".part"
		try:
			with open(temporary, "w", encoding="utf-8") as file:
				for video in videos:
					file.write(json.dumps(video, ensure_ascii=False) + "\n")
			os.replace(temporary, path)
		except OSError:
			pass
	return videos


class Provider:
	"""A source of media: what Cinema can search, find and download.

	Each kind of the class says what it can do, and is what the helper tells
	Cinema at the start. The methods return MediaItems, not yt-dlp's info.
	"""

	id = ""
	name = ""
	kinds = ()

	def describe(self):
		return {"id": self.id, "name": self.name, "kinds": list(self.kinds)}

	def search(self, query, count):
		raise NotImplementedError

	def channel(self, channel_id, count):
		raise NotImplementedError

	def find(self, media_id, kind):
		raise NotImplementedError

	def download(self, media_id, folder, progress):
		raise NotImplementedError


class YouTubeProvider(Provider):
	id = "youtube"
	name = "YouTube"
	kinds = ("search", "channel", "find", "download")

	# YouTube's Android client needs no JavaScript to give the file; the web
	# client with QuickJS is much slower on the AC100 and only used if it fails
	FORMAT = "18/b[height<=360][vcodec^=avc1]"
	OPTIONS = {"quiet": True, "no_warnings": True, "noprogress": True}

	def item(self, entry, channel_name="", channel_id=""):
		"""The MediaItem of an entry of yt-dlp, or None if it can't be played."""
		duration = entry.get("duration")
		# a live stream has no duration, and no file to play
		if not entry.get("id") or not duration:
			return None
		return {
			"provider": self.id,
			"kind": "video",
			"id": entry["id"],
			"title": clean(entry.get("title") or ""),
			"description": clean(entry.get("description") or ""),
			"channel": clean(entry.get("channel") or channel_name),
			"channel_id": entry.get("channel_id") or channel_id,
			"duration": int(duration),
			"year": 0,
			"rating": 0.0,
			"poster": "",
			"thumbnail": "https://i.ytimg.com/vi/%s/mqdefault.jpg" % entry["id"],
			"season": 0,
			"episode": 0,
		}

	def videos_of(self, url, count, channel_name="", channel_id=""):
		options = dict(self.OPTIONS, extract_flat="in_playlist",
			playlistend=count)
		with yt_dlp.YoutubeDL(options) as ydl:
			info = ydl.extract_info(url, download=False)
		videos = []
		for entry in info.get("entries") or []:
			video = self.item(entry,
				channel_name=info.get("channel") or channel_name,
				channel_id=info.get("channel_id") or channel_id)
			if video is not None:
				videos.append(video)
		return videos

	def search(self, query, count):
		return self.videos_of("ytsearch%d:%s" % (count, query), count)

	def channel(self, channel_id, count):
		# the videos of a channel only name it once, in the list
		return self.videos_of(
			"https://www.youtube.com/channel/%s/videos" % channel_id,
			count, channel_id=channel_id)

	def find(self, media_id, kind="video"):
		url = "https://www.youtube.com/watch?v=" + media_id
		clients = [
			{"youtube": {"player_client": ["android"]}},
			{"youtube": {}},
		]
		for arguments in clients:
			options = dict(self.OPTIONS, format=self.FORMAT,
				extractor_args=arguments)
			if not arguments["youtube"]:
				options["js_runtimes"] = {"quickjs": {}}
			try:
				with yt_dlp.YoutubeDL(options) as ydl:
					info = ydl.extract_info(url, download=False)
				if info.get("url"):
					return info["url"]
			except Exception:
				pass
		return None

	def download(self, media_id, folder, progress=None):
		url = "https://www.youtube.com/watch?v=" + media_id
		last = [-1]

		def report(status):
			if progress is None or status.get("status") != "downloading":
				return
			total = status.get("total_bytes") or \
				status.get("total_bytes_estimate")
			if not total:
				return
			percent = int(100 * status.get("downloaded_bytes", 0) / total)
			if percent != last[0]:
				last[0] = percent
				progress(percent)

		# a single file with sound and picture, as played: nothing to merge
		options = dict(self.OPTIONS, format=self.FORMAT,
			progress_hooks=[report],
			outtmpl=os.path.join(folder,
				"%(title).150B [%(id)s].%(ext)s"),
			extractor_args={"youtube": {"player_client": ["android"]}})
		for attempt in range(2):
			try:
				with yt_dlp.YoutubeDL(options) as ydl:
					info = ydl.extract_info(url, download=True)
					done = info.get("requested_downloads") or [{}]
					path = done[0].get("filepath") \
						or ydl.prepare_filename(info)
				if os.path.exists(path):
					return path
			except Exception:
				pass
			# the web client, as find() does
			options["extractor_args"] = {"youtube": {}}
			options["js_runtimes"] = {"quickjs": {}}
		return None


def register(provider_class):
	provider = provider_class()
	PROVIDERS[provider.id] = provider
	return provider_class


register(YouTubeProvider)


def job_search(provider, tag, count, query):
	try:
		name = "%s search %d %s" % (provider.id, count, query)
		items = cached(name, lambda: provider.search(query, count))
		answer({"type": "results", "tag": tag, "items": items})
	except Exception:
		answer({"type": "nothing", "tag": tag})


def job_channel(provider, tag, count, channel_id):
	try:
		name = "%s channel %d %s" % (provider.id, count, channel_id)
		items = cached(name, lambda: provider.channel(channel_id, count))
		answer({"type": "results", "tag": tag, "items": items})
	except Exception:
		answer({"type": "nothing", "tag": tag})


def job_find(provider, media_id, kind):
	try:
		url = provider.find(media_id, kind)
	except Exception:
		url = None
	if url:
		answer({"type": "found", "id": media_id, "url": url})
	else:
		answer({"type": "lost", "id": media_id})


def job_download(provider, media_id, folder):
	def progress(percent):
		answer({"type": "progress", "id": media_id, "percent": percent})

	try:
		path = provider.download(media_id, folder, progress)
	except Exception:
		path = None
	if path:
		answer({"type": "saved", "id": media_id, "path": path})
	else:
		answer({"type": "failed", "id": media_id})


def work(request):
	provider = PROVIDERS.get(request.get("provider") or DEFAULT_PROVIDER)
	if provider is None:
		provider = PROVIDERS.get(DEFAULT_PROVIDER)
	if provider is None:
		return

	kind = request.get("type")
	tag = as_int(request.get("tag"), -1)
	count = as_int(request.get("count"), 24)

	if kind == "search":
		thread = (job_search,
			(provider, tag, count, request.get("query", "")))
	elif kind == "channel":
		thread = (job_channel,
			(provider, tag, count, request.get("channel", "")))
	elif kind == "find":
		thread = (job_find, (provider, request.get("id", ""),
			request.get("kind", "video")))
	elif kind == "download":
		thread = (job_download, (provider, request.get("id", ""),
			request.get("folder", "")))
	else:
		return
	threading.Thread(target=thread[0], args=thread[1], daemon=True).start()


def serve():
	answer({"type": "providers",
		"providers": [provider.describe() for provider in PROVIDERS.values()]})
	answer({"type": "ready"})
	for line in sys.stdin:
		line = line.strip()
		if not line:
			continue
		try:
			request = json.loads(line)
		except ValueError:
			continue
		if isinstance(request, dict):
			work(request)


if __name__ == "__main__":
	serve()

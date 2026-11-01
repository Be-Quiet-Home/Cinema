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
#   {"type": "details",  "provider": "youtube", "id": "...", "kind": "video"}
#   {"type": "find",     "id": "...", "provider": "youtube", "kind": "video"}
#   {"type": "download", "id": "...", "provider": "youtube", "folder": "/..."}
#
# The answers:
#
#   {"type": "providers", "providers": [{"id", "name", "kinds"}, ...]}
#   {"type": "results", "tag": 1, "items": [MediaItem, ...]}
#   {"type": "details", "id": "...", "item": MediaItem or null}
#   {"type": "nothing", "tag": 1}
#   {"type": "found", "id": "...", "url": "..."}
#   {"type": "lost", "id": "..."}
#   {"type": "progress", "id": "...", "percent": 42}
#   {"type": "saved", "id": "...", "path": "/..."}
#   {"type": "failed", "id": "..."}
#
# A MediaItem is {"provider", "kind", "id", "title", "description",
# "channel", "channel_id", "duration", "year", "date", "views", "rating",
# "poster", "thumbnail", "season", "episode"}. Other providers add more kinds
# later.
#
# Loading yt-dlp takes about 9 s on the AC100; this way it happens once.
# Each request runs on its own thread, so a search does not wait for a video.
# Searches are kept for a few hours in the folder given as argument.

import hashlib
import html
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request


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


try:
	yt_dlp = load_yt_dlp()
except ImportError:
	# the Internet Archive still works without it
	yt_dlp = None
output = threading.Lock()

CACHE_TIME = 6 * 60 * 60
DETAILS_CACHE_TIME = 24 * 60 * 60
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


def multiline(text):
	"""The description, with its own lines, as the detail view shows it."""
	return str(text).replace("\r\n", "\n").replace("\r", "\n").replace(
		"\t", " ").strip()


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


def cached_details(name, fetch):
	"""The MediaItem fetch() gives, from the cache when it is recent enough.

	A video does not change much, so its details are kept longer than the
	lists, and a failure is not kept at all.
	"""
	path = None
	if cache:
		key = hashlib.sha1(name.encode()).hexdigest()
		path = os.path.join(cache, "details-%s.json" % key[:16])
		try:
			if time.time() - os.path.getmtime(path) < DETAILS_CACHE_TIME:
				with open(path, encoding="utf-8") as file:
					return json.load(file)
		except (OSError, ValueError):
			pass

	item = fetch()
	if path and item:
		temporary = path + ".part"
		try:
			with open(temporary, "w", encoding="utf-8") as file:
				json.dump(item, file, ensure_ascii=False)
			os.replace(temporary, path)
		except OSError:
			pass
	return item


USER_AGENT = "Cinema (Haiku)"
# machines that play 320x240 or 360p at once, and need a faster processor
# for more, such as the Toshiba AC100
SLOW_MACHINE = platform.machine().lower().startswith("arm")


def http_json(url, query=None):
	if query:
		url += "?" + urllib.parse.urlencode(query)
	request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
	with urllib.request.urlopen(request, timeout=30) as response:
		return json.load(response)


def file_name(title):
	"""A title that can be the name of a file."""
	return re.sub(r'[/\\:*?"<>|]', " ", str(title)).strip()[:150] or "video"


def http_download(url, path, size=0, progress=None):
	"""Downloads url to path, through a .part file that goes away if it
	fails. size is used when the server does not tell the length."""
	temporary = path + ".part"
	request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
	try:
		with urllib.request.urlopen(request, timeout=60) as response, \
				open(temporary, "wb") as output_file:
			total = as_int(response.headers.get("Content-Length"), size)
			done = 0
			last = -1
			while True:
				block = response.read(256 * 1024)
				if not block:
					break
				output_file.write(block)
				done += len(block)
				if progress is not None and total:
					percent = int(100 * done / total)
					if percent != last:
						last = percent
						progress(percent)
	except Exception:
		try:
			os.remove(temporary)
		except OSError:
			pass
		raise
	os.replace(temporary, path)
	return path


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

	def details(self, media_id, kind):
		raise NotImplementedError

	def find(self, media_id, kind):
		raise NotImplementedError

	def download(self, media_id, folder, progress):
		raise NotImplementedError


class YouTubeProvider(Provider):
	id = "youtube"
	name = "YouTube"
	kinds = ("search", "channel", "details", "find", "download")

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
			"date": "",
			"views": 0,
			"rating": 0.0,
			"poster": "",
			"thumbnail": "https://i.ytimg.com/vi/%s/mqdefault.jpg" % entry["id"],
			"season": 0,
			"episode": 0,
		}

	def picture(self, info):
		"""The address of the biggest picture of a video yt-dlp described.

		The list of the thumbnails does not always say how big they are, so
		the name of the file does: maxresdefault is 1280x720, hq720 the same
		in another name, sddefault 640x480, hqdefault 480x360.
		"""
		urls = [thumbnail.get("url") or ""
			for thumbnail in info.get("thumbnails") or []]
		urls.append(info.get("thumbnail") or "")
		for name in ("maxresdefault", "hq720", "sddefault", "hqdefault",
				"mqdefault"):
			for url in urls:
				if name in url and ".jpg" in url:
					return url.split("?")[0]
		for url in urls:
			if ".jpg" in url:
				return url.split("?")[0]
		if info.get("id"):
			return "https://i.ytimg.com/vi/%s/hqdefault.jpg" % info["id"]
		return ""

	def full_item(self, info):
		"""The MediaItem of a video yt-dlp described in full, not flat."""
		video = self.item(info)
		if video is None:
			return None
		date = str(info.get("upload_date") or "")
		if len(date) == 8:
			video["date"] = "%s-%s-%s" % (date[:4], date[4:6], date[6:])
			video["year"] = as_int(date[:4], 0)
		video["description"] = multiline(info.get("description") or "")
		video["views"] = as_int(info.get("view_count"), 0)
		video["poster"] = self.picture(info)
		return video

	def details(self, media_id, kind="video"):
		url = "https://www.youtube.com/watch?v=" + media_id
		clients = [
			{"youtube": {"player_client": ["android"]}},
			{"youtube": {}},
		]
		for arguments in clients:
			options = dict(self.OPTIONS, extractor_args=arguments)
			if not arguments["youtube"]:
				options["js_runtimes"] = {"quickjs": {}}
			try:
				with yt_dlp.YoutubeDL(options) as ydl:
					info = ydl.extract_info(url, download=False)
			except Exception:
				continue
			video = self.full_item(info) if info else None
			if video is not None:
				return video
		return None

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


class ArchiveProvider(Provider):
	"""The films of the Internet Archive: public domain features, cartoons,
	television and newsreels, with their own metadata. A channel is one of
	its collections, such as feature_films or animationandcartoons.
	"""

	id = "archive"
	name = "Internet Archive"
	kinds = ("search", "channel", "details", "find", "download")

	SITE = "https://archive.org"
	FIELDS = ("identifier", "title", "year", "runtime", "description",
		"creator", "downloads", "avg_rating", "collection")
	# collections that only say where an item comes from
	GENERIC = {"moviesandfilms", "movies", "opensource_movies",
		"feature_films_unsorted", "community_media"}
	# the smallest file that still looks good first on slow machines: the
	# AC100 plays 320x240 at once, 640x480 needs a faster processor
	FORMATS = ("512Kb MPEG4", "h.264", "h.264 IA", "MPEG4", "Ogg Video") \
		if SLOW_MACHINE else ("h.264", "h.264 IA", "512Kb MPEG4", "MPEG4",
			"Ogg Video")

	def __init__(self):
		self.collection_names = {}

	def get_json(self, path, query=None):
		return http_json(self.SITE + path, query)

	@staticmethod
	def seconds(runtime):
		"""The length of a "1:17:26", "20:33", "77 min" or "1 hr 5 min"."""
		text = str(runtime or "").strip().lower()
		if re.fullmatch(r"\d+(:\d{1,2}){1,2}", text):
			total = 0
			for part in text.split(":"):
				total = total * 60 + int(part)
			return total
		hours = re.search(r"(\d+)\s*h", text)
		minutes = re.search(r"(\d+)\s*m", text)
		if hours or minutes:
			return (int(hours.group(1)) * 3600 if hours else 0) \
				+ (int(minutes.group(1)) * 60 if minutes else 0)
		return as_int(text, 0) * 60 if text.isdigit() else 0

	@staticmethod
	def text(value):
		"""A field that may be a list, as plain text without its HTML."""
		if isinstance(value, list):
			value = value[0] if value else ""
		value = re.sub(r"<br\s*/?>|</p>", "\n", str(value or ""),
			flags=re.IGNORECASE)
		return html.unescape(re.sub(r"<[^>]+>", "", value))

	def collection(self, collections):
		"""The collection that says the most about an item."""
		if isinstance(collections, str):
			collections = [collections]
		for name in collections or []:
			if name not in self.GENERIC:
				return name
		return (collections or [""])[0]

	def collection_name(self, identifier):
		if not identifier:
			return ""
		if identifier not in self.collection_names:
			try:
				metadata = self.get_json("/metadata/" + identifier)
				name = self.text(metadata.get("metadata", {}).get("title"))
			except Exception:
				name = ""
			self.collection_names[identifier] = name or identifier
		return self.collection_names[identifier]

	def item(self, document, channel_id=""):
		identifier = document.get("identifier")
		if not identifier:
			return None
		channel_id = channel_id or self.collection(document.get("collection"))
		picture = "%s/services/img/%s" % (self.SITE, identifier)
		return {
			"provider": self.id,
			"kind": "movie",
			"id": identifier,
			"title": clean(self.text(document.get("title")) or identifier),
			"description": clean(self.text(document.get("description"))),
			"channel": clean(self.collection_name(channel_id)),
			"channel_id": channel_id,
			"duration": self.seconds(self.text(document.get("runtime"))),
			"year": as_int(str(document.get("year") or "")[:4], 0),
			"date": "",
			"views": as_int(document.get("downloads"), 0),
			# reviews give 1 to 5 stars
			"rating": float(document.get("avg_rating") or 0) * 2,
			"poster": picture,
			"thumbnail": picture,
			"season": 0,
			"episode": 0,
		}

	def items(self, query, count, channel_id=""):
		response = self.get_json("/advancedsearch.php", [("q", query)]
			+ [("fl[]", field) for field in self.FIELDS]
			+ [("sort[]", "downloads desc"), ("rows", str(count)),
				("output", "json")])
		items = []
		for document in response.get("response", {}).get("docs", []):
			item = self.item(document, channel_id)
			if item is not None:
				items.append(item)
		return items

	# films only, and only those that anybody can watch
	PLAYABLE = " AND mediatype:(movies) AND NOT access-restricted-item:(true)"

	def search(self, query, count):
		words = query.replace('"', " ").strip()
		return self.items("(%s)%s" % (words, self.PLAYABLE), count)

	def channel(self, channel_id, count):
		return self.items("collection:(%s)%s" % (channel_id, self.PLAYABLE),
			count, channel_id)

	def metadata(self, media_id):
		metadata = self.get_json("/metadata/" + media_id)
		return metadata if metadata.get("metadata") else None

	def video_file(self, metadata):
		"""The file to play: the first of FORMATS, the smallest of a kind."""
		files = metadata.get("files") or []
		for wanted in self.FORMATS:
			found = [f for f in files if f.get("format") == wanted]
			if found:
				return min(found, key=lambda f: as_int(f.get("size"), 0))
		for f in files:
			if f.get("name", "").lower().endswith((".mp4", ".ogv", ".webm")):
				return f
		return None

	def file_url(self, media_id, file):
		return "%s/download/%s/%s" % (self.SITE, media_id,
			urllib.parse.quote(file["name"]))

	def details(self, media_id, kind="movie"):
		metadata = self.metadata(media_id)
		if metadata is None:
			return None
		fields = dict(metadata["metadata"], identifier=media_id)
		item = self.item(fields)
		item["description"] = multiline(self.text(
			metadata["metadata"].get("description")))
		date = self.text(metadata["metadata"].get("date"))
		if re.match(r"\d{4}-\d{2}-\d{2}", date):
			item["date"] = date[:10]
		if not item["year"]:
			item["year"] = as_int(date[:4], 0)
		file = self.video_file(metadata)
		if file is not None and not item["duration"]:
			item["duration"] = int(float(file.get("length") or 0))
		return item

	def find(self, media_id, kind="movie"):
		metadata = self.metadata(media_id)
		file = self.video_file(metadata) if metadata else None
		return self.file_url(media_id, file) if file else None

	def download(self, media_id, folder, progress=None):
		metadata = self.metadata(media_id)
		file = self.video_file(metadata) if metadata else None
		if file is None:
			return None
		title = file_name(self.text(metadata["metadata"].get("title"))
			or media_id)
		extension = os.path.splitext(file["name"])[1] or ".mp4"
		path = os.path.join(folder, "%s [%s]%s" % (title, media_id, extension))
		return http_download(self.file_url(media_id, file), path,
			as_int(file.get("size"), 0), progress)


class PeerTubeProvider(Provider):
	"""The videos of PeerTube, the federation of video sites: searched on
	all of them through SepiaSearch, played from the site that has them.
	A video is "<uuid>@<site>", a channel "<name>@<site>".
	"""

	id = "peertube"
	name = "PeerTube"
	kinds = ("search", "channel", "details", "find", "download")

	SEARCH = "https://sepiasearch.org/api/v1/search/videos"
	# the highest resolution to play
	RESOLUTION = 360 if SLOW_MACHINE else 720

	@staticmethod
	def split(media_id):
		name, _, host = str(media_id).rpartition("@")
		return name, host

	def item(self, video, host=""):
		channel = video.get("channel") or {}
		host = channel.get("host") or host
		if not video.get("uuid") or not host or video.get("isLive") \
				or video.get("nsfw"):
			return None

		def address(url, path):
			if url:
				return url
			return "https://%s%s" % (host, path) if path else ""

		date = str(video.get("publishedAt") or "")[:10]
		return {
			"provider": self.id,
			"kind": "video",
			"id": "%s@%s" % (video["uuid"], host),
			"title": clean(video.get("name") or ""),
			"description": clean(video.get("truncatedDescription")
				or video.get("description") or ""),
			"channel": clean(channel.get("displayName")
				or channel.get("name") or ""),
			"channel_id": "%s@%s" % (channel.get("name"), host)
				if channel.get("name") else "",
			"duration": as_int(video.get("duration"), 0),
			"year": as_int(date[:4], 0),
			"date": date,
			"views": as_int(video.get("views"), 0),
			"rating": 0.0,
			"poster": address(video.get("previewUrl"),
				video.get("previewPath")),
			"thumbnail": address(video.get("thumbnailUrl"),
				video.get("thumbnailPath")),
			"season": 0,
			"episode": 0,
		}

	def items(self, videos, host=""):
		return [item for item in (self.item(video, host) for video in videos)
			if item is not None]

	def search(self, query, count):
		response = http_json(self.SEARCH, {"search": query, "count": count,
			"nsfw": "false", "isLive": "false"})
		return self.items(response.get("data") or [])

	def channel(self, channel_id, count):
		name, host = self.split(channel_id)
		response = http_json("https://%s/api/v1/video-channels/%s/videos"
			% (host, urllib.parse.quote(name)),
			{"count": count, "sort": "-publishedAt", "nsfw": "false"})
		return self.items(response.get("data") or [], host)

	def video(self, media_id):
		uuid, host = self.split(media_id)
		return http_json("https://%s/api/v1/videos/%s" % (host, uuid)), host

	def details(self, media_id, kind="video"):
		video, host = self.video(media_id)
		item = self.item(video, host)
		if item is not None:
			item["description"] = multiline(video.get("description") or "")
		return item

	def video_file(self, video):
		"""The file to play: the biggest up to RESOLUTION, or the smallest
		when they are all bigger."""
		files = list(video.get("files") or [])
		for playlist in video.get("streamingPlaylists") or []:
			files += playlist.get("files") or []
		files = [f for f in files if f.get("fileUrl")
			and as_int((f.get("resolution") or {}).get("id"), 0) > 0]
		if not files:
			return None

		def height(f):
			return as_int((f.get("resolution") or {}).get("id"), 0)

		fitting = [f for f in files if height(f) <= self.RESOLUTION]
		if fitting:
			return max(fitting, key=height)
		return min(files, key=height)

	def find(self, media_id, kind="video"):
		video, host = self.video(media_id)
		file = self.video_file(video)
		return file["fileUrl"] if file else None

	def download(self, media_id, folder, progress=None):
		video, host = self.video(media_id)
		file = self.video_file(video)
		if file is None:
			return None
		uuid, host = self.split(media_id)
		extension = os.path.splitext(
			urllib.parse.urlparse(file["fileUrl"]).path)[1] or ".mp4"
		path = os.path.join(folder, "%s [%s]%s"
			% (file_name(video.get("name") or uuid), uuid[:8], extension))
		return http_download(file["fileUrl"], path, as_int(file.get("size"), 0),
			progress)


VIDEO_EXTENSIONS = (".mp4", ".m4v", ".mkv", ".webm", ".avi", ".mov", ".ogv",
	".mpg", ".mpeg", ".wmv", ".flv", ".ts")


class TorrentProvider(Provider):
	"""Downloads what a magnet link or a .torrent file (a path or an
	address) shares, with aria2c, and keeps its biggest video. The id is
	the link, the path or the address itself.
	"""

	id = "torrent"
	name = "BitTorrent"
	kinds = ("download",)

	PERCENT = re.compile(r"\((\d+)%\)")

	def download(self, media_id, folder, progress=None):
		key = hashlib.sha1(media_id.encode()).hexdigest()[:12]
		work = os.path.join(folder, ".torrent-" + key)
		os.makedirs(work, exist_ok=True)
		try:
			command = ["aria2c", "--dir=" + work, "--seed-time=0",
				"--summary-interval=1", "--console-log-level=error",
				"--follow-torrent=mem", "--file-allocation=none",
				"--bt-save-metadata=false", "--enable-dht=true",
				"--auto-file-renaming=false", "--allow-overwrite=true",
				# give up after ten minutes with nothing coming
				"--bt-stop-timeout=600", "--connect-timeout=30",
				media_id]
			process = subprocess.Popen(command, stdout=subprocess.PIPE,
				stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
				text=True, errors="replace")
			last = -1
			for line in process.stdout:
				found = self.PERCENT.search(line)
				if found and progress is not None:
					percent = int(found.group(1))
					if percent != last:
						last = percent
						progress(percent)
			if process.wait() != 0:
				return None

			videos = []
			for directory, _, names in os.walk(work):
				for name in names:
					if name.lower().endswith(VIDEO_EXTENSIONS):
						path = os.path.join(directory, name)
						videos.append((os.path.getsize(path), path))
			if not videos:
				return None
			source = max(videos)[1]
			stem, extension = os.path.splitext(os.path.basename(source))
			path = os.path.join(folder, "%s [%s]%s"
				% (file_name(stem), key[:8], extension))
			shutil.move(source, path)
			return path
		finally:
			shutil.rmtree(work, ignore_errors=True)


class LocalProvider(Provider):
	"""The videos in a folder of this computer: a channel is the folder,
	a video its path. Their length and a picture come from ffprobe and
	ffmpeg when they are there, and are kept in the cache.
	"""

	id = "local"
	name = "Files"
	kinds = ("channel", "details", "find")

	DEPTH = 3
	LIMIT = 500
	# what is not known yet is found this long, the rest the next time
	BUDGET = 6

	def __init__(self):
		self.lock = threading.Lock()
		self.known = {}
		self.path = os.path.join(cache, "local.json") if cache else None
		try:
			with open(self.path, encoding="utf-8") as file:
				self.known = json.load(file)
		except (OSError, ValueError, TypeError):
			pass

	def save(self):
		if not self.path:
			return
		with self.lock:
			try:
				with open(self.path + ".part", "w", encoding="utf-8") as file:
					json.dump(self.known, file)
				os.replace(self.path + ".part", self.path)
			except OSError:
				pass

	def probe(self, path, key):
		"""The length of a video and the path of a picture of it."""
		duration = 0
		picture = ""
		try:
			result = subprocess.run(["ffprobe", "-v", "error",
				"-show_entries", "format=duration", "-of",
				"default=noprint_wrappers=1:nokey=1", path],
				capture_output=True, text=True, timeout=30)
			duration = int(float(result.stdout.strip() or 0))
		except (OSError, ValueError, subprocess.SubprocessError):
			pass
		if cache:
			picture = os.path.join(cache, "local-%s.jpg"
				% hashlib.sha1(key.encode()).hexdigest()[:16])
			try:
				subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss",
					str(min(60, max(1, duration // 10))), "-i", path,
					"-frames:v", "1", "-vf", "scale=320:-2", picture],
					capture_output=True, timeout=60)
			except (OSError, subprocess.SubprocessError):
				pass
			if not os.path.exists(picture):
				picture = ""
		return {"duration": duration, "picture": picture}

	def videos(self, folder):
		found = []
		folder = os.path.abspath(folder)
		base = folder.count(os.sep)
		for directory, directories, names in os.walk(folder):
			if directory.count(os.sep) - base >= self.DEPTH:
				directories[:] = []
			directories[:] = [d for d in directories if not d.startswith(".")]
			for name in names:
				if name.lower().endswith(VIDEO_EXTENSIONS) \
						and not name.startswith("."):
					path = os.path.join(directory, name)
					try:
						found.append((os.path.getmtime(path), path))
					except OSError:
						pass
		found.sort(reverse=True)
		return [path for _, path in found[:self.LIMIT]]

	def item(self, path, wait):
		try:
			info = os.stat(path)
		except OSError:
			return None
		key = "%s %d %d" % (path, info.st_size, int(info.st_mtime))
		known = self.known.get(key)
		if known is None and wait:
			known = self.probe(path, key)
			with self.lock:
				self.known[key] = known
		known = known or {}
		picture = known.get("picture") or ""
		return {
			"provider": self.id,
			"kind": "video",
			"id": path,
			"title": clean(os.path.splitext(os.path.basename(path))[0]),
			"description": path,
			"channel": clean(os.path.basename(os.path.dirname(path))),
			"channel_id": os.path.dirname(path),
			"duration": as_int(known.get("duration"), 0),
			"year": time.localtime(info.st_mtime).tm_year,
			"date": time.strftime("%Y-%m-%d", time.localtime(info.st_mtime)),
			"views": 0,
			"rating": 0.0,
			"poster": "file://" + picture if picture else "",
			"thumbnail": "file://" + picture if picture else "",
			"season": 0,
			"episode": 0,
		}

	def channel(self, channel_id, count):
		start = time.time()
		items = []
		late = []
		for path in self.videos(channel_id):
			wait = time.time() - start < self.BUDGET
			item = self.item(path, wait)
			if item is not None:
				items.append(item)
				if not wait:
					late.append(path)
		if late:
			# what was not looked at yet, for the next time
			def finish():
				for path in late:
					self.item(path, True)
				self.save()
			threading.Thread(target=finish, daemon=True).start()
		self.save()
		return items

	def details(self, media_id, kind="video"):
		return self.item(media_id, True)

	def find(self, media_id, kind="video"):
		return media_id if os.path.isfile(media_id) else None


if yt_dlp is not None:
	register(YouTubeProvider)
	DEFAULT_PROVIDER = "youtube"
else:
	DEFAULT_PROVIDER = "archive"
register(ArchiveProvider)
register(PeerTubeProvider)
if shutil.which("aria2c"):
	register(TorrentProvider)
register(LocalProvider)


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


def job_details(provider, media_id, kind):
	try:
		name = "%s details %s %s" % (provider.id, kind, media_id)
		item = cached_details(name,
			lambda: provider.details(media_id, kind))
	except Exception:
		item = None
	answer({"type": "details", "id": media_id, "item": item})


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
	elif kind == "details":
		thread = (job_details, (provider, request.get("id", ""),
			request.get("kind", "video")))
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

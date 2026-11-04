# Cinema

Videos and live TV on [Haiku](https://www.haiku-os.org) and
[RenkuOS](https://github.com/RenkuOS), in a player of their own: YouTube (with
[yt-dlp](https://github.com/yt-dlp/yt-dlp)), the public domain films and
cartoons of the [Internet Archive](https://archive.org), the free video sites
of [PeerTube](https://joinpeertube.org), BitTorrent, live TV channels, and the
videos of your own folders.

It runs on x86_64, and on 32-bit ARM with RenkuOS, for example on the Toshiba
AC100.

![The videos of a category](screenshots/main.png)

## What it does

- **Sources**: the menu next to the search field chooses where to search
  (YouTube, Internet Archive, PeerTube); `@peertube words` in the field does it
  for one search.
- **Tabs** at the top: searches, or channels whose latest videos are shown.
  They are kept in `~/config/settings/Cinema/categories`, one per line:
  `Name = what to search on YouTube`, or `Name = channel:<id>`. `@source`
  before them takes another source, as in
  `Classic films = @archive channel:feature_films` (a collection of the
  Internet Archive), `Blender = @peertube channel:blender_open_movies@video.blender.org`
  or `Home videos = @local channel:/boot/home/Videos`.
- **Your videos**: drop a folder on the window and it becomes a tab of the
  videos in it, newest first, with their length and a picture (from `ffprobe`
  and `ffmpeg`, when they are installed). Drop a video to play it.
- **Library**: the films and series in the folders you dropped get the
  **Movies** and **TV series** tabs. Their names are read from the files
  (`Title (1968)`, `Title.1968.720p`, `Show.S01E03`, `Show - 1x03`, or a
  `Season 1` folder), and, with a key of [TMDB](https://www.themoviedb.org)
  in the settings (an API key or a read access token, both free), they get
  posters, plots, casts and the names of the episodes,
  kept for a month so that the library opens without the network too.
  A series opens its episodes, with the next one to watch chosen, and an
  episode watched to the end goes on to the next.
- **Player**: the videos play in Cinema, from where they were left. A check
  marks what was watched, a red bar what was started. Space pauses, the
  arrows go back and on by 10 seconds and change the volume, Esc closes,
  M plays the video in MediaPlayer instead. On the whole screen (**F**) the
  tabs go away too.
- **Subtitles**: the tracks of YouTube, the subtitle files next to a video
  and the text tracks inside it. The **CC** button chooses one, **V** turns
  them on and off.
- **Settings** (the **≡** menu, or Alt+,): the key of TMDB, the folders of
  the library, the TV lists, whether videos play in Cinema or in MediaPlayer
  and from where they were left, the video quality, and where a search goes
  first.
- **Video quality**, in the settings: Automatic (360p on 32-bit ARM, 720p
  elsewhere), or 360p to 1080p. Above 360p YouTube gives the picture and the
  sound apart: `ffmpeg` puts them together for the player, and finding them
  takes longer (about a minute and a half on the AC100).
- **TV**: the free channels of your country, from the public lists of
  [iptv-org](https://github.com/iptv-org/iptv), get a **TV** tab the first
  time; the settings add the channels of other countries, or any M3U list
  (an address, or an `.m3u` file dropped on the window). Live channels play
  with FFmpeg, which keeps up on slow machines by decoding less. Channels for
  adults are left out.
- **Favorites**: **S** or a right click gives a channel or a video a star;
  the stars have a **Favorites** tab of their own.
- **BitTorrent**: paste a magnet link in the search field, or drop a
  `.torrent` file on the window: the biggest video it shares is downloaded with
  `aria2c` to the **Downloaded** tab, where its progress shows and it can be
  stopped; a download stopped, or cut by a restart, goes on from where it was.
  Use it for what you may share, such as the films of the Blender Foundation
  or the torrents of the Internet Archive.
- **Channels**: the name of the channel is under each video; a click shows its
  latest videos, and the **+** button next to its tab keeps it among the tabs
  (a right click on the name does it too). A right click on a tab removes it.
- **Downloads** to watch offline: press **D** on a video, or right click it.
  The files go to `~/Cinema`, with their title, channel and length in
  attributes, and are listed in the **Downloaded** tab, which works without the
  network. A video downloaded plays from its file wherever it is shown.
- **Keys**: arrows to choose, **Enter** or **Space** to play, **D** to download,
  **S** for a star, **F** for full screen and **Esc** to go back. **Tab** or **F6** goes to the
  top tabs, the arrows choose one and **Enter** opens it. A gamepad works too:
  its D-pad or left stick chooses, **A** plays, **B** goes back, **X** downloads
  and **Y** toggles full screen; **L1**, **Select** or **Start** goes to the top tabs.
- English, Italian, German, French, Spanish, Portuguese, Ukrainian, Korean,
  Japanese and Simplified Chinese (the last three need a CJK font, such as
  `noto_sans_cjk_jp`).

Unless the settings ask for more, videos are played small enough for slow
machines: 360p on 32-bit ARM such as the Toshiba AC100, up to 720p elsewhere.

![The latest videos of a channel](screenshots/channel.png)

## Installing

The [releases](https://github.com/atomozero/Cinema/releases) have packages for
x86_64 and 32-bit ARM:

```sh
pkgman install cinema-1.2.1-1-x86_64.hpkg
```

On x86_64 this brings Python, the yt-dlp module and FFmpeg 6 from HaikuPorts
too.

On ARM, where HaikuPorts has no yt-dlp package, put the single-file release of
yt-dlp in the `PATH`:

```sh
mkdir -p ~/config/non-packaged/bin
curl -L -o ~/config/non-packaged/bin/yt-dlp \
	https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp
chmod +x ~/config/non-packaged/bin/yt-dlp
```

YouTube changes often: when videos stop being found, a newer yt-dlp usually
helps (`pkgman update`, or the same `curl` again).

## Needs

- `python3` and yt-dlp, either the Python module (`yt_dlp_python3.14`,
  matching your Python) or the single-file `yt-dlp` in the `PATH`.
- `curl`, for the pictures.
- The FFmpeg libraries, for live TV: `ffmpeg6` on x86_64, `ffmpeg` on ARM.
  The `ffmpeg` command too, for YouTube above 360p and for the length and the
  picture of your own videos.

yt-dlp is loaded once, by `cinema-helper.py`, which Cinema starts and talks to.

## Building

```sh
make
```

It links the FFmpeg libraries: on x86_64, install `ffmpeg6_devel` first.

`packaging/build-hpkg.sh`, run on Haiku or RenkuOS, builds the package of the
machine's architecture. Otherwise, put `objects/Cinema` and `helper/cinema-helper.py` together in a folder,
for example `~/config/non-packaged/apps/Cinema/`, and the catalogs of
`objects/catalogs/` in
`~/config/non-packaged/data/locale/catalogs/x-vnd.atomozero-Cinema/`.

To translate: `make catkeys` writes `locales/en.catkeys`, and
`tools/update-catkeys.py <language>` brings a translation up to date with it.

## Credits

This product uses the TMDB API but is not endorsed or certified by TMDB.

Live TV is played after the FFmpeg player of
[R Television](https://github.com/rainygirl/rtelevision), by Lee JunHaeng (MIT
license), and its channels come from the lists of
[iptv-org](https://github.com/iptv-org/iptv).

## License

MIT, see [LICENSE](LICENSE).

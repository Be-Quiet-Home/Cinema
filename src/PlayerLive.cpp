/*
 * Copyright 2026, Andrea Bernardi
 * Copyright 2026, Lee JunHaeng
 * SPDX-License-Identifier: MIT
 *
 * Live streams played with FFmpeg rather than the Media Kit, after
 * FFmpegMediaPlayer.cpp of R Television (https://github.com/rainygirl/
 * rtelevision, MIT license): the choice of the picture of an HLS stream
 * a slow processor keeps up with, the streams not played left alone, the
 * loop filter of H.264 skipped on such a processor and the frames nothing
 * refers to while it is behind, the pictures decoded only near their time,
 * the holes and the jumps of the sound.
 */


#include "Player.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <stdlib.h>
#include <string.h>

#include <Autolock.h>
#include <Bitmap.h>
#include <SoundPlayer.h>

#include <algorithm>
#include <deque>

#include "LiveAddress.h"
#include "Messages.h"


#if defined(__arm__) && !defined(__aarch64__)
	// a Cortex-A9 without NEON, as the Toshiba AC100's, decodes H.264 in C
static const bool kSlowProcessor = true;
#else
static const bool kSlowProcessor = false;
#endif

static const bigtime_t kSoundAhead = 4000000;
	// the sound decoded ahead: segments of HLS come in bursts
static const int64 kDecodeAhead = 500000;
	// pictures decoded this much before their time, for the reordering
static const int64 kMaxQueuedVideo = 8000000;
	// then decoded all the same
static const size_t kMaxDecodedFrames = 12;
static const int64 kLateMicros = 60000;
	// a picture this late counts as late


int
Player::_LiveInterrupt(void* data)
{
	return ((Player*)data)->fQuit ? 1 : 0;
}


static AVCodecContext*
open_decoder(AVFormatContext* format, int index, bool video)
{
	if (index < 0)
		return NULL;
	AVCodecParameters* parameters = format->streams[index]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(parameters->codec_id);
	if (codec == NULL)
		return NULL;
	AVCodecContext* decoder = avcodec_alloc_context3(codec);
	if (decoder == NULL)
		return NULL;
	avcodec_parameters_to_context(decoder, parameters);
	decoder->thread_count = 0;
	if (video) {
		decoder->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
		if (kSlowProcessor)
			decoder->flags2 |= AV_CODEC_FLAG2_FAST;
	}
	if (avcodec_open2(decoder, codec, NULL) < 0) {
		avcodec_free_context(&decoder);
		return NULL;
	}
	return decoder;
}


/*!	The bitmaps the pictures go to: YCbCr422 for the overlay when it is
	free, which scales them by itself, or 32 bits RGB the view draws.
*/
bool
Player::_SetUpBitmaps(int32 width, int32 height)
{
	if (width <= 0 || height <= 0)
		return false;
	BRect bounds(0, 0, width - 1, height - 1);
	BAutolock locker(fFrameLock);
	BBitmap* overlay = new BBitmap(bounds, B_BITMAP_WILL_OVERLAY
		| B_BITMAP_RESERVE_OVERLAY_CHANNEL, B_YCbCr422);
	if (overlay->InitCheck() == B_OK) {
		fOverlay = overlay;
		fFront = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_YCbCr422);
		fBack = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_YCbCr422);
		fLivePixels = AV_PIX_FMT_YUYV422;
	} else {
		delete overlay;
		fFront = new BBitmap(bounds, 0, B_RGB32);
		fBack = new BBitmap(bounds, 0, B_RGB32);
		fLivePixels = AV_PIX_FMT_BGRA;
	}
	return fFront->InitCheck() == B_OK && fBack->InitCheck() == B_OK;
}


void
Player::_OpenLive()
{
	LiveAddress live;
	if (!LiveAddress::Parse(fAddress.String(), live)) {
		_Failed();
		return;
	}
	static bool sNetwork = false;
	if (!sNetwork) {
		avformat_network_init();
		av_log_set_level(AV_LOG_ERROR);
		sNetwork = true;
	}

	// twice: opening reads a segment of every size of the stream, and one
	// of the many connections may fail for a moment
	AVFormatContext* format = NULL;
	for (int attempt = 0; attempt < 2 && format == NULL && !fQuit;
			attempt++) {
		format = avformat_alloc_context();
		if (format == NULL)
			break;
		format->interrupt_callback.callback = &_LiveInterrupt;
		format->interrupt_callback.opaque = this;
		AVDictionary* options = NULL;
		av_dict_set(&options, "user_agent", live.agent.IsEmpty()
			? "Cinema (Haiku)" : live.agent.String(), 0);
		if (!live.referrer.IsEmpty())
			av_dict_set(&options, "referer", live.referrer.String(), 0);
		av_dict_set(&options, "rw_timeout", "15000000", 0);
		av_dict_set(&options, "reconnect", "1", 0);
		av_dict_set(&options, "reconnect_streamed", "1", 0);
		int status = avformat_open_input(&format, live.url.String(), NULL,
			&options);
		av_dict_free(&options);
		// the context is gone with a failure
		if (status < 0)
			format = NULL;
		else if (avformat_find_stream_info(format, NULL) < 0)
			avformat_close_input(&format);
	}
	if (format == NULL || fQuit) {
		if (format != NULL)
			avformat_close_input(&format);
		if (!fQuit)
			_Failed();
		return;
	}
	fLiveFormat = format;

	fLiveVideoIndex = _LiveVideoStream(live.height);
	fLiveAudioIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1,
		fLiveVideoIndex, NULL, 0);
	if (fLiveAudioIndex < 0)
		fLiveAudioIndex = -1;
	// an HLS stream shows all its sizes at once, and every one not left
	// alone would be downloaded all along
	for (unsigned i = 0; i < format->nb_streams; i++) {
		format->streams[i]->discard = (int)i == fLiveVideoIndex
			|| (int)i == fLiveAudioIndex ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
	}
	fLiveVideo = open_decoder(format, fLiveVideoIndex, true);
	if (fLiveVideo == NULL)
		fLiveVideoIndex = -1;
	fLiveAudio = open_decoder(format, fLiveAudioIndex, false);
	if (fLiveAudio == NULL)
		fLiveAudioIndex = -1;
	fLiveLate = fLiveOnTime = 0;
	_LiveSkip(kSlowProcessor ? 1 : 0);

	// the pictures
	if (fLiveVideo != NULL && !_SetUpBitmaps(fLiveVideo->width & ~1,
			fLiveVideo->height)) {
		avcodec_free_context(&fLiveVideo);
		fLiveVideoIndex = -1;
	}

	// the sound, as 16 bit samples of the machine's order, two channels
	// at most
	if (fLiveAudio != NULL) {
		int rate = fLiveAudio->sample_rate;
		int channels = std::min(2, fLiveAudio->ch_layout.nb_channels);
		if (rate > 0 && channels > 0) {
			AVChannelLayout layout;
			av_channel_layout_default(&layout, channels);
			swr_alloc_set_opts2(&fLiveResampler, &layout, AV_SAMPLE_FMT_S16,
				rate, &fLiveAudio->ch_layout, fLiveAudio->sample_fmt,
				fLiveAudio->sample_rate, 0, NULL);
			av_channel_layout_uninit(&layout);
			if (fLiveResampler != NULL && swr_init(fLiveResampler) < 0)
				swr_free(&fLiveResampler);
		}
		if (fLiveResampler != NULL) {
			fSound = media_raw_audio_format::wildcard;
			fSound.frame_rate = rate;
			fSound.channel_count = channels;
			fSound.format = media_raw_audio_format::B_AUDIO_SHORT;
			fSound.byte_order = B_MEDIA_HOST_ENDIAN;
			fSound.buffer_size = 1024 * channels * 2;
			fFrameSize = channels * 2;
			fRing.resize((size_t)((int64)rate * kSoundAhead / 1000000)
				* fFrameSize);
			fRingRead = fRingFill = 0;
			fSoundPlayer = new BSoundPlayer(&fSound, "Cinema", &_PlayBuffer,
				NULL, this);
			if (fSoundPlayer->InitCheck() != B_OK) {
				delete fSoundPlayer;
				fSoundPlayer = NULL;
			} else {
				fSoundPlayer->SetVolume(fVolume);
				fLatency = fSoundPlayer->Latency();
			}
		}
		if (fSoundPlayer == NULL) {
			if (fLiveResampler != NULL)
				swr_free(&fLiveResampler);
			avcodec_free_context(&fLiveAudio);
			fLiveAudioIndex = -1;
		}
	}

	if (fLiveVideoIndex < 0 && fLiveAudioIndex < 0) {
		_Failed();
		return;
	}

	fVideoEnded = fLiveVideoIndex < 0;
	fAudioEnded = fLiveAudioIndex < 0;
	fClockBase = 0;
	fPlayed = 0;
	fLastFill = 0;
	fClockStart = system_time();
	fLiveBase = AV_NOPTS_VALUE;
	fLiveSoundEnd = AV_NOPTS_VALUE;
	fDuration = 0;
	fShowNext = true;
	fOpened = true;

	BMessage opened(kMsgPlayerOpened);
	opened.AddInt64("duration", 0);
	opened.AddBool("overlay", fOverlay != NULL);
	if (fFront != NULL) {
		opened.AddInt32("width", fFront->Bounds().IntegerWidth() + 1);
		opened.AddInt32("height", fFront->Bounds().IntegerHeight() + 1);
	}
	fTarget.SendMessage(&opened);

	if (fQuit)
		return;
	fVideoThread = spawn_thread(&_LiveEntry, "player live", B_DISPLAY_PRIORITY,
		this);
	resume_thread(fVideoThread);
	if (fSoundPlayer != NULL) {
		fSoundPlayer->Start();
		fSoundPlayer->SetHasData(true);
	}
}


void
Player::_CloseLive()
{
	if (fLiveScaler != NULL)
		sws_freeContext(fLiveScaler);
	fLiveScaler = NULL;
	if (fLiveResampler != NULL)
		swr_free(&fLiveResampler);
	if (fLiveAudio != NULL)
		avcodec_free_context(&fLiveAudio);
	if (fLiveVideo != NULL)
		avcodec_free_context(&fLiveVideo);
	if (fLiveFormat != NULL)
		avformat_close_input(&fLiveFormat);
	fLiveVideoIndex = fLiveAudioIndex = -1;
	fLivePcm.clear();
}


/*!	The picture to play. With several sizes (adaptive HLS), the tallest
	not taller than maxHeight, or the smallest; the lowest rate between
	two of a size. maxHeight 0: the one FFmpeg finds best.
*/
int
Player::_LiveVideoStream(int32 maxHeight)
{
	AVFormatContext* format = fLiveFormat;
	int best = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,
		0);
	if (maxHeight <= 0 || best < 0)
		return best;

	int chosen = -1;
	int chosenHeight = 0;
	int64 chosenRate = 0;
	int candidates = 0;
	for (unsigned i = 0; i < format->nb_streams; i++) {
		const AVStream* stream = format->streams[i];
		const AVCodecParameters* parameters = stream->codecpar;
		if (parameters->codec_type != AVMEDIA_TYPE_VIDEO
			|| parameters->height <= 0
			|| (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0
			|| avcodec_find_decoder(parameters->codec_id) == NULL)
			continue;
		candidates++;
		int64 rate = parameters->bit_rate;
		const AVDictionaryEntry* entry = av_dict_get(stream->metadata,
			"variant_bitrate", NULL, 0);
		if (entry != NULL)
			rate = strtoll(entry->value, NULL, 10);
		int height = parameters->height;
		bool better;
		if (chosen < 0)
			better = true;
		else {
			bool fits = height <= maxHeight;
			bool chosenFits = chosenHeight <= maxHeight;
			if (fits != chosenFits)
				better = fits;
			else if (height != chosenHeight)
				better = fits ? height > chosenHeight : height < chosenHeight;
			else
				better = rate > 0 && (chosenRate <= 0 || rate < chosenRate);
		}
		if (better) {
			chosen = i;
			chosenHeight = height;
			chosenRate = rate;
		}
	}
	return candidates > 1 && chosen >= 0 ? chosen : best;
}


/*!	0: all decoded. 1: without the loop filter of H.264, a good part of
	the time for a little blockiness (on a slow processor, always). 2: the
	frames nothing refers to skipped too, while it is behind.
*/
void
Player::_LiveSkip(int level)
{
	if (fLiveVideo == NULL)
		return;
	fLiveSkipLevel = std::max(0, std::min(2, level));
	fLiveVideo->skip_loop_filter = fLiveSkipLevel >= 1
		? AVDISCARD_ALL : AVDISCARD_DEFAULT;
	fLiveVideo->skip_frame = fLiveSkipLevel >= 2
		? AVDISCARD_NONREF : AVDISCARD_DEFAULT;
}


/*!	Skips more after a few late pictures in a row, less after about five
	seconds on time.
*/
void
Player::_LiveLateness(bool late)
{
	int base = kSlowProcessor ? 1 : 0;
	if (late) {
		fLiveOnTime = 0;
		if (++fLiveLate >= 4 && fLiveSkipLevel < 2) {
			_LiveSkip(2);
			fLiveLate = 0;
		}
	} else {
		fLiveLate = 0;
		if (++fLiveOnTime >= 125 && fLiveSkipLevel > base) {
			_LiveSkip(fLiveSkipLevel - 1);
			fLiveOnTime = 0;
		}
	}
}


int64
Player::_LiveFrameMicros(int index, const AVFrame* frame) const
{
	int64 pts = frame->best_effort_timestamp;
	if (pts == AV_NOPTS_VALUE)
		return AV_NOPTS_VALUE;
	return av_rescale_q(pts, fLiveFormat->streams[index]->time_base,
		AVRational{ 1, 1000000 });
}


/*!	Where the clock is, from fLiveBase: the sound played, or the system
	time without sound.
*/
int64
Player::_LiveClock()
{
	return _Clock();
}


bool
Player::_LiveRender(AVFrame* frame)
{
	if (fBack == NULL)
		return true;
	int width = fBack->Bounds().IntegerWidth() + 1;
	int height = fBack->Bounds().IntegerHeight() + 1;
	fLiveScaler = sws_getCachedContext(fLiveScaler, frame->width,
		frame->height, (AVPixelFormat)frame->format, width, height,
		(AVPixelFormat)fLivePixels,
		kSlowProcessor ? SWS_FAST_BILINEAR : SWS_BILINEAR, NULL, NULL, NULL);
	if (fLiveScaler == NULL)
		return false;
	uint8_t* planes[4] = { (uint8_t*)fBack->Bits(), NULL, NULL, NULL };
	int strides[4] = { (int)fBack->BytesPerRow(), 0, 0, 0 };
	sws_scale(fLiveScaler, frame->data, frame->linesize, 0, frame->height,
		planes, strides);
	_ShowFrame();
	fShowNext = false;
	if (!fFramePending.exchange(true))
		fTarget.SendMessage(kMsgPlayerFrame);
	return true;
}


/*!	Into the ring the sound player takes from; the caller made room. */
void
Player::_LiveQueueSound(const void* data, size_t bytes)
{
	BAutolock ring(fRingLock);
	bytes = std::min(bytes, fRing.size() - fRingFill);
	size_t write = (fRingRead + fRingFill) % fRing.size();
	size_t first = std::min(bytes, fRing.size() - write);
	memcpy(&fRing[write], data, first);
	memcpy(&fRing[0], (const uint8*)data + first, bytes - first);
	fRingFill += bytes;
}


/*!	The clock counts the sound played, so sound that never comes, a hole
	in the stream, would leave it behind for good: short holes are filled
	with silence, a jump of the times is followed.
*/
void
Player::_LiveAlignSound(int64 micros)
{
	fLivePcm.clear();
	if (micros == AV_NOPTS_VALUE || fLiveSoundEnd == AV_NOPTS_VALUE)
		return;
	int64 gap = micros - fLiveSoundEnd;
	if (gap > 2000000 || gap < -2000000) {
		fLiveBase += gap;
		fLiveSoundEnd = micros;
		return;
	}
	if (gap < 20000)
		return;
	int64 samples = gap * (int64)fSound.frame_rate / 1000000;
	fLivePcm.assign(samples * fFrameSize, 0);
	fLiveSoundEnd += samples * 1000000 / (int64)fSound.frame_rate;
}


/*!	The samples of a frame of sound, after the silence _LiveAlignSound()
	may have put in fLivePcm.
*/
void
Player::_LivePushSound(AVFrame* frame)
{
	int most = swr_get_out_samples(fLiveResampler, frame->nb_samples);
	if (most <= 0)
		return;
	size_t before = fLivePcm.size();
	fLivePcm.resize(before + most * fFrameSize);
	uint8_t* planes[1] = { fLivePcm.data() + before };
	int written = swr_convert(fLiveResampler, planes, most,
		(const uint8_t**)frame->data, frame->nb_samples);
	if (written <= 0) {
		fLivePcm.resize(before);
		return;
	}
	fLivePcm.resize(before + written * fFrameSize);
	if (fLiveSoundEnd != AV_NOPTS_VALUE)
		fLiveSoundEnd += (int64)written * 1000000 / (int64)fSound.frame_rate;
}


status_t
Player::_LiveEntry(void* data)
{
	((Player*)data)->_LiveLoop();
	return B_OK;
}


/*!	Reads the stream: the sound is decoded as it comes, the pictures wait
	compressed until the clock is near them. A stream may carry its
	pictures a second ahead of their sound; decoding them at once would
	wait on a clock only the sound further on moves.
*/
void
Player::_LiveLoop()
{
	AVFormatContext* format = fLiveFormat;
	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	std::deque<AVPacket*> packets;
	std::deque<AVFrame*> frames;
	bool started = false;
	bool failed = false;
	const int video = fLiveVideoIndex;
	const int audio = fLiveAudioIndex;

	auto packetMicros = [&](const AVPacket* p) -> int64 {
		int64 time = p->dts != AV_NOPTS_VALUE ? p->dts : p->pts;
		if (time == AV_NOPTS_VALUE)
			return AV_NOPTS_VALUE;
		return av_rescale_q(time, format->streams[video]->time_base,
			AVRational{ 1, 1000000 });
	};
	auto queuedSpan = [&]() -> int64 {
		if (packets.size() < 2)
			return 0;
		int64 first = packetMicros(packets.front());
		int64 last = packetMicros(packets.back());
		return first == AV_NOPTS_VALUE || last == AV_NOPTS_VALUE
			? 0 : last - first;
	};
	// when a picture is due on the clock, or AV_NOPTS_VALUE if not known
	auto dueAt = [&](int64 micros) -> int64 {
		if (micros == AV_NOPTS_VALUE || fLiveBase == AV_NOPTS_VALUE)
			return AV_NOPTS_VALUE;
		return micros - fLiveBase;
	};

	// shows the pictures that are due; with wait, holds the first until
	// it is. A late one is dropped unconverted when the next is due too.
	auto showFrames = [&](bool wait) -> bool {
		while (!frames.empty() && !fQuit) {
			AVFrame* next = frames.front();
			int64 at = dueAt(_LiveFrameMicros(video, next));
			int64 now = _LiveClock();
			if (at != AV_NOPTS_VALUE && at > now) {
				if (!wait)
					break;
				for (int guard = 0; guard < 250 && !fQuit; guard++) {
					if (fPaused) {
						guard--;
						snooze(20000);
						continue;
					}
					int64 delay = at - _LiveClock();
					if (delay <= 0)
						break;
					snooze(std::min(delay, (int64)20000));
				}
				wait = false;
				now = _LiveClock();
			}
			frames.pop_front();
			bool known = at != AV_NOPTS_VALUE;
			bool late = known && now - at > kLateMicros;
			bool drop = false;
			if (late && started && !frames.empty()) {
				int64 after = dueAt(_LiveFrameMicros(video, frames.front()));
				drop = after != AV_NOPTS_VALUE && after <= now;
			}
			if (known)
				_LiveLateness(late);
			bool ok = drop || _LiveRender(next);
			av_frame_free(&next);
			if (!ok)
				return false;
			if (!drop)
				started = true;
		}
		return true;
	};

	// decodes the pictures near the clock, or any once the queue is long
	auto pumpVideo = [&](bool all) -> bool {
		while (!packets.empty() && frames.size() < kMaxDecodedFrames
			&& !fQuit) {
			AVPacket* next = packets.front();
			if (!all) {
				int64 at = dueAt(packetMicros(next));
				bool near = at == AV_NOPTS_VALUE
					|| at <= _LiveClock() + kDecodeAhead;
				if (!near && queuedSpan() < kMaxQueuedVideo)
					break;
			}
			packets.pop_front();
			int status = avcodec_send_packet(fLiveVideo, next);
			av_packet_free(&next);
			if (status < 0)
				continue;
			while (avcodec_receive_frame(fLiveVideo, frame) == 0) {
				int64 micros = _LiveFrameMicros(video, frame);
				if (audio < 0 && fLiveBase == AV_NOPTS_VALUE
					&& micros != AV_NOPTS_VALUE) {
					// without sound the clock starts with the pictures
					fLiveBase = micros;
					fClockBase = 0;
					fClockStart = system_time();
				}
				if (AVFrame* copy = av_frame_clone(frame))
					frames.push_back(copy);
				av_frame_unref(frame);
			}
			if (!showFrames(false))
				return false;
		}
		return true;
	};

	// the sound waiting for room in the ring, the pictures shown meanwhile
	auto queueSound = [&]() -> bool {
		size_t offset = 0;
		while (offset < fLivePcm.size() && !fQuit) {
			size_t room;
			{
				BAutolock ring(fRingLock);
				room = fRing.size() - fRingFill;
			}
			room -= room % fFrameSize;
			if (room == 0) {
				if (video >= 0 && (!pumpVideo(false) || !showFrames(false)))
					return false;
				snooze(5000);
				continue;
			}
			size_t bytes = std::min(room, fLivePcm.size() - offset);
			_LiveQueueSound(fLivePcm.data() + offset, bytes);
			offset += bytes;
		}
		fLivePcm.clear();
		return true;
	};

	while (!fQuit && !failed) {
		if (fPaused) {
			snooze(40000);
			continue;
		}
		if (av_read_frame(format, packet) < 0) {
			// the end: what is still there
			while (!fQuit && !failed && (!packets.empty() || !frames.empty())) {
				if (!pumpVideo(true) || !showFrames(true))
					failed = true;
			}
			break;
		}

		if (packet->stream_index == video && video >= 0) {
			if (AVPacket* copy = av_packet_clone(packet))
				packets.push_back(copy);
			av_packet_unref(packet);
		} else if (packet->stream_index == audio && audio >= 0) {
			int status = avcodec_send_packet(fLiveAudio, packet);
			av_packet_unref(packet);
			while (status >= 0 && !fQuit
				&& avcodec_receive_frame(fLiveAudio, frame) == 0) {
				int64 micros = _LiveFrameMicros(audio, frame);
				if (fLiveBase == AV_NOPTS_VALUE && micros != AV_NOPTS_VALUE) {
					fLiveBase = micros;
					fLiveSoundEnd = micros;
				}
				_LiveAlignSound(micros);
				_LivePushSound(frame);
				av_frame_unref(frame);
				if (!queueSound()) {
					failed = true;
					break;
				}
			}
		} else
			av_packet_unref(packet);

		if (video >= 0 && (!pumpVideo(false) || !showFrames(false)))
			failed = true;

		// without sound nothing else paces the reading
		if (!failed && video >= 0 && frames.size() >= kMaxDecodedFrames
			&& queuedSpan() >= kMaxQueuedVideo && !showFrames(true))
			failed = true;
	}

	for (AVPacket* queued : packets)
		av_packet_free(&queued);
	for (AVFrame* queued : frames)
		av_frame_free(&queued);
	av_frame_free(&frame);
	av_packet_free(&packet);

	if (fQuit)
		return;
	// the sound queued plays out first
	while (!fQuit) {
		{
			BAutolock ring(fRingLock);
			if (fRingFill == 0 || fSoundPlayer == NULL)
				break;
		}
		snooze(20000);
	}
	if (!fQuit && !fEndSent.exchange(true))
		fTarget.SendMessage(started || !failed ? kMsgPlayerEnded
			: kMsgPlayerError);
}

/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Player.h"

#include <stdlib.h>
#include <string.h>

#include <Autolock.h>
#include <Bitmap.h>
#include <Entry.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>
#include <Url.h>

#include <algorithm>

#include "LiveAddress.h"
#include "Messages.h"


static const bigtime_t kLate = 120000;
	// a picture later than this is not shown
static const bigtime_t kRingTime = 2000000;
	// the sound decoded ahead
static const bigtime_t kStill = 250000;
	// a picture too late is shown all the same when none was for this
	// long: on a machine too slow for the video, it goes on by jerks
	// instead of standing still


Player::Player(const BMessenger& target)
	:
	fTarget(target),
	fStartAt(0),
	fRelay(false),
	fRelayDuration(0),
	fFirstTime(-1),
	fLive(false),
	fLiveFormat(NULL),
	fLiveVideo(NULL),
	fLiveAudio(NULL),
	fLiveResampler(NULL),
	fLiveScaler(NULL),
	fLiveVideoIndex(-1),
	fLiveAudioIndex(-1),
	fLiveSkipLevel(0),
	fLiveLate(0),
	fLiveOnTime(0),
	fLivePixels(-1),
	fLiveBase(0),
	fLiveSoundEnd(0),
	fFile(NULL),
	fVideoTrack(NULL),
	fAudioTrack(NULL),
	fVideoLock("player video"),
	fAudioLock("player audio"),
	fDuration(0),
	fFront(NULL),
	fBack(NULL),
	fOverlay(NULL),
	fFrameLock("player frame"),
	fHasFrame(false),
	fSoundPlayer(NULL),
	fFrameSize(0),
	fLatency(0),
	fLastFill(0),
	fFillTime(0),
	fRingRead(0),
	fRingFill(0),
	fRingLock("player ring"),
	fClockBase(0),
	fPlayed(0),
	fClockStart(0),
	fPausedClock(0),
	fPaused(false),
	fQuit(false),
	fOpened(false),
	fShowNext(false),
	fGeneration(0),
	fVideoEnded(false),
	fAudioEnded(false),
	fEndSent(false),
	fFramePending(false),
	fVolume(1.0f),
	fOpenThread(-1),
	fVideoThread(-1),
	fAudioThread(-1)
{
}


Player::~Player()
{
	Close();
}


void
Player::Open(const char* address, bigtime_t startAt)
{
	Close();
	fAddress = address;
	fStartAt = startAt;
	// the relay: how long it lasts is the last part of the address
	fLive = LiveAddress::Is(address);
	if (fLive)
		fStartAt = 0;
	fRelay = fAddress.StartsWith("http://127.0.0.1:");
	fRelayDuration = 0;
	fFirstTime = -1;
	if (fRelay) {
		int32 slash = fAddress.FindLast('/');
		fRelayDuration = (bigtime_t)atoll(fAddress.String() + slash + 1)
			* 1000000;
		if (fRelayDuration <= 0)
			fStartAt = 0;
	}
	fQuit = false;
	fPaused = false;
	fEndSent = false;
	fOpenThread = spawn_thread(&_OpenEntry, "player open", B_NORMAL_PRIORITY,
		this);
	if (fOpenThread >= 0)
		resume_thread(fOpenThread);
}


void
Player::Close()
{
	fQuit = true;
	status_t status;
	if (fOpenThread >= 0) {
		wait_for_thread(fOpenThread, &status);
		fOpenThread = -1;
	}
	if (fSoundPlayer != NULL) {
		fSoundPlayer->Stop();
		delete fSoundPlayer;
		fSoundPlayer = NULL;
	}
	if (fVideoThread >= 0) {
		wait_for_thread(fVideoThread, &status);
		fVideoThread = -1;
	}
	if (fAudioThread >= 0) {
		wait_for_thread(fAudioThread, &status);
		fAudioThread = -1;
	}
	_CloseLive();
	if (fFile != NULL) {
		if (fVideoTrack != NULL)
			fFile->ReleaseTrack(fVideoTrack);
		if (fAudioTrack != NULL)
			fFile->ReleaseTrack(fAudioTrack);
		delete fFile;
	}
	fFile = NULL;
	fVideoTrack = NULL;
	fAudioTrack = NULL;

	BAutolock locker(fFrameLock);
	delete fFront;
	delete fBack;
	delete fOverlay;
	fFront = NULL;
	fBack = NULL;
	fOverlay = NULL;
	fHasFrame = false;
	fOpened = false;
	fDuration = 0;
	fRing.clear();
	fRingRead = fRingFill = 0;
}


status_t
Player::_OpenEntry(void* data)
{
	((Player*)data)->_Open();
	return B_OK;
}


void
Player::_Open()
{
	if (fLive) {
		_OpenLive();
		return;
	}
	if (fAddress.ByteAt(0) == '/') {
		entry_ref ref;
		if (get_ref_for_path(fAddress.String(), &ref) == B_OK)
			fFile = new BMediaFile(&ref);
	} else if (fRelay) {
		// from where it starts, which the times of the stream count from
		BString address(fAddress);
		if (fStartAt > 0)
			address << "?t=" << (double)fStartAt / 1000000;
		fFile = new BMediaFile(BUrl(address.String(), true));
	} else
		fFile = new BMediaFile(BUrl(fAddress.String(), true));
	if (fFile == NULL || fFile->InitCheck() != B_OK) {
		_Failed();
		return;
	}
	if (fQuit)
		return;

	for (int32 i = 0; i < fFile->CountTracks(); i++) {
		BMediaTrack* track = fFile->TrackAt(i);
		if (track == NULL)
			continue;
		media_format encoded;
		track->EncodedFormat(&encoded);
		if (encoded.IsVideo() && fVideoTrack == NULL)
			fVideoTrack = track;
		else if (encoded.IsAudio() && fAudioTrack == NULL)
			fAudioTrack = track;
		else
			fFile->ReleaseTrack(track);
	}

	if (fVideoTrack != NULL && !_SetUpVideo()) {
		fFile->ReleaseTrack(fVideoTrack);
		fVideoTrack = NULL;
	}

	// the sound, as 16 bit samples of the machine's order
	if (fAudioTrack != NULL) {
		media_format format;
		format.type = B_MEDIA_RAW_AUDIO;
		format.u.raw_audio = media_raw_audio_format::wildcard;
		format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
		format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
		if (fAudioTrack->DecodedFormat(&format) == B_OK
			&& format.u.raw_audio.channel_count > 0
			&& format.u.raw_audio.frame_rate > 0) {
			fSound = format.u.raw_audio;
			fFrameSize = (fSound.format & 0x0f) * fSound.channel_count;
			fRing.resize((size_t)(fSound.frame_rate * kRingTime / 1000000)
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
			fDuration = std::max(fDuration, fAudioTrack->Duration());
		}
		if (fSoundPlayer == NULL) {
			fFile->ReleaseTrack(fAudioTrack);
			fAudioTrack = NULL;
		}
	}

	if (fVideoTrack == NULL && fAudioTrack == NULL) {
		_Failed();
		return;
	}

	fVideoEnded = fVideoTrack == NULL;
	fAudioEnded = fAudioTrack == NULL;
	fClockBase = 0;
	fPlayed = 0;
	fClockStart = system_time();
	if (fRelay) {
		fDuration = fRelayDuration;
		fClockBase = fStartAt;
		fPausedClock = fStartAt;
	} else if (fStartAt > 0)
		_SeekTracks(fStartAt);
	fShowNext = true;
	fOpened = true;

	BMessage opened(kMsgPlayerOpened);
	opened.AddInt64("duration", fDuration);
	opened.AddBool("overlay", fOverlay != NULL);
	if (fFront != NULL) {
		opened.AddInt32("width", fFront->Bounds().IntegerWidth() + 1);
		opened.AddInt32("height", fFront->Bounds().IntegerHeight() + 1);
	}
	fTarget.SendMessage(&opened);

	if (fQuit)
		return;
	if (fVideoTrack != NULL) {
		fVideoThread = spawn_thread(&_VideoEntry, "player pictures",
			B_DISPLAY_PRIORITY, this);
		resume_thread(fVideoThread);
	}
	if (fAudioTrack != NULL) {
		fAudioThread = spawn_thread(&_AudioEntry, "player sound",
			B_URGENT_DISPLAY_PRIORITY, this);
		resume_thread(fAudioThread);
		fSoundPlayer->Start();
		fSoundPlayer->SetHasData(true);
	}
}


void
Player::_Failed()
{
	fTarget.SendMessage(kMsgPlayerError);
}


bigtime_t
Player::_Clock() const
{
	if (fSoundPlayer != NULL) {
		bigtime_t played = fPlayed * 1000000LL
			/ (int64)std::max(1.0f, fSound.frame_rate);
		// between two buffers too, so that the pictures go on evenly
		if (!fPaused && fLastFill > 0)
			played += std::min(system_time() - fLastFill, fFillTime);
		return fClockBase + std::max((bigtime_t)0, played - fLatency);
	}
	if (fPaused)
		return fPausedClock;
	return fClockBase + system_time() - fClockStart;
}


bigtime_t
Player::Position() const
{
	if (!fOpened)
		return fStartAt;
	bigtime_t position = _Clock();
	if (fDuration > 0)
		position = std::min(position, fDuration);
	return std::max((bigtime_t)0, position);
}


void
Player::SetPaused(bool paused)
{
	if (paused == fPaused)
		return;
	if (fSoundPlayer == NULL) {
		if (paused)
			fPausedClock = _Clock();
		else {
			fClockBase = fPausedClock;
			fClockStart = system_time();
		}
	}
	fPaused = paused;
}


void
Player::SetVolume(float volume)
{
	fVolume = std::max(0.0f, std::min(1.0f, volume));
	if (fSoundPlayer != NULL)
		fSoundPlayer->SetVolume(fVolume);
}


/*!	Both tracks to time, or rather to the picture that starts there or
	before, so that the picture is whole and the sound goes with it.
*/
void
Player::_SeekTracks(bigtime_t time)
{
	BAutolock video(fVideoLock);
	BAutolock audio(fAudioLock);
	fGeneration++;
	time = std::max((bigtime_t)0, time);
	if (fDuration > 0)
		time = std::min(time, fDuration - 1000000);
	if (fVideoTrack != NULL)
		fVideoTrack->SeekToTime(&time, B_MEDIA_SEEK_CLOSEST_BACKWARD);
	if (fAudioTrack != NULL) {
		bigtime_t soundTime = time;
		fAudioTrack->SeekToTime(&soundTime);
	}
	{
		BAutolock ring(fRingLock);
		fRingRead = fRingFill = 0;
	}
	fClockBase = time;
	fPlayed = 0;
	fLastFill = 0;
	fClockStart = system_time();
	fPausedClock = time;
	fVideoEnded = fVideoTrack == NULL;
	fAudioEnded = fAudioTrack == NULL;
	fEndSent = false;
	fShowNext = true;
}


void
Player::Seek(bigtime_t time)
{
	if (!fOpened || fLive)
		return;
	if (fRelay) {
		if (fDuration <= 0)
			return;
		// opened again from there, by the window: the bitmap of the
		// overlay goes with it
		time = std::max((bigtime_t)0, std::min(time, fDuration - 1000000));
		BMessage reopen(kMsgPlayerReopen);
		reopen.AddInt64("time", time);
		fTarget.SendMessage(&reopen);
		return;
	}
	_SeekTracks(time);
}


/*!	The time of a picture or a sound in the video: for a stream of the
	relay, which counts from where it starts as it likes, from fStartAt.
*/
bigtime_t
Player::_StreamTime(bigtime_t time)
{
	if (!fRelay)
		return time;
	bigtime_t first = -1;
	fFirstTime.compare_exchange_strong(first, time);
	return time - fFirstTime + fStartAt;
}


status_t
Player::_AudioEntry(void* data)
{
	((Player*)data)->_AudioLoop();
	return B_OK;
}


void
Player::_AudioLoop()
{
	size_t size = std::max((size_t)fSound.buffer_size, (size_t)65536);
	uint8* buffer = (uint8*)malloc(size * 2);
	if (buffer == NULL)
		return;

	while (!fQuit) {
		size_t room;
		{
			BAutolock ring(fRingLock);
			room = fRing.size() - fRingFill;
		}
		if (fAudioEnded || room < size) {
			snooze(10000);
			continue;
		}

		int32 generation;
		int64 frames = 0;
		media_header header;
		status_t status;
		{
			BAutolock locker(fAudioLock);
			generation = fGeneration;
			status = fAudioTrack->ReadFrames(buffer, &frames, &header);
		}
		if (status != B_OK) {
			// the end, or nothing more to read
			fAudioEnded = true;
			_CheckEnded();
			continue;
		}
		if (generation != fGeneration)
			continue;
		_StreamTime(header.start_time);

		BAutolock ring(fRingLock);
		size_t bytes = std::min((size_t)frames * fFrameSize,
			fRing.size() - fRingFill);
		size_t write = (fRingRead + fRingFill) % fRing.size();
		size_t first = std::min(bytes, fRing.size() - write);
		memcpy(&fRing[write], buffer, first);
		memcpy(&fRing[0], buffer + first, bytes - first);
		fRingFill += bytes;
	}
	free(buffer);
}


void
Player::_PlayBuffer(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	((Player*)cookie)->_FillSound(buffer, size);
}


void
Player::_FillSound(void* buffer, size_t size)
{
	uint8* out = (uint8*)buffer;
	if (fPaused || fRing.empty()) {
		memset(out, 0, size);
		return;
	}
	BAutolock ring(fRingLock);
	size_t bytes = std::min(size, fRingFill);
	bytes -= bytes % std::max((size_t)1, fFrameSize);
	size_t first = std::min(bytes, fRing.size() - fRingRead);
	memcpy(out, &fRing[fRingRead], first);
	memcpy(out + first, &fRing[0], bytes - first);
	fRingRead = (fRingRead + bytes) % fRing.size();
	fRingFill -= bytes;
	if (bytes < size)
		memset(out + bytes, 0, size - bytes);
	fPlayed += bytes / std::max((size_t)1, fFrameSize);
	fFillTime = (bigtime_t)(size / std::max((size_t)1, fFrameSize)) * 1000000LL
		/ (int64)std::max(1.0f, fSound.frame_rate);
	fLastFill = system_time();
	if (fRingFill == 0 && fAudioEnded)
		_CheckEnded();
}


status_t
Player::_VideoEntry(void* data)
{
	((Player*)data)->_VideoLoop();
	return B_OK;
}


void
Player::_VideoLoop()
{
	bigtime_t lastShown = system_time();
	while (!fQuit) {
		if (fVideoEnded || (fPaused && !fShowNext)) {
			snooze(10000);
			continue;
		}

		int32 generation;
		int64 frames = 1;
		media_header header;
		status_t status;
		{
			BAutolock locker(fVideoLock);
			generation = fGeneration;
			status = fVideoTrack->ReadFrames(fBack->Bits(), &frames, &header);
		}
		if (status != B_OK) {
			fVideoEnded = true;
			_CheckEnded();
			continue;
		}
		if (generation != fGeneration)
			continue;

		bigtime_t time = _StreamTime(header.start_time);
		bool show = fShowNext;
		if (!show) {
			// waits for the sound to get there
			while (!fQuit && generation == fGeneration && !fShowNext) {
				if (fPaused) {
					snooze(10000);
					continue;
				}
				bigtime_t wait = time - _Clock();
				if (wait <= 0)
					break;
				snooze(std::min(wait, (bigtime_t)10000));
			}
			if (fQuit || generation != fGeneration)
				continue;
			show = fShowNext || _Clock() - time < kLate
				|| system_time() - lastShown > kStill;
		}
		if (!show)
			continue;

		lastShown = system_time();
		_ShowFrame();
		fShowNext = false;
		if (!fFramePending.exchange(true))
			fTarget.SendMessage(kMsgPlayerFrame);
	}
}


void
Player::_CheckEnded()
{
	if (!fVideoEnded || !fAudioEnded || fEndSent)
		return;
	{
		BAutolock ring(fRingLock);
		if (fRingFill > 0)
			return;
	}
	if (!fEndSent.exchange(true))
		fTarget.SendMessage(kMsgPlayerEnded);
}


BBitmap*
Player::LockFrame()
{
	fFrameLock.Lock();
	fFramePending = false;
	return fHasFrame ? fFront : NULL;
}


void
Player::UnlockFrame()
{
	fFrameLock.Unlock();
}


/*!	The pictures in YCbCr422 for the overlay, when it is free: the screen
	scales them and turns them into colors by itself, which the AC100
	could not do on time. Otherwise in 32 bits RGB, which the view draws.
*/
bool
Player::_SetUpVideo()
{
	media_format format;
	format.type = B_MEDIA_RAW_VIDEO;
	format.u.raw_video = media_raw_video_format::wildcard;
	format.u.raw_video.display.format = B_YCbCr422;
	if (fVideoTrack->DecodedFormat(&format) == B_OK
		&& format.u.raw_video.display.format == B_YCbCr422) {
		const media_video_display_info& display = format.u.raw_video.display;
		BRect bounds(0, 0, display.line_width - 1, display.line_count - 1);
		BBitmap* overlay = new BBitmap(bounds, B_BITMAP_WILL_OVERLAY
			| B_BITMAP_RESERVE_OVERLAY_CHANNEL, B_YCbCr422);
		if (overlay->InitCheck() == B_OK) {
			BAutolock locker(fFrameLock);
			fOverlay = overlay;
			fFront = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_YCbCr422,
				display.bytes_per_row);
			fBack = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_YCbCr422,
				display.bytes_per_row);
			fDuration = std::max(fDuration, fVideoTrack->Duration());
			return true;
		}
		delete overlay;
	}

	format.type = B_MEDIA_RAW_VIDEO;
	format.u.raw_video = media_raw_video_format::wildcard;
	format.u.raw_video.display.format = B_RGB32;
	if (fVideoTrack->DecodedFormat(&format) != B_OK
		|| (format.u.raw_video.display.format != B_RGB32
			&& format.u.raw_video.display.format != B_RGBA32))
		return false;
	const media_video_display_info& display = format.u.raw_video.display;
	BRect bounds(0, 0, display.line_width - 1, display.line_count - 1);
	BAutolock locker(fFrameLock);
	fFront = new BBitmap(bounds, 0, B_RGB32, display.bytes_per_row);
	fBack = new BBitmap(bounds, 0, B_RGB32, display.bytes_per_row);
	fDuration = std::max(fDuration, fVideoTrack->Duration());
	return true;
}


/*!	The picture decoded becomes the one shown; with the overlay, it is
	copied where the screen takes it from.
*/
void
Player::_ShowFrame()
{
	BAutolock locker(fFrameLock);
	std::swap(fFront, fBack);
	fHasFrame = true;
	if (fOverlay == NULL || fOverlay->LockBits() != B_OK)
		return;
	const uint8* from = (const uint8*)fFront->Bits();
	uint8* to = (uint8*)fOverlay->Bits();
	int32 fromRow = fFront->BytesPerRow();
	int32 toRow = fOverlay->BytesPerRow();
	int32 row = std::min(fromRow, toRow);
	int32 rows = fFront->Bounds().IntegerHeight() + 1;
	for (int32 y = 0; y < rows; y++)
		memcpy(to + y * toRow, from + y * fromRow, row);
	fOverlay->UnlockBits();
}

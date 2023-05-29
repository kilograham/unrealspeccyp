/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2010 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
Copyright (C) 2023 Graham Sanderson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __TAPE_H__
#define __TAPE_H__

#ifndef NO_USE_TAPE
#include "../../std.h"
#include "../sound/device_sound.h"
#include "../../z80/z80.h"
#include "stream.h"

#pragma once

//#define USE_LEGACY_TAPE_COMPARISON
class eSpeccy;
namespace xZ80 { class eZ80_FastTape; }

class eTapePulseIterator {
public:
	// return -1 for none
	virtual int32_t next_pulse_length() = 0;
};

class eTapeBlockPulseIterator : public eTapePulseIterator {
public:
	static const int CACHE_SIZE = 32;
	eTapeBlockPulseIterator() { set_needs_reset(); }

	void set_needs_reset() {
		state = DONE;
	}

	inline void reset(struct stream *stream, dword size, dword pilot_t, dword s1_t,
					  dword s2_t, dword zero_t, dword one_t, dword pilot_len, dword pause,
					  byte last = 8) {
		this->stream = stream;
		this->size = this->remaining_bytes = size;
		this->pilot_t = pilot_t;
		this->s1_t = s1_t;
		this->s2_t = s2_t;
		this->zero_t = zero_t;
		this->one_t = one_t;
		this->pilot_len = pilot_len;
		this->pause = pause;
		this->last = last;
		this->state = RESET;
		recache();
	}
	virtual int32_t next_pulse_length() {
		while (true) {
			switch (state) {
				case RESET:
					if (pilot_len != (dword)-1) {
						counter = pilot_len;
						state = PILOT;
					} else {
						state = DATA;
						counter = 0;
					}
					break;
				case DONE:
					return -1;
				case PILOT:
					if (counter--) {
						return pilot_t;
					} else {
						state = S1;
					}
					break;
				case S1:
					state = S2;
					return s1_t;
				case S2:
					state = DATA;
					counter = 0;
					intra_byte = 0x180;
					return s2_t;
				case DATA:
					if (counter < size) {
						int32_t len = data_cache[data_cache_pos] & intra_byte ? one_t : zero_t;
						if (intra_byte & 0x100) {
							// need second half of pulse
							intra_byte = intra_byte & 0xff;
						} else
						{
							intra_byte >>= 1;
							if (counter == size -1) {
								if (intra_byte == (0x80 >> last)) {
									state = END_PAUSE;
								}
							}
							if (!intra_byte) {
								intra_byte = 0x180;
								counter++;
								data_cache_pos++;
								if (data_cache_pos == data_cache_size) {
									recache();
								}
							} else
							{
								intra_byte |= 0x100;
							}
						}
						return len;
					} else {
						state = END_PAUSE;
					}
					break;
				case END_PAUSE:
					state = DONE;
					if (pause)
					{
						return pause * 3500;
					}
					break;
				default:
					assert(false);
			}
		}
	}
protected:
	struct stream *stream;
	dword size;
	dword remaining_bytes;
	dword data_cache_size;
	dword data_cache_pos;
	dword pilot_t;
	dword s1_t;
	dword s2_t;
	dword zero_t;
	dword one_t;
	dword pilot_len;
	dword pause;
	byte data_cache[CACHE_SIZE];
	byte last;

	void recache() {
		data_cache_size = MIN(remaining_bytes, CACHE_SIZE);
		stream_read(stream, data_cache, data_cache_size, true);
		data_cache_pos = 0;
		remaining_bytes -= data_cache_size;
		assert(data_cache);
	}

	enum State {
		RESET,
		PILOT,
		S1,
		S2,
		DATA,
		DONE,
		END_PAUSE
	} state;
	// for use by state
	dword counter;
	int intra_byte; // +/i
};

class eTapeInstance {
public:
	eTapeInstance(struct stream *stream) : stream(stream) {}

	// start at beginning of tape (note the instance owns the iterator)
	virtual eTapePulseIterator *reset() = 0;
	virtual ~eTapeInstance() {
		stream_close(stream);
	}
protected:
	struct stream *stream;
};

class eTapeInstanceTAP : public eTapeInstance, eTapePulseIterator {
public:
	eTapeInstanceTAP(struct stream *stream) : eTapeInstance(stream) {}
	eTapePulseIterator *reset() override {
		stream_reset(stream);
		done = false;
		block_pulse_iterator.set_needs_reset();
		return this;
	}
protected:
	eTapeBlockPulseIterator block_pulse_iterator;
	bool done;

	int32_t next_pulse_length() override
	{
		while (!done) {
			int32_t r = block_pulse_iterator.next_pulse_length();
			if (r != -1) {
				return r;
			}
			next_block();
		}
		return -1;
	}

	void next_block() {
		const uint8_t *size_ptr = stream_peek(stream, 2);
		if (!size_ptr) {
			done = true;
		} else {
			dword size = Word(size_ptr);
			if (size)
			{
				stream_skip(stream, 2);
				const uint8_t *ptr = stream_peek(stream, 1);
				// this should read the full contents of the block
				block_pulse_iterator.reset(stream, size, 2168, 667, 735, 855, 1710, (*ptr < 4) ? 8064 : 3220,
										   1000);
			} else {
				done = true;
			}
		}
	}
};

class eTape : public eDeviceSound
{
	typedef eDeviceSound eInherited;
#ifndef NO_USE_FAST_TAPE
	friend class xZ80::eZ80_FastTape;
#endif
public:
	eTape(eSpeccy* s) : speccy(s) {}
#ifndef NO_USE_DESTRUCTORS
	virtual ~eTape() { CloseTape(); }
#endif
	virtual void Init();
	virtual void Reset();

#ifndef USE_HACKED_DEVICE_ABSTRACTION
	virtual bool IoRead(word port) const final;
	virtual dword IoNeed() const { return ION_READ; }
#endif
	virtual void IoRead(word port, byte* v, int tact) final;

#ifndef USE_STREAM
	bool Open(const char* type, const void* data, size_t data_size) {
		assert(false);
		return false;
	}
#else
	bool Open(const char* type, struct stream *stream);
#endif
	void Start();
	void Stop();
	void Rewind() { ResetTape(); }
	bool Started() const;
	bool Inserted() const;

	static eDeviceId Id() { return D_TAPE; }

	byte TapeBit(int tact);
protected:
	bool OpenTAP(struct stream *stream);
#ifndef NO_USE_CSW
	bool ParseCSW(struct stream *stream);
#endif
#ifndef NO_USE_TZX
	bool ParseTZX(struct stream *stream);
#endif

	void StopTape();
	void ResetTape();
	void StartTape();
public:
	void CloseTape();
protected:
	dword NextPulseOrStop();
	void SkipPulse();

#ifdef USE_LEGACY_TAPE_COMPARISON
	dword FindPulse(dword t);
	void FindTapeIndex();
#ifndef USE_MU_SIMPLIFICATIONS
	void FindTapeSizes();
#endif
	void Reserve(dword datasize);
	void MakeBlock(const byte* data, dword size, dword pilot_t,
		  dword s1_t, dword s2_t, dword zero_t, dword one_t,
		  dword pilot_len, dword pause, byte last = 8);
	void Desc(const byte* data, dword size, char* dst);
	void AllocInfocell();
#endif
protected:
	eSpeccy* speccy;

	eTapeInstance *tape_instance;
	eTapePulseIterator *pulse_iterator;

	struct eTapeState
	{
		qword edge_change;
#ifdef USE_LEGACY_TAPE_COMPARISON
		byte* play_pointer; // or NULL if tape stopped
		byte* end_of_tape;  // where to stop tape
		dword index;    // current tape block
#endif
		byte tape_bit;
		bool playing;
	};
	eTapeState tape;

#ifdef USE_LEGACY_TAPE_COMPARISON
	struct TAPEINFO
	{
#ifndef NO_USE_TAPEINFO_DESC
	   char desc[280];
#endif
	   dword pos;
	   dword t_size;
	};

	dword tape_pulse[0x100];
	dword max_pulses;
	dword tape_err;

	byte* tape_image;
	dword tape_imagesize;

	TAPEINFO* tapeinfo;
	dword tape_infosize;

	dword appendable;
#endif
};

#ifndef NO_USE_FAST_TAPE
extern xZ80::eZ80::eHandlerStep* fast_tape_emul;
#endif

#endif
#endif//__TAPE_H__

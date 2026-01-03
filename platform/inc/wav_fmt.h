/*
 *  File: wav_fmt.h
 *
 *  RIFF WAVE file format header.
 *
 *
 *  2022-2026 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 *
 */

#pragma once

#define RIFF_CHUNK_ID 0x46464952U
#define WAVE_FORMAT_ID 0x45564157U
#define FMT_CHUNK_ID 0x20746D66U
#define DATA_CHUNK_ID 0x61746164U
#define ACID_CHUNK_ID 0x64696361U
#define STRC_CHUNK_ID 0x63727473U
#define SLICE_CHUNK_ID 0x656C6973U

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_FLOAT 3

typedef struct chunk_t {
  uint32_t chunkId;
  uint32_t chunkSize;
} chunk_t;

typedef struct riff_chunk_t {
  uint32_t chunkId;
  uint32_t chunkSize;
  uint32_t format;
} riff_chunk_t;

typedef struct fmt_chunk_t {
  uint16_t format;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
} fmt_chunk_t;

typedef struct acid_chunk_t {
  uint32_t type;
  uint16_t rootNote;
  uint32_t unknown;
  uint32_t beats;
  uint16_t denominator;
  uint16_t nominator;
  float tempo;
} acid_chunk_t;

typedef struct strc_chunk_t {
  uint32_t unknown1;
  uint32_t slices;
  uint32_t unknown2;
  uint32_t unknown3;
  uint32_t unknown4;
  uint32_t unknown5;
  uint32_t unknown6;
} strc_chunk_t;

typedef struct slice_chunk_t {
  uint32_t unknown1;
  uint32_t unknown2;
  uint64_t sample1;
  uint64_t sample2;
  uint32_t unknown3;
  uint32_t unknown4;
} slice_chunk_t;

/*
** The acid chunk goes a little something like this:
**
** 4 bytes          'acid'
** 4 bytes (int)     length of chunk starting at next byte
**
** 4 bytes (int)     type of file:
**        this appears to be a bit mask,however some combinations
**        are probably impossible and/or qualified as "errors"
**
**        0x01 On: One Shot         Off: Loop
**        0x02 On: Root note is Set Off: No root
**        0x04 On: Stretch is On,   Off: Strech is OFF
**        0x08 On: Disk Based       Off: Ram based
**        0x10 On: ??????????       Off: ????????? (Acidizer puts that ON)
**
** 2 bytes (short)      root note
**        if type 0x10 is OFF : [C,C#,(...),B] -> [0x30 to 0x3B]
**        if type 0x10 is ON  : [C,C#,(...),B] -> [0x3C to 0x47]
**         (both types fit on same MIDI pitch albeit different octaves, so who cares)
**
** 2 bytes (short)      ??? always set to 0x8000
** 4 bytes (float)      ??? seems to be always 0
** 4 bytes (int)        number of beats
** 2 bytes (short)      meter denominator   //always 4 in SF/ACID
** 2 bytes (short)      meter numerator     //always 4 in SF/ACID
**                      //are we sure about the order?? usually its num/denom
** 4 bytes (float)      tempo
**
Subchunk1 ID	12	4	“fmt”
Subchunk1 Size	16	4	16
Audio Format	20	2	1
Num Channels	22	2	2
Sample Rate	24	4	22050
Byte Rate	28	4	88200
Block Align	32	2	4
Bits per Sample	34	2	16
Subchunk2 ID	36	4	“data”
*/

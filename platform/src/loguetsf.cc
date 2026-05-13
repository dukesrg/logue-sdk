/*
 *  File: loguetsf.cc
 *
 *  SoundFont unit for drumlogue and microKORG2
 * 
 *  2026 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 * 
 *  Based on TinySoundFont (TSF) by Bernhard Schelling
 *  Licensed under the MIT License.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "logue_wrap.h"
#include "logue_perf.h"
#include "logue_fs.h"

#define CHUNK_SIZE 131072
#define VELOCITY_SCALE (1.f / 127.f)

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#define TSF_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tsf.h"
#pragma GCC diagnostic pop

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  #define OUTPUT_MODE	TSF_MONO
  #define SOUNDFONT_PATH "/var/lib/microkorgd/userfs/Programs"
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  #define OUTPUT_MODE	TSF_STEREO_INTERLEAVED
  #define SOUNDFONT_PATH "/var/lib/drumlogued/userfs/Programs"
#endif

#ifdef UNIT_TARGET_MODULE_OSC
const unit_runtime_osc_context_t *runtime_context;
#endif

enum {
  param_soundfont = 0U,
  param_preset,
  param_max_voices,
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  param_velocity,
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  param_note,
#endif
  param_num,
};

enum {
  load_idle = 0L,
  load_start,
  load_alloc,
  load_read,
  load_close,
  load_tsf_load,
  load_tsf_set,
  load_finished,
};

const char *prefix = "";
const char *suffix = ".sf2";
static fs_dir soundfont_list = fs_dir(SOUNDFONT_PATH, prefix, suffix);
static tsf *soundfont;
static char *soundfont_buf;
static float *out_buf;
static int32_t Params[PARAM_COUNT];
static uint32_t state = load_idle;
static bool suspended;

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != UNIT_HEADER_TARGET_FIELD)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->input_channels != UNIT_INPUT_CHANNELS || desc->output_channels != UNIT_OUTPUT_CHANNELS)
    return k_unit_err_geometry;
#ifdef UNIT_TARGET_MODULE_OSC
  runtime_context = (unit_runtime_osc_context_t *)desc->hooks.runtime_context;
  out_buf = (float *)malloc(sizeof(float) * desc->frames_per_buffer);
#endif
  if (soundfont_list.count > 0)
    state = load_start;
  return k_unit_err_none;
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void) in;
//  PERFMON_START
  static FILE *fp;
  static size_t size;
  static size_t pos;  

  if (suspended)
    return;

  switch (state) {
    case load_start: {
      char *path = (char*)malloc(MAXNAMLEN + 1);    
      snprintf(path, MAXNAMLEN + 1, "%s/%s", SOUNDFONT_PATH, soundfont_list.get(Params[param_soundfont]));
      if (fp != nullptr)
        fclose(fp);
      fp = fopen(path, "rb");
      free(path);
      break;
    } 
    case load_alloc: {
      struct stat st;
      fstat(fileno(fp), &st);
      size = st.st_size;
      free(soundfont_buf);
      soundfont_buf = (char*)malloc(size);
      pos = 0;
//state = load_close - 1;
      break;
    }  
    case load_read:
      if (fread(soundfont_buf + pos, 1, CHUNK_SIZE, fp) < CHUNK_SIZE)
        break;
      pos += CHUNK_SIZE;
      state--;
      break;
    case load_close:
      if (fp != nullptr)
        fclose(fp);
      fp = nullptr;
      tsf_close(soundfont);
      break;
    case load_tsf_load:
      soundfont = tsf_load_memory(soundfont_buf, size);
      break;
    case load_tsf_set:
      tsf_set_output(soundfont, OUTPUT_MODE, k_samplerate, 0.f);
      tsf_set_max_voices(soundfont, Params[param_max_voices]);
      break;
    default:
      state = load_idle;
  }

  if (state != load_idle) {
    state++;
    return;
  }
  
  if (soundfont == nullptr)
    return;

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++) {
    if (runtime_context->trigger & (1 << voice_idx)) {
      tsf_note_on(soundfont, Params[param_preset], (uint32_t)runtime_context->pitch[voice_idx], Params[param_velocity] * VELOCITY_SCALE);
    }
  }
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  tsf_render_float(soundfont, out_buf, frames, TSF_FALSE);
  for (uint32_t i = 0; i < frames; i++) {
    out_buf[i] *= 0.125f;
    out[i * runtime_context->outputStride] = out_buf[i];
    out[i * runtime_context->outputStride + 1] = out_buf[i];
    out[i * runtime_context->outputStride + 2] = out_buf[i];
    out[i * runtime_context->outputStride + 3] = out_buf[i];
    out[i * runtime_context->outputStride + runtime_context->bufferOffset] = out_buf[i];
    out[i * runtime_context->outputStride + runtime_context->bufferOffset + 1] = out_buf[i];
    out[i * runtime_context->outputStride + runtime_context->bufferOffset + 2] = out_buf[i];
    out[i * runtime_context->outputStride + runtime_context->bufferOffset + 3] = out_buf[i];
  }
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  tsf_render_float(soundfont, out, frames, TSF_FALSE);
#endif
//  PERFMON_END(frames)
}

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  if (soundfont == nullptr)
    return;
  tsf_note_on(soundfont, Params[param_preset], note, velocity * VELOCITY_SCALE);
}
#endif

__unit_callback void unit_note_off(uint8_t note) {
  if (soundfont == nullptr)
    return;
  tsf_note_off(soundfont, Params[param_preset], note);
}

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  switch (index) {
    case param_soundfont:
      if (soundfont_list.count == 0)
        break;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      if (value == Params[index])
        break;
      if (soundfont != nullptr)
        tsf_note_off_all(soundfont);
      state = load_start;
      break;
    case param_preset:
    	if (soundfont == nullptr)
        break;
      if (value > tsf_get_presetcount(soundfont))
        value = tsf_get_presetcount(soundfont) - 1;
      if (value != Params[index])
        tsf_note_off_all(soundfont);
      break;
    case param_max_voices:
    	if (soundfont == nullptr)
        break;
      tsf_set_max_voices(soundfont, value);
      break;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
    case param_velocity:
#endif
    default:
      break;
  }
  Params[index] = value;
}

__unit_callback int32_t unit_get_param_value(uint8_t index) {
  return Params[index];
}

__unit_callback const char * unit_get_param_str_value(uint8_t index, int32_t value) {
  value = (int16_t)value;
  switch (index) {
    case param_soundfont:
      if (soundfont_list.count == 0)
        break;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      return soundfont_list.get(value);
    case param_preset:
    	if (soundfont == nullptr)
        break;
      if (value > tsf_get_presetcount(soundfont))
        value = tsf_get_presetcount(soundfont) - 1;
      return tsf_get_presetname(soundfont, value);
  }
  return nullptr;
}

__unit_callback void unit_reset() {
	if (soundfont == nullptr)
    return;
  tsf_reset(soundfont);
}

__unit_callback void unit_teardown() {
  free(soundfont_buf);
  soundfont_buf = nullptr;
  tsf_close(soundfont);
  soundfont = nullptr;
  soundfont_list.cleanup();
  free(out_buf);
  out_buf = nullptr;
}

__unit_callback void unit_suspend() {
  suspended = true;
}

__unit_callback void unit_resume() {
  suspended = false;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;
}

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  if (soundfont == nullptr)
    return;
  tsf_note_on(soundfont, Params[param_preset], Params[param_note], velocity * VELOCITY_SCALE);
}

__unit_callback void unit_gate_off() {
  if (soundfont == nullptr)
    return;
  tsf_note_off(soundfont, Params[param_preset], Params[param_note]);
}

__unit_callback void unit_all_note_off() {
	if (soundfont == nullptr)
    return;  
  tsf_note_off_all(soundfont);  
}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  (void)pitch_bend;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  (void)aftertouch;
}
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
__unit_callback void unit_platform_exclusive(uint8_t messageId, void * data, uint32_t dataSize) {
  (void)messageId;
  (void)data;
  (void)dataSize;
}
#endif

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
  #define SOUNDFONT_PATH "/var/lib/microkorgd/userfs/Programs"
  #define OUTPUT_MODE	TSF_MONO
  #define OUTPUT_INSTANCES UNIT_OUTPUT_CHANNELS
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  #include "osc_api.h"
  #define SOUNDFONT_PATH "/var/lib/drumlogued/userfs/Programs"
  #define OUTPUT_MODE	TSF_STEREO_INTERLEAVED
  #define OUTPUT_INSTANCES 1
#endif

#ifdef UNIT_TARGET_MODULE_OSC
const unit_runtime_osc_context_t *runtime_context;
#endif

enum {
  param_soundfont = 0U,
  param_preset,
  param_max_voices,
  param_sustain,
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
  load_tsf_copy,
  load_tsf_set,
  load_finished,
};

const char *prefix = "";
const char *suffix = ".sf2";
static fs_dir soundfont_list = fs_dir(SOUNDFONT_PATH, prefix, suffix);
static tsf __attribute__((aligned(32))) *soundfont[OUTPUT_INSTANCES];
static char __attribute__((aligned(32))) *soundfont_buf;
static float __attribute__((aligned(32))) *out_buf[OUTPUT_INSTANCES];
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
  for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
    out_buf[i] = (float *)aligned_alloc(32, sizeof(float) * desc->frames_per_buffer);
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
  static uint32_t instance_idx;

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
      for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
        tsf_close(soundfont[i]);
      break;
    case load_tsf_load:
      soundfont[0] = tsf_load_memory(soundfont_buf, size);
      if (Params[param_preset] >= tsf_get_presetcount(soundfont[0]))
        Params[param_preset] = tsf_get_presetcount(soundfont[0]) - 1;
      instance_idx = 1;
      break;
    case load_tsf_copy:
      if (instance_idx >= OUTPUT_INSTANCES)
        break;
      soundfont[instance_idx] = tsf_copy(soundfont[0]);
      instance_idx++;
      state--;
      break;
    case load_tsf_set:
      for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++) {
        tsf_set_output(soundfont[i], OUTPUT_MODE, k_samplerate, 0.f);
        tsf_set_max_voices(soundfont[i], Params[param_max_voices]);
        tsf_channel_set_presetindex(soundfont[i], 0, Params[param_preset]);
        tsf_channel_set_sustain(soundfont[i], 0, Params[param_sustain]);
      }
      break;
    default:
      state = load_idle;
  }

  if (state != load_idle) {
    state++;
    return;
  }
  
  if (soundfont[0] == nullptr)
    return;

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++) {
    if (runtime_context->trigger & (1 << voice_idx)) {
      tsf_channel_note_off_all(soundfont[voice_idx], 0);
      tsf_channel_note_on(soundfont[voice_idx], 0, (uint32_t)runtime_context->pitch[voice_idx], Params[param_velocity] * VELOCITY_SCALE);
    }
    tsf_render_float(soundfont[voice_idx], out_buf[voice_idx], frames, TSF_FALSE);
  }

  float *end = out_buf[0] + frames;
  switch (runtime_context->voiceLimit) {
    case kMk2MaxVoices: {
      float32x4_t v0, v1, v2, v3;
      __asm__ volatile (
        ".p2align 5\n"
        "1:\n"
        "vld1.32 {%q[v0]}, [%[s0]:128]!\n"
        "vld1.32 {%q[v1]}, [%[s1]:128]!\n"
        "vld1.32 {%q[v2]}, [%[s2]:128]!\n"
        "vld1.32 {%q[v3]}, [%[s3]:128]!\n"
        "vst4.32 {%e[v0], %e[v1], %e[v2], %e[v3]}, [%[out0]:256]!\n"
        "vst4.32 {%f[v0], %f[v1], %f[v2], %f[v3]}, [%[out0]:256]!\n"
        "vld1.32 {%q[v0]}, [%[s4]:128]!\n"
        "vld1.32 {%q[v1]}, [%[s5]:128]!\n"
        "vld1.32 {%q[v2]}, [%[s6]:128]!\n"
        "vld1.32 {%q[v3]}, [%[s7]:128]!\n"
        "vst4.32 {%e[v0], %e[v1], %e[v2], %e[v3]}, [%[out1]:256]!\n"
        "cmp %[s0], %[end]\n"
        "vst4.32 {%f[v0], %f[v1], %f[v2], %f[v3]}, [%[out1]:256]!\n"
        "blo 1b\n"
        : [v0]"=w"(v0), [v1]"=w"(v1), [v2]"=w"(v2), [v3]"=w"(v3)
        : [out0]"r"(out), [s0]"r"(out_buf[0]), [s1]"r"(out_buf[1]), [s2]"r"(out_buf[2]), [s3]"r"(out_buf[3]),
          [out1]"r"(out + 256), [s4]"r"(out_buf[4]), [s5]"r"(out_buf[5]), [s6]"r"(out_buf[6]), [s7]"r"(out_buf[7]),
          [end]"r"(end)
        : "cc", "memory"
      );
      break;
    }
    case kMk2HalfVoices: {
      float32x4_t v0, v1, v2, v3;
      __asm__ volatile (
        ".p2align 5\n"
        "1:\n"
        "vld1.32 {%q[v0]}, [%[s0]:128]!\n"
        "vld1.32 {%q[v1]}, [%[s1]:128]!\n"
        "vld1.32 {%q[v2]}, [%[s2]:128]!\n"
        "vld1.32 {%q[v3]}, [%[s3]:128]!\n"
        "vst4.32 {%e[v0], %e[v1], %e[v2], %e[v3]}, [%[out0]:256]!\n"
        "cmp %[s0], %[end]\n"
        "vst4.32 {%f[v0], %f[v1], %f[v2], %f[v3]}, [%[out0]:256]!\n"
        "blo 1b\n"
        : [v0]"=w"(v0), [v1]"=w"(v1), [v2]"=w"(v2), [v3]"=w"(v3)
        : [out0]"r"(out + runtime_context->bufferOffset), [s0]"r"(out_buf[0]), [s1]"r"(out_buf[1]), [s2]"r"(out_buf[2]), [s3]"r"(out_buf[3]),
          [end]"r"(end)
        : "cc", "memory"
      );
      break;
    }
    case kMk2QuarterVoices: {
      float32x4_t v0, v1;
      if (runtime_context->outputStride == 2) {
        __asm__ volatile (
          ".p2align 5\n"
          "1:\n"
          "vld1.32 {%q[v0]}, [%[s0]:128]!\n"
          "vld1.32 {%q[v1]}, [%[s1]:128]!\n"
          "cmp %[s0], %[end]\n"
          "vst2.32 {%q[v0], %q[v1]}, [%[out0]:256]!\n"
          "blo 1b\n"
          : [v0]"=w"(v0), [v1]"=w"(v1)
          : [out0]"r"(out), [s0]"r"(out_buf[0]), [s1]"r"(out_buf[1]),
            [end]"r"(end)
          : "cc", "memory"
        );
      } else if (runtime_context->voiceOffset == 0) {
        __asm__ volatile (
          ".p2align 5\n"
          "1:\n"
          "vld4.32 {%q[v0], %q[v1]}, [%[out0]:256]\n"
          "vld1.32 {%e[v0]}, [%[s0]:64]!\n"
          "vld1.32 {%f[v0]}, [%[s1]:64]!\n"
          "cmp %[s0], %[end]\n"
          "vst4.32 {%q[v0], %q[v1]}, [%[out0]:256]!\n"
          "blo 1b\n"
          : [v0]"=w"(v0), [v1]"=w"(v1)
          : [out0]"r"(out), [s0]"r"(out_buf[0]), [s1]"r"(out_buf[1]),
            [end]"r"(end)
          : "cc", "memory"
        );
      } else {
        __asm__ volatile (
          ".p2align 5\n"
          "1:\n"
          "vld4.32 {%q[v0], %q[v1]}, [%[out0]:256]\n"
          "vld1.32 {%e[v1]}, [%[s0]:64]!\n"
          "vld1.32 {%f[v1]}, [%[s1]:64]!\n"
          "cmp %[s0], %[end]\n"
          "vst4.32 {%q[v0], %q[v1]}, [%[out0]:256]!\n"
          "blo 1b\n"
          : [v0]"=w"(v0), [v1]"=w"(v1)
          : [out0]"r"(out), [s0]"r"(out_buf[0]), [s1]"r"(out_buf[1]),
            [end]"r"(end)
          : "cc", "memory"
        );
      }
      break;
    }
    case kMk2SingleVoice: {
      float32x4_t v0, v1;
      if (runtime_context->voiceOffset == 0) {
        __asm__ volatile (
          ".p2align 5\n"
          "1:\n"
          "vld2.32 {%q[v0], %q[v1]}, [%[out0]:256]\n"
          "vld1.32 {%q[v0]}, [%[s0]:128]!\n"
          "cmp %[s0], %[end]\n"
          "vst2.32 {%q[v0], %q[v1]}, [%[out0]:256]!\n"
          "blo 1b\n"
          : [v0]"=w"(v0), [v1]"=w"(v1)
          : [out0]"r"(out), [s0]"r"(out_buf[0]),
            [end]"r"(end)
          : "cc", "memory"
        );
      } else {
        __asm__ volatile (
          ".p2align 5\n"
          "1:\n"
          "vld2.32 {%q[v0], %q[v1]}, [%[out0]:256]\n"
          "vld1.32 {%q[v1]}, [%[s0]:128]!\n"
          "cmp %[s0], %[end]\n"
          "vst2.32 {%q[v0], %q[v1]}, [%[out0]:256]!\n"
          "blo 1b\n"
          : [v0]"=w"(v0), [v1]"=w"(v1)
          : [out0]"r"(out), [s0]"r"(out_buf[0]),
            [end]"r"(end)
          : "cc", "memory"
        );
      }
      break;
    }
  }
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  tsf_render_float(soundfont[0], out, frames, TSF_FALSE);
#endif
//  PERFMON_END(frames)
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
      if (soundfont[0] != nullptr)
        for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
          tsf_channel_sounds_off_all(soundfont[i], 0);
      state = load_start;
      break;
    case param_preset:
    	if (soundfont[0] == nullptr)
        break;
      if (value > tsf_get_presetcount(soundfont[0]))
        value = tsf_get_presetcount(soundfont[0]) - 1;
      if (value != Params[index]) {
        for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++) {
          tsf_channel_note_off_all(soundfont[i], 0);
          tsf_channel_set_presetindex(soundfont[i], 0, value);
        }
      }
      break;
    case param_max_voices:
    	if (soundfont[0] == nullptr)
        break;
      for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
        tsf_set_max_voices(soundfont[i], value);
      break;
    case param_sustain:
    	if (soundfont[0] == nullptr)
        break;
      for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
        tsf_channel_set_sustain(soundfont[i], 0, value);
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
    	if (soundfont[0] == nullptr)
        break;
      if (value > tsf_get_presetcount(soundfont[0]))
        value = tsf_get_presetcount(soundfont[0]) - 1;
      return tsf_get_presetname(soundfont[0], value);
  }
  return nullptr;
}

__unit_callback void unit_reset() {
	if (soundfont[0] != nullptr)
    for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++)
      tsf_reset(soundfont[i]);
}

__unit_callback void unit_teardown() {
  soundfont_list.cleanup();
  free(soundfont_buf);
  soundfont_buf = nullptr;
  for (uint32_t i = 0; i < OUTPUT_INSTANCES; i++) {
    tsf_close(soundfont[i]);
    soundfont[i] = nullptr;
    free(out_buf[i]);
    out_buf[i] = nullptr;
  }
}

__unit_callback void unit_suspend() {
  suspended = true;
}

__unit_callback void unit_resume() {
  suspended = false;
}

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  if (soundfont[0] != nullptr)
    tsf_channel_note_on(soundfont[0], 0, note, velocity * VELOCITY_SCALE);
}

__unit_callback void unit_note_off(uint8_t note) {
  if (soundfont[0] != nullptr)
    tsf_channel_note_off(soundfont[0], 0, note);
}

__unit_callback void unit_all_note_off() {
	if (soundfont[0] != nullptr)
    tsf_channel_note_off_all(soundfont[0], 0);
}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
	if (soundfont[0] != nullptr)
    tsf_channel_set_pitchwheel(soundfont[0], 0, pitch_bend);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
	if (soundfont[0] != nullptr)
    tsf_channel_midi_control(soundfont[0], 0, 11, pressure);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
	if (soundfont[0] != nullptr)
    tsf_channel_midi_control(soundfont[0], 0, 11, aftertouch);
}
#endif

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;
}

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
__unit_callback void unit_platform_exclusive(uint8_t messageId, void * data, uint32_t dataSize) {
  (void)messageId;
  (void)data;
  (void)dataSize;
}
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  if (soundfont[0] != nullptr)
    tsf_channel_note_on(soundfont[0], 0, Params[param_note], velocity * VELOCITY_SCALE);
}

__unit_callback void unit_gate_off() {
	if (soundfont[0] != nullptr)
    tsf_channel_note_off(soundfont[0], 0, Params[param_note]);
}
#endif

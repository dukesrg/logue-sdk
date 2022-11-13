#pragma once
/*
 *  File: synth.h
 *
 *  FM64 Synth Class.
 *
 *
 *  2022 (c) Oleg Burdaev
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <arm_neon.h>

#include <string.h>

#include "fm64.h"

class Synth {
 public:
  /*===========================================================================*/
  /* Public Data Structures/Types. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

    // Note: if need to allocate some memory can do it here and return k_unit_err_memory if getting allocation errors
    return k_unit_err_none;
  }

  inline void Teardown() {
    // Note: cleanup and release resources if any
  }

  inline void Reset() {
    // Note: Reset synth state. I.e.: Clear filter memory, reset oscillator
    // phase etc.
  }

  inline void Resume() {
    // Note: Synth will resume and exit suspend state. Usually means the synth
    // was selected and the render callback will be called again
  }

  inline void Suspend() {
    // Note: Synth will enter suspend state. Usually means another synth was
    // selected and thus the render callback will not be called
  }

  /*===========================================================================*/
  /* Other Public Methods. */
  /*===========================================================================*/

  fast_inline void Render(float * out, size_t frames) {
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output

    for (; out_p != out_e; out_p += 2) {
      // Note: should take advantage of NEON ArmV7 instructions
      vst1_f32(out_p, vdup_n_f32(0.f));
    }
  }

  inline void setParameter(uint8_t index, int32_t value) {
    (void)value;
    switch (index) {
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    switch (index) {
      default:
        break;
    }
    return 0;
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    (void)value;
    static char name[11] = {0};
    switch (index) {
      // Note: String memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the string
      //       before the next call to getParameterStrValue
      case 0:
        memcpy(name, dx_voices[0][value].dx7.vnam, 10);
        break;
      default:
        break;
    }
    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index,
                                              int32_t value) const {
    (void)value;
    switch (index) {
      // Note: Bitmap memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the bitmap
      //       before the next call to getParameterBmpValue
      // Note: Not yet implemented upstream
      default:
        break;
    }
    return nullptr;
  }

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    (void)note;
    (void)velocity;
  }

  inline void NoteOff(uint8_t note) { (void)note; }

  inline void GateOn(uint8_t velocity) {
    (void)velocity;
  }

  inline void GateOff() {}

  inline void AllNoteOff() {}

  inline void PitchBend(uint16_t bend) { (void)bend; }

  inline void ChannelPressure(uint8_t pressure) { (void)pressure; }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
  }

  inline void LoadPreset(uint8_t idx) {
    (void)idx;
    sPresetIndex = idx;
  }

  inline uint8_t getPresetIndex() const {
    return sPresetIndex;
  }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    (void)idx;
    // Note: String memory must be accessible even after function returned.
    //       It can be assumed that caller will have copied or used the string
    //       before the next call to getPresetName
    static char name[11] = {0};
    memcpy(name, dx_voices[0][idx].dx7.vnam, 10);
    return name;
  }

  static uint8_t sPresetIndex;

 private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/

  std::atomic_uint_fast32_t flags_;

  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
};

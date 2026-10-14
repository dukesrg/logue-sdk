/*
 * FM64 host test harness: a fake drumlogue runtime that loads the unit the
 * way the device does (sample banks named 'FM64Bank...' / 'FM64Wave ...'),
 * then drives it through the unit API and checks the audio it renders.
 *
 *   harness smoke             every factory voice: note, release, must decay
 *   harness params            every parameter across its full range
 *   harness fuzz [seed] [n]   random notes/params/gates for n blocks
 *   harness bench             worst-case render time per 64-frame block
 *
 * Exit status is the number of failed checks (capped at 255).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <string>
#include <vector>

#include "unit.h"
#include "../build/unit_host.cc"

extern "C" const unit_header_t unit_header;

static const uint32_t kFrames = 64;      // drumlogue block size
static const uint32_t kRate = 48000;

// ---- fake sample banks ------------------------------------------------------

struct Sample {
  std::vector<float> data;
  sample_wrapper_t w;
};
static std::vector<Sample> g_samples;

static bool load_wav(const char *path, const char *name) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "missing fixture %s\n", path); return false; }
  std::vector<uint8_t> b;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
  fclose(f);
  for (size_t i = 12; i + 8 <= b.size();) {
    uint32_t sz; memcpy(&sz, &b[i + 4], 4);
    if (!memcmp(&b[i], "data", 4)) {
      Sample s;
      s.data.resize(sz / 4);
      memcpy(s.data.data(), &b[i + 8], sz);
      memset(&s.w, 0, sizeof s.w);
      s.w.channels = 1;
      s.w.frames = s.data.size();
      strncpy(s.w.name, name, UNIT_SAMPLE_WRAPPER_MAX_NAME_LEN);
      g_samples.push_back(std::move(s));
      return true;
    }
    i += 8 + sz + (sz & 1);
  }
  return false;
}

static uint8_t rt_num_banks() { return 1; }
static uint8_t rt_num_samples(uint8_t bank) { return bank == 0 ? (uint8_t)g_samples.size() : 0; }
static const sample_wrapper_t *rt_get_sample(uint8_t bank, uint8_t idx) {
  if (bank != 0 || idx >= g_samples.size()) return nullptr;
  Sample &s = g_samples[idx];
  s.w.bank = bank; s.w.index = idx; s.w.sample_ptr = s.data.data();
  return &s.w;
}

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_fail++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void init_unit(const char *fixtures) {
  struct { const char *file, *name; } fx[] = {
    {"127_FM64Bank_Factory_and_VRC.wav", "FM64Bank Factory and VRC"},
    {"120_FM64Wave_DX11_4096.wav", "FM64Wave DX11 4096"},
    {"121_FM64Wave_SY77_4096.wav", "FM64Wave SY77 4096"},
    {"122_FM64Wave_opsix_1024.wav", "FM64Wave opsix 1024"},
    {"123_FM64Wave_logue_Saw,Sqr,Para_256.wav", "FM64Wave logue Saw,Sqr,Para 256"},
    {"124_FM64Wave_logue_Wave_banks_128.wav", "FM64Wave logue Wave banks 128"},
  };
  for (auto &f : fx) load_wav((std::string(fixtures) + "/" + f.file).c_str(), f.name);
  // the device copies names into fixed buffers; the unit reads the id as uint32
  static unit_runtime_desc_t desc;
  desc.target = unit_header.target;
  desc.api = unit_header.api;
  desc.samplerate = kRate;
  desc.frames_per_buffer = kFrames;
  desc.input_channels = 0;
  desc.output_channels = 2;
  desc.get_num_sample_banks = rt_num_banks;
  desc.get_num_samples_for_bank = rt_num_samples;
  desc.get_sample = rt_get_sample;
  int8_t err = unit_init(&desc);
  CHECK(err == k_unit_err_none, "unit_init returned %d", err);
  for (uint32_t i = 0; i < unit_header.num_params; i++)
    unit_set_param_value(i, unit_header.params[i].init);
}

// ---- audio checks -----------------------------------------------------------

struct Stats { float peak = 0; uint32_t bad = 0; };

static Stats render(uint32_t blocks) {
  Stats st;
  float out[kFrames * 2];
  for (uint32_t b = 0; b < blocks; b++) {
    unit_render(nullptr, out, kFrames);
    for (uint32_t i = 0; i < kFrames * 2; i++) {
      if (!isfinite(out[i])) st.bad++;
      else if (fabsf(out[i]) > st.peak) st.peak = fabsf(out[i]);
    }
  }
  return st;
}
static uint32_t blocks_for(float seconds) { return (uint32_t)(seconds * kRate / kFrames); }

// ---- tests ------------------------------------------------------------------

enum { P_NOTE = 0, P_VOICE = 1, P_MODE = 2 };

static void test_identity_and_range() {
  CHECK(strcmp(unit_header.name, "FM64") == 0, "unexpected build name");
  CHECK(unit_header.version == 0x00020100U, "unexpected build version");
  CHECK(unit_header.unit_id == 0x34364D46U, "voice-bank identifier changed");
  CHECK(unit_header.params[P_VOICE].max == 1023, "unexpected factory voice range");
  unit_set_param_value(P_VOICE, 1023);
  CHECK(unit_get_param_value(P_VOICE) == 1023, "last factory voice is unavailable");
  for (int32_t invalid : {-1, 1024, 32767}) {
    unit_set_param_value(P_VOICE, invalid);
    CHECK(unit_get_param_value(P_VOICE) == 1023, "invalid voice %d changed selection", invalid);
  }
  unit_set_param_value(P_VOICE, 0);
  const uint32_t bank_count = DxVoiceBanks[0].count;
  DxVoiceBanks[0].count = 16;
  unit_set_param_value(P_VOICE, 15);
  CHECK(unit_get_param_value(P_VOICE) == 15, "last loaded voice is unavailable");
  unit_set_param_value(P_VOICE, 16);
  CHECK(unit_get_param_value(P_VOICE) == 15, "unloaded voice changed selection");
  DxVoiceBanks[0].count = bank_count;
  unit_set_param_value(P_VOICE, 0);
}

static void test_slow_rate() {
  unit_set_param_value(P_VOICE, 0);
  unit_set_param_value(P_MODE, mode_poly);
  unit_set_param_value(param_rate_offset_carriers, -396);
  unit_set_param_value(param_rate_offset_modulators, -396);
  unit_set_param_value(param_krs_offset_carriers, 112);
  unit_set_param_value(param_krs_offset_modulators, 112);
  render(1);
  unit_note_on(0, 100);
  Stats audio = render(4);
  CHECK(audio.bad == 0, "slow envelope produced non-finite audio");
  unit_note_off(0);
  audio = render(4);
  CHECK(audio.bad == 0, "slow release produced non-finite audio");
  unit_set_param_value(param_rate_offset_carriers, 0);
  unit_set_param_value(param_rate_offset_modulators, 0);
  unit_set_param_value(param_krs_offset_carriers, 0);
  unit_set_param_value(param_krs_offset_modulators, 0);
}

static void test_all_notes_off() {
  unit_set_param_value(P_VOICE, 0);
  unit_set_param_value(P_MODE, mode_poly);
  render(1);
  for (uint8_t note : {48, 55, 60, 67}) {
    unit_note_on(note, 100);
    render(1);
  }
  render(blocks_for(0.5f));
  unit_all_note_off();
  for (uint32_t voice = 0; voice < POLYPHONY; voice++)
    CHECK(SynVoices[voice].state & state_noteoff, "All Notes Off did not release voice %u", voice);
  render(1);
  for (uint32_t voice = 0; voice < POLYPHONY; voice++)
    for (uint32_t op = 0; op < OPERATOR_COUNT; op++)
      CHECK(s_egstage[voice][op] == EG_STAGE_COUNT - 1,
            "All Notes Off: voice %u operator %u not in release", voice, op);
  printf("All Notes Off: %d failed checks\n", g_fail);
}

static void test_smoke(int voices) {
  printf("smoke: %d voices\n", voices);
  int silent = 0, loud = 0, retained = 0, slow = 0;
  for (int v = 0; v < voices; v++) {
    unit_set_param_value(P_VOICE, v);
    const char *name = unit_get_param_str_value(P_VOICE, v);
    if (getenv("FM64_VERBOSE")) fprintf(stderr, "voice %d '%s'\n", v, name);
    unit_note_on(60, 100);
    Stats on = render(blocks_for(0.5f));
    uint32_t active_voice = POLYPHONY;
    for (uint32_t voice = 0; voice < POLYPHONY; voice++)
      if (SynVoices[voice].note == 60) active_voice = voice;
    CHECK(active_voice < POLYPHONY, "voice %d was not triggered", v);
    if (active_voice == POLYPHONY) continue;
    const uint32_t voice = active_voice;
    int32_t targets[OPERATOR_COUNT];
    uint64_t duration = 0;
    bool retains_level = false;
    unit_note_off(60);
    for (uint32_t op = 0; op < OPERATOR_COUNT; op++) {
      targets[op] = s_eglevel[voice][op][EG_STAGE_COUNT - 1];
      retains_level |= targets[op] != 0;
      const int64_t delta = (int64_t)targets[op] - s_egval[voice][op];
      const int64_t signed_rate = s_egsrate[voice][op][delta > 0 ? EG_STAGE_COUNT * 2 - 1 : EG_STAGE_COUNT - 1];
      const int64_t rate = signed_rate < 0 ? -signed_rate : signed_rate;
      CHECK(delta == 0 || rate > 0, "voice %d operator %u: nonzero release distance with zero rate", v, op);
      if (delta != 0 && rate > 0) {
        const uint64_t frames = (uint64_t)ceil(fabs((double)delta) / rate);
        if (frames > duration) duration = frames;
      }
    }
    if (duration > 10 * kRate) slow++;
    if (retains_level) retained++;
    CHECK(duration <= 600 * kRate, "voice %d release exceeds test budget (600 seconds)", v);
    if (duration > 600 * kRate) continue;
    Stats release = render(2);
    s_sample_cnt[voice] += duration;
    Stats boundary = render(2);
    CHECK(boundary.bad == 0, "voice %d: non-finite release-boundary audio", v);
    CHECK(release.bad == 0, "voice %d: non-finite release audio", v);
    for (uint32_t op = 0; op < OPERATOR_COUNT; op++) {
      CHECK(s_egstage[voice][op] == EG_STAGE_COUNT - 1, "voice %d operator %u did not enter release", v, op);
      CHECK(s_egval[voice][op] == targets[op], "voice %d operator %u did not reach release target", v, op);
    }
    Stats tail = render(2);
    for (uint32_t op = 0; op < OPERATOR_COUNT; op++)
      CHECK(s_egval[voice][op] == targets[op], "voice %d operator %u release target is unstable", v, op);
    CHECK(tail.bad == 0, "voice %d: non-finite tail audio", v);
    CHECK(on.bad == 0, "voice %d '%s': %u non-finite samples", v, name, on.bad);
    if (on.peak < 1e-4f) silent++;
    if (on.peak > 4.f) { loud++; printf("  loud: voice %d '%s' peak %.2f\n", v, name, on.peak); }
  }
  printf("  silent %d, loud %d, releases over 10 seconds %d, nonzero final levels %d\n", silent, loud, slow, retained);
}

static void test_params() {
  printf("params: %u\n", unit_header.num_params);
  for (uint32_t id = 0; id < unit_header.num_params; id++) {
    const unit_param_t &p = unit_header.params[id];
    if (p.type == k_unit_param_type_none) continue;
    int32_t span = p.max - p.min, step = span > 400 ? span / 400 : 1;
    unit_note_on(48, 100);
    for (int32_t v = p.min; v <= p.max; v += (v + step > p.max && v != p.max) ? p.max - v : step) {
      unit_set_param_value(id, v);
      CHECK(unit_get_param_value(id) == v, "param %u '%s' get(%d) != set", id, p.name, v);
      const char *s = unit_get_param_str_value(id, v);
      if (p.type == k_unit_param_type_strings) {
        CHECK(s != nullptr, "param %u '%s' value %d: null string", id, p.name, v);
        if (s) CHECK(strlen(s) <= UNIT_PARAM_NAME_LEN, "param %u '%s' value %d: string '%s' too long (%zu)", id, p.name, v, s, strlen(s));
      }
      Stats st = render(2);
      CHECK(st.bad == 0, "param %u '%s' value %d: non-finite output", id, p.name, v);
      if (v == p.max) break;
    }
    unit_note_off(48);
    unit_set_param_value(id, p.init);
    render(blocks_for(0.2f));
  }
}

static uint32_t rng_state;
static uint32_t rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }
static int32_t rnd_in(int32_t lo, int32_t hi) { return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)); }

static void test_fuzz(uint32_t seed, uint32_t blocks) {
  printf("fuzz: seed %u, %u blocks (%.0f s audio)\n", seed, blocks, blocks * (float)kFrames / kRate);
  rng_state = seed ? seed : 1;
  uint8_t held[128] = {0};
  float worst = 0;
  for (uint32_t b = 0; b < blocks; b++) {
    for (uint32_t k = rnd() % 4; k > 0; k--) {
      uint32_t op = rnd() % 100;
      if (op < 40) { uint8_t n = rnd_in(0, 127); unit_note_on(n, rnd_in(1, 127)); held[n] = 1; }
      else if (op < 75) { uint8_t n = rnd_in(0, 127); unit_note_off(n); held[n] = 0; }
      else if (op < 80) unit_gate_on(rnd_in(1, 127));
      else if (op < 85) unit_gate_off();
      else if (op < 87) unit_pitch_bend(rnd_in(0, 16383));
      else {
        uint32_t id = rnd() % unit_header.num_params;
        const unit_param_t &p = unit_header.params[id];
        if (p.type == k_unit_param_type_none) continue;
        int32_t v = rnd_in(p.min, p.max);
        unit_set_param_value(id, v);
        unit_get_param_str_value(id, v);
      }
    }
    Stats st = render(1);
    if (st.bad) { CHECK(false, "block %u: %u non-finite samples", b, st.bad); break; }
    if (st.peak > worst) worst = st.peak;
  }
  for (int n = 0; n < 128; n++) if (held[n]) unit_note_off(n);
  unit_gate_off();
  render(blocks_for(15.f));
  Stats tail = render(blocks_for(0.5f));
  printf("  worst peak %.2f, tail after all notes off %.5f\n", worst, tail.peak);
  CHECK(tail.peak < 1e-3f, "still sounding 15 s after all notes off (tail %.4f)", tail.peak);
}

static double now_us() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }

static void test_bench() {
  // 4 held notes, unison/poly, worst-case algorithm with feedback
  for (int n = 0; n < 4; n++) unit_note_on(48 + n * 7, 127);
  float out[kFrames * 2];
  const int N = 20000;
  double worst = 0, total = 0;
  for (int i = 0; i < N; i++) {
    double t = now_us();
    unit_render(nullptr, out, kFrames);
    double d = now_us() - t;
    total += d; if (d > worst) worst = d;
  }
  double budget = kFrames * 1e6 / kRate;
  printf("bench: mean %.2f us, worst %.2f us per %u-frame block (real-time budget %.0f us, host CPU)\n",
         total / N, worst, kFrames, budget);
}

int main(int argc, char **argv) {
  const char *fixtures = getenv("FM64_FIXTURES") ? getenv("FM64_FIXTURES") : "fixtures";
  std::string cmd = argc > 1 ? argv[1] : "all";
  init_unit(fixtures);
  int voices = unit_header.params[P_VOICE].max + 1;
  // past the last bank getDxVoice returns nullptr and the name falls back to "%d"
  int count;
  for (count = 0; count < voices; count++) {
    char num[16]; snprintf(num, sizeof num, "%d", count);
    if (!strcmp(unit_get_param_str_value(P_VOICE, count), num)) break;
  }
  printf("loaded %zu samples, %d voices\n", g_samples.size(), count);

  if (cmd == "identity" || cmd == "all") test_identity_and_range();
  if (cmd == "slow-rate" || cmd == "all") test_slow_rate();
  if (cmd == "notes-off" || cmd == "all") test_all_notes_off();
  if (cmd == "smoke" || cmd == "all") test_smoke(count);
  if (cmd == "params" || cmd == "all") test_params();
  if (cmd == "fuzz" || cmd == "all")
    test_fuzz(argc > 2 ? strtoul(argv[2], 0, 10) : 1, argc > 3 ? strtoul(argv[3], 0, 10) : blocks_for(120.f));
  if (cmd == "bench") test_bench();
  printf("%s: %d failed checks\n", cmd.c_str(), g_fail);
  return g_fail > 255 ? 255 : g_fail;
}

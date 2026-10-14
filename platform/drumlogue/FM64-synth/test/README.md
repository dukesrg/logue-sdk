# FM64 host checks

Run from this directory on an ARM64 Mac with Apple Clang, make, curl, and shasum:

```sh
bash fetch-fixtures.sh
make test
make fuzz SEEDS="1 2 3 4 5" BLOCKS=100000
make bench
```

The downloader retrieves the author's six voice-bank and waveform WAV files from https://dukesrg.github.io/logue-sdk/ and verifies hashes against the files used locally. Those assets are not included in this contribution; downloading them does not establish redistribution rights. If the files change, review them before updating the hashes.

`make test` runs a byte-range envelope-level regression, build identity and voice-range checks, a slow-rate regression, All Notes Off checks, all 1,024 factory voices, parameter sweeps, and randomized notes and parameters under AddressSanitizer and UndefinedBehaviorSanitizer.

For one check:

```sh
make build/fm64_san
FM64_FIXTURES=fixtures build/fm64_san identity
FM64_FIXTURES=fixtures build/fm64_san slow-rate
FM64_FIXTURES=fixtures build/fm64_san notes-off
FM64_FIXTURES=fixtures build/fm64_san smoke
FM64_FIXTURES=fixtures build/fm64_san params
```

The harness invokes the unit API and inspects envelope state. It compiles a host-only copy of `unit.cc` with four compatibility rewrites defined in the Makefile. `host/arm.h` substitutes portable implementations for ARM32 instructions, while NEON vector operations run on the Mac. These substitutions need review. They do not prove equivalence to ARM32 hardware.

Release tests calculate each envelope's remaining duration from its target and quantized increment, then advance its sample clock to check the release boundary and stable target. They do not render the whole release tail or compare sound against another synthesizer. Benchmark results describe the host CPU, not the drumlogue.

The fixture reader expects the supplied float WAV assets; it is not a general or hardened WAV reader. Missing or modified fixtures must be resolved before interpreting test results.

The production build remains:

```sh
cd ..
make CROSS_COMPILE=arm-unknown-linux-gnueabihf-
```

Local production builds used GCC 15.2.0. Korg's official toolchain still needs verification. The display name and bank identifier remain FM64; the version is 2.1.0. The fixed Voice maximum is 1023, matching the factory bank; larger custom banks are no longer selectable through that parameter.

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p fixtures
for filename in \
  120_FM64Wave_DX11_4096.wav \
  121_FM64Wave_SY77_4096.wav \
  122_FM64Wave_opsix_1024.wav \
  '123_FM64Wave_logue_Saw,Sqr,Para_256.wav' \
  124_FM64Wave_logue_Wave_banks_128.wav \
  127_FM64Bank_Factory_and_VRC.wav
do
  curl --fail --location --output "fixtures/$filename" "https://dukesrg.github.io/logue-sdk/$filename"
done
(cd fixtures && shasum -a 256 -c ../fixtures.sha256)

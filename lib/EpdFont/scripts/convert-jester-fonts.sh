#!/bin/bash
set -e
cd "$(dirname "$0")"
SRC="../builtinFonts/source/IBMPlexSerif"

# Medium body: Medium/Medium Italic occupy the regular/italic slots so body
# text is a little heavier on e-ink; Bold/Bold Italic occupy the bold slots.
# Same faces as SD_Card_Extras/Fonts/IBMPlexSerifMedium-ExpandedCoverage.
declare -A FACE=( [regular]=Medium [italic]=MediumItalic [bold]=Bold [bolditalic]=BoldItalic )
NOTO="../builtinFonts/source/NotoSerif"

# Coverage transcribed from the tested .cpfont files so the built-in faces
# match them exactly.
DEFAULT_INTERVALS=(
  --additional-intervals 0x0000,0x017F
  --additional-intervals 0x01A0,0x01A1
  --additional-intervals 0x01AF,0x01B0
  --additional-intervals 0x01C4,0x021F
  --additional-intervals 0x0300,0x036F
  --additional-intervals 0x0400,0x04FF
  --additional-intervals 0x1EA0,0x1EF9
  --additional-intervals 0x2000,0x2064
  --additional-intervals 0x2066,0x2071
  --additional-intervals 0x2074,0x208E
  --additional-intervals 0x2090,0x209C
  --additional-intervals 0x20A0,0x20BF
  --additional-intervals 0x2190,0x22FF
  --additional-intervals 0xFB00,0xFB06
  --additional-intervals 0xFFFD,0xFFFD
  # IPA and Greek. IBM Plex Serif covers the full modern Greek alphabet but
  # almost no IPA (only U+0259), so these are filled from Noto Serif below.
  --additional-intervals 0x0250,0x02AF
  --additional-intervals 0x02B0,0x02FF
  --additional-intervals 0x0370,0x03FF
  --additional-intervals 0x1F00,0x1FFF
)
# Noto Serif is face index 1 in the size-12 stack. Restricting it to these
# ranges means Plex (face 0) still wins wherever Plex has the glyph, so modern
# Greek stays in Plex and only archaic signs, polytonic Greek and IPA come
# from Noto.
NOTO_FILL=(
  --font-include-intervals 1:0x0250,0x02AF
  --font-include-intervals 1:0x02B0,0x02FF
  --font-include-intervals 1:0x0370,0x03FF
  --font-include-intervals 1:0x1F00,0x1FFF
)
# UI sizes: ASCII, Latin-1 (accented file names, titles and authors, plus the
# degree sign in UI strings) and General Punctuation.
BASE_INTERVALS=(
  --additional-intervals 0x0000,0x007F
  --additional-intervals 0x00A0,0x00FF
  --additional-intervals 0x2000,0x2064
  --additional-intervals 0x2066,0x206F
  --additional-intervals 0xFFFD,0xFFFD
)
RENDER=(--2bit --compress --pnum --darken-aa --zopfli --no-default-intervals)

gen() {  # $1 size  $2 style-slot  $3 interval-array-name  [$4 "stack"]
  local size=$1 slot=$2 arr=$3[@]
  local name="plexserif_${size}_${slot}"
  local stack=("$SRC/IBMPlexSerif-${FACE[$slot]}.ttf") fill=()
  if [[ "$4" == "stack" ]]; then
    # Noto Serif uses the same weights as Plex.
    stack+=("$NOTO/NotoSerif-${FACE[$slot]}.ttf")
    fill=("${NOTO_FILL[@]}")
  fi
  python3 fontconvert.py "$name" "$size" "${stack[@]}" \
    "${!arr}" "${fill[@]}" "${RENDER[@]}" > "../builtinFonts/${name}.h"
  echo "  $name  $(stat -c%s ../builtinFonts/${name}.h) B source"
}

ONLY="${1:-all}"
if [[ "$ONLY" == all || "$ONLY" == 12 ]]; then
  for slot in regular italic bold bolditalic; do gen 12 "$slot" DEFAULT_INTERVALS stack; done
fi
for size in 8 10 14; do
  [[ "$ONLY" == all || "$ONLY" == "$size" ]] || continue
  for slot in regular italic bold bolditalic; do gen "$size" "$slot" BASE_INTERVALS; done
done

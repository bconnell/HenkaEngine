# UI font parser test fixture

`AtkinsonHyperlegible-Regular.ttf` is a test-only static TrueType fixture used
to exercise Henka's pinned `stb_truetype` parser and raster path. It is not the
production font selection and is not included in Sandbox packages.

- Upstream: `googlefonts/atkinson-hyperlegible`, commit
  `1cb311624b2ddf88e9e37873999d165a8cd28b46`
- Upstream artifact: `fonts/ttf/AtkinsonHyperlegible-Regular.ttf`
- Version: 1.006, static TrueType outlines
- SHA-256: `7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669`
- License: SIL Open Font License 1.1; the full license wording is retained in
  `Atkinson-OFL.txt` beside the font, with trailing whitespace normalized.
  The original upstream license SHA-256 is
  `F32D22B3908FCAD2C86A74000614EC22E6A7F66EA7E867E616026A27AEBDC143`.
  Copyright 2020 Braille Institute of America, Inc. No Reserved Font Name is
  declared in the retained license.

The fixture is deliberately separate from `assets/fonts/`; the production
typeface remains unselected until the matched editor comparison gate.

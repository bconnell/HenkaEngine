# Editor font comparison assets

These two static TrueType files are packaged solely to compare the approved
editor typography candidates in Henka. Neither candidate is selected as the
final editor font until matched native-size captures have been reviewed.

Both font files and their complete SIL Open Font License 1.1 texts are kept
beside this record. These assets are application-owned UI resources; the core
text renderer receives verified bytes and does not open filesystem paths.

## Atkinson Hyperlegible Regular

- Source: `googlefonts/atkinson-hyperlegible`, commit
  `1cb311624b2ddf88e9e37873999d165a8cd28b46`, file
  `fonts/ttf/AtkinsonHyperlegible-Regular.ttf`.
- Size: 54,348 bytes.
- SHA-256: `7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669`.
- License: SIL Open Font License 1.1; copyright 2020 Braille Institute of
  America, Inc.; full text is in `Atkinson-OFL.txt`.
- Static TrueType artifact; no variable-font tables.

## Noto Sans Regular

- Source: `notofonts/notofonts.github.io`, commit
  `28b15b4b43b7bed62b5cf6e6b0b5ff5846270535`, file
  `fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf`.
- Size: 621,572 bytes.
- SHA-256: `478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823`.
- License: SIL Open Font License 1.1; copyright 2022 The Noto Project Authors;
  full text is in `NotoSans-OFL.txt`.
- Static hinted TrueType artifact; no variable-font tables.

The exact source artifact and license hashes were checked against the local
source record on 2026-10-12 before importing these files. The pinned stb
parser/raster path and packaged runtime still require executable validation.

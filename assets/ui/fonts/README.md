# Editor typography assets

Atkinson Hyperlegible Regular is Henka's current bundled editor font. Noto
Sans Regular remains a comparison-only candidate for the local packaged
readability harness; the ordinary product package excludes it. The matched
packaged captures were reviewed at normal working size on 2026-11-14 at 100%,
125%, 150%, and 200% UI scale, at 1280×720 and 1920×1080. Both fonts looked
good and no clear visual winner emerged. Atkinson remains the current product
default because it was the selected default; the comparison did not establish an objective readability advantage over Noto. The refreshed schema-2
comparison manifest records OS display scale for every capture; this set
reported 1/1 throughout, so it does not prove non-100% Windows display scaling.

Both static TrueType files and their complete SIL Open Font License 1.1 texts
are kept beside this record so the comparison remains reproducible. Only
Atkinson and its license are part of ordinary Sandbox packages. These assets
are application-owned UI resources; the core text renderer receives verified
bytes and does not open filesystem paths.

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
source record on 2026-10-12 before importing these files. On 2026-11-13, the
rebuilt packaged runtime comparison verified both font identities across all
four Editor UI Scale values and two window sizes, including persisted 200%
scale after restart. It also exposed clipped scale-choice labels in the compact
200% Settings row. The shared segmented-control text inset now follows the
active context's measured font width; refreshed packaged captures show all
four values without truncation. Atkinson is the current product default and
Noto remains a validation-only package option. The pinned stb parser/raster
path, layout metrics, and package policy remain covered by executable
validation.

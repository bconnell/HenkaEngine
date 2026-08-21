# Henka Script Source Authoring and Reload Design

## Goal

Add a bounded, reusable source-authoring seam for Lua and HenkaScript so the
Inspector can edit, validate, save, and eventually reload behavior sources
without creating a second language definition or mutating a live Play runtime
until a candidate backend has passed validation.

## Authority and boundaries

- The HenkaScript compiler remains the authority for HenkaScript tokens,
  grammar, diagnostics, and executable validity.
- The Lua backend remains the authority for Lua source compilation and
  diagnostics. The editor must not copy a Lua lexer, keyword table, or grammar.
- The source document owns bounded editable text and presentation state only;
  it does not own a scene document behavior, runtime entity, or backend.
- The Game Authoring coordinator owns behavior attachment metadata and save
  policy. The Play session owns active runtime backends.
- Reload is a candidate-build-and-swap operation. A failed candidate leaves
  the previous backend and behavior state untouched.
- Source edits use the application-local editor input path. Physical cursor
  movement must not alter editor coordinates during deterministic automation,
  and no global OS input blocking is permitted.

## Source document contract

Add `engine/include/henka/script_source.h` and
`engine/src/scripting/script_source.c` with a bounded document whose maximum
source size is the existing language limit (`HENKA_HKS_MAX_SOURCE_BYTES` for
HenkaScript and `HENKA_LUA_MAX_SOURCE_BYTES` for Lua). The document stores a
heap-owned NUL-terminated buffer of fixed maximum capacity, language, byte
length, monotonically increasing local revision, dirty state, and the latest
language-neutral diagnostic.

The public operations are:

```c
henka_result henka_script_source_create(
    henka_script_language language,
    henka_script_source_document** out_document);
void henka_script_source_destroy(henka_script_source_document* document);
henka_result henka_script_source_set_text(
    henka_script_source_document* document,
    const char* source,
    size_t source_size);
henka_result henka_script_source_get_text(
    const henka_script_source_document* document,
    const char** out_source,
    size_t* out_source_size);
henka_script_language henka_script_source_get_language(
    const henka_script_source_document* document);
henka_result henka_script_source_mark_clean(
    henka_script_source_document* document);
henka_result henka_script_source_validate(
    henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic);
henka_result henka_script_source_get_diagnostic(
    const henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic);
bool henka_script_source_is_dirty(
    const henka_script_source_document* document);
uint64_t henka_script_source_get_revision(
    const henka_script_source_document* document);
```

`set_text` accepts syntactically invalid text when it is within the bounded
source limit; validation is separate so an editor can preserve unsaved work.
`validate` invokes `henka_hks_behavior_backend_create` or
`henka_lua_behavior_backend_create`, immediately destroys the temporary backend,
and translates only the diagnostic envelope. It does not duplicate compiler
rules or retain executable state.

## Confined loading and atomic saving

Extend the script asset API with source-document load and atomic save operations
that resolve paths through `henka_path_resolve_confined`. Saving is allowed for
invalid source so the user does not lose work, but it must never activate or
reload invalid source.

The save path must:

1. reject null, empty, traversal, overlong, and language-mismatched paths;
2. create only the confined parent directory;
3. write the complete NUL-excluded source to a unique sibling temporary file;
4. flush and close the temporary file before replacement;
5. replace the destination with `MoveFileExA(...MOVEFILE_WRITE_THROUGH)` on
   Windows or `rename` on POSIX;
6. remove the temporary file after any failure; and
7. leave the previous destination byte-for-byte intact when validation or I/O
   fails.

The source document is marked clean only after replacement succeeds. The
persistence boundary is the only intended caller of
`henka_script_source_mark_clean`; a failed save leaves its text and dirty state
available to the editor.

## Editor behavior

The existing `examples/sandbox3d/script_editor.c` becomes a focused editor
module rather than a second parser. It will:

- keep a bounded source document per selected behavior;
- preserve line endings and user indentation when displaying source;
- use compiler/backend diagnostics for error line and column presentation;
- use HenkaScript public token kinds for token spans and brace-depth display;
- avoid syntax-aware Lua behavior unless the Lua backend supplies the result;
- provide explicit Edit, Save, Revert, and Reload actions;
- reject active editing while Play is running;
- keep invalid edits staged and visibly marked, but never activate them; and
- route keyboard, text, cursor, and mouse interactions through application-local
  logical input state so automation is deterministic.

The first editor slice may use a bounded line buffer and fixed maximum visible
lines, but it must not flatten code into a single document-style label. Lines,
indentation, line numbers, selection/caret state, diagnostics, and language
identity remain distinct presentation elements.

## Transactional runtime reload

Add a Play-session reload operation that accepts the selected authored behavior
and a candidate source document. It must:

1. reject stopped, paused-error, invalid-handle, or Play-locked authoring
   mutations with the existing result contract;
2. validate and construct the candidate backend using the existing asset/backend
   boundary;
3. preserve the old backend, callback binding, lifecycle state, and behavior
   state until candidate construction succeeds;
4. stop/destroy the old backend only after the candidate is ready;
5. attach the candidate to the same generation-checked behavior handle; and
6. report a bounded diagnostic while leaving the old runtime active on failure.

State migration is not implicit in this slice. The default behavior is to retain
the existing typed state store identity when the behavior ID and authored
entity ID remain unchanged; explicit schema/version migration is future work.

## Verification requirements

- Unit tests cover source limits, ownership, revision/dirty transitions,
  language validation, diagnostics, confined traversal rejection, atomic save
  failure retention, and successful replacement.
- Editor-facing tests cover preserved indentation, compiler-derived HKS token
  spans, invalid-source staging, and Play edit rejection through stable module
  seams.
- Runtime tests cover candidate reload success, invalid-candidate retention,
  generation-checked handles, lifecycle ordering, and state preservation.
- The Debug MSVC warning gate, focused scripting/scene/Play suite, external
  mixed-language project gate, documentation truth check, and `git diff --check`
  run before publication.

## Non-goals for this slice

- no new language syntax;
- no copied Lua or HenkaScript grammar in the editor;
- no unrestricted filesystem access;
- no global input blocking;
- no implicit state migration;
- no debugger, breakpoint, or replay system; and
- no claim that the entire Game Authoring campaign is complete.

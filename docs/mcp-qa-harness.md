# MCP QA Harness

> **Status:** Foundation plus face-authoring history validation

Henka Sandbox3D provides a bounded local MCP adapter for executable inspection
and semantic editor checks. The adapter exposes the existing Henka Action API
and Scene Document mapping; it does not create a second scene or identity
authority.

## Current scope

| Capability | Availability | Production authority |
| --- | --- | --- |
| Capability discovery | Available | MCP protocol adapter |
| Live scene observation | Available | Scene, viewport, and Action API state |
| Persistent-object selection | Available | Scene Document ID resolved by Game Authoring, then Action API selection |
| Clean candidate exit | Available | Henka engine lifecycle |
| Face component editing (mode, selection, extrusion) | Available | Live authoring bridge and canonical mesh operation |
| Framebuffer capture | Planned | Renderer and visual-proof path |
| Undo and redo through canonical authoring history | Available | Live authoring bridge and canonical history |
| Save and reload workflow | Planned | Authoring and persistence systems |

The initial adapter is intentionally small. A tool is added only when the
same operation is available through a supported Henka product path.

## Protocol boundary

The adapter uses newline-delimited JSON-RPC over local standard input/output
and reports MCP protocol revision `2026-07-28`. Each request is independent;
the adapter does not create a session or retain scene state between requests.
The `server/discover` response identifies the current candidate and advertised
tools. `tools/list` returns bounded input schemas.

```mermaid
flowchart LR
    Client[Local MCP client]
    Transport[Bounded JSON-RPC stdio]
    Adapter[Sandbox3D MCP adapter]
    Coordinator[Game Authoring coordinator]
    Action[Henka Action API]
    Scene[Live scene and Scene Document]
    Client --> Transport --> Adapter
    Adapter --> Coordinator
    Coordinator --> Action
    Action --> Scene
    Scene --> Adapter --> Transport --> Client
```

## Advertised tools

### `henka.observe`

Reads authoritative live state, including:

- render revision;
- framebuffer and scene-viewport dimensions;
- active shading mode;
- persistent Scene Document IDs and live runtime entities;
- object names, visibility, and selection state;
- authoring geometry revision, selection mode, selected-component count, and
  topology counts when an object has an authoring mesh.

### `henka.select_object`

Accepts a persistent Scene Document ID. The Game Authoring coordinator resolves
that ID to the live entity, then the existing `HENKA_ACTION_COMMAND_SELECT_OBJECT`
path performs the selection. Missing or stale IDs return a structured semantic
error and do not silently select another object.

### `henka.authoring_set_selection_mode`

Sets the live authoring component-selection mode for the currently selected
editable object. The supported values are `vertex`, `edge`, and `face`. The
returned `authoring_mode` value is `edit`, which describes the existing
authoring surface; the MCP adapter does not create a separate hidden edit-mode
authority.

### `henka.authoring_select_face`

Selects a face by its persistent object document ID and authoritative face ID.
The target must already be selected through `henka.select_object`, be editable,
and be in face-selection mode. Face IDs are discovered from the live
authoring mesh; callers must not guess them. Invalid, stale, or unavailable
faces return a semantic error.

### `henka.authoring_extrude_faces`

Extrudes the currently selected live faces through the same transactional
authoring operation used by the editor. The response reports the geometry
revision and topology counts before and after the operation. It does not
implement a second mesh mutation path.

### `henka.authoring_undo` and `henka.authoring_redo`

Move the selected live editable object through its existing canonical
authoring history. Each response reports the operation, geometry revision,
topology counts before and after, and current selection state. The adapter
does not retain a second history or reconstruct mesh state itself.

### `henka.exit`

Requests clean shutdown of the local Sandbox3D candidate. It does not terminate
arbitrary processes or provide shell, filesystem, or operating-system control.

## Candidate identity

Launchers may provide `HENKA_CANDIDATE_ID`. The adapter reports that bounded
value in structured results so a harness can associate observations with the
executable it started. Candidate identity is provenance metadata, not a product
object ID and not an authorization mechanism.

## Validation

The protocol core has unit coverage for bounded parsing, discovery, tool
schemas, structured semantic errors, oversized requests, and callback routing.
The Windows production smoke starts the real packaged Sandbox3D executable and
proves this chain:

```text
live Sandbox3D scene
  -> server/discover and tools/list
  -> observe a real scene object
  -> resolve its persistent Scene Document ID
  -> select it through the canonical Action API
  -> observe the resulting live selection
  -> enter the existing face authoring selection mode
  -> discover and select the first authoritative face
  -> extrude it through the canonical authoring operation
  -> verify revision and topology advancement in a fresh observation
  -> undo through canonical authoring history and verify restored topology
  -> redo through canonical authoring history and verify restored topology
  -> reject an invalid face identity
  -> reject an invalid persistent ID
  -> request clean exit
```

The smoke validates that protocol output remains on stdout while product
diagnostics remain on stderr. It uses a bounded progress-aware response
deadline rather than waiting for a fixed startup log line.

Framebuffer capture, save, and reload are not part of this slice. They remain
separate validation work until the corresponding canonical product paths are
exposed through the harness.

## Security and ownership boundaries

- The adapter is local-first and has no network listener.
- Request and response sizes are bounded.
- Product state, entity handles, persistent IDs, and mutations remain owned by
  Henka systems.
- MCP clients cannot invoke arbitrary shell commands, read arbitrary files, or
  bypass Action API validation.
- An MCP request cannot establish a capability that is absent from the normal
  editor, runtime, or public API path.

The MCP adapter is an interoperability and validation surface. It is not a
replacement for the editor, the Action API, the Scene Document, or the
renderer.

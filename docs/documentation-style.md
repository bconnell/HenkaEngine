# Documentation Presentation Standard

Henka documentation should be technically complete, truthful, readable, and easy to scan. Presentation work must preserve capability boundaries, limitations, ownership rules, validation expectations, commands, paths, and implementation meaning.

## Goals

Documentation should help a reader quickly answer four questions:

1. What does this subsystem or workflow do?
2. What works today?
3. What remains incomplete or planned?
4. Where can the reader find implementation, validation, or usage details?

## Live documentation

Documentation is part of the product boundary.

When implementation changes affect public behavior, capability status, commands, platform support, persistence, editor terminology, packaging, validation, supported formats, ownership, failure behavior, or roadmap direction, the relevant live documentation must be updated in the same coherent change.

At a publication boundary:

- known stale documentation is a defect;
- capability wording must match the actual production implementation;
- removed or changed commands must be reflected in usage docs;
- new supported workflows must be documented at the level users need to operate them;
- planned work must remain clearly planned;
- current limitations must remain visible;
- cross-references must point to the current authoritative document.

Documentation-only modernization may improve structure and wording while preserving capability truth.

## Page structure

Long documents should begin with a short summary and, when useful, a local table of contents.

Use headings often enough that readers can navigate by scanning. Useful recurring section names include:

- Summary
- Status
- Architecture
- Available
- In Progress
- Planned
- Limitations
- Validation
- Usage
- Failure behavior
- Related documentation

Not every document needs every section. Use the smallest structure that makes the material easier to understand.

## Paragraphs

Avoid screen-length walls of text.

A paragraph that contains several distinct requirements, capabilities, lifecycle stages, validation conditions, or limitations should be split into smaller paragraphs, lists, tables, or focused subsections.

Use prose for explanation and reasoning. Use lists or tables for repeated facts, requirements, states, technical comparisons, inventories, sequences, and validation gates.

Keep normal prose cohesive. Avoid artificial one-line fragments.

## Direct declarative wording

State facts, requirements, limitations, and plans directly.

Use one clear assertion at a time. Name the current scope. State remaining gaps separately. State validation requirements separately. This keeps technical documentation factual and removes defensive or conversational wording.

Documentation should avoid:

- rhetorical contrast;
- defensive capability disclaimers;
- conversational justification of a design choice;
- negative framing when a positive scope statement carries the same information;
- repeated statements that explain what a feature is by comparing it with something else;
- prose that argues with an imagined reader;
- wording that sounds like a response to a previous conversation.

Preferred wording is declarative:

- `The runtime uses the production scene graph.`
- `Linux support is planned.`
- `This subsystem remains a Foundation.`
- `Human visual QA remains required for subjective visual acceptance.`
- `Complete game-project serialization remains future work.`
- `The current scene model is a bounded entity model.`

Negative wording is appropriate when the negative condition is itself part of the technical contract, such as a rejected input, forbidden state, unsupported operation, safety boundary, or failure condition. Keep those statements short and factual.

## Lists and tables

Use lists when order, steps, requirements, supported operations, or known gaps matter.

Use tables when readers need to compare several items across the same attributes. Good uses include:

- capability status;
- platform support;
- file formats;
- lifecycle states;
- validation gates;
- feature maturity;
- backend support.

Avoid excessively wide tables on pages that must remain readable on narrow displays.

## Status callouts

Short blockquotes may be used for important status information.

Example:

> **Status:** Foundation  
> **Current focus:** Runtime integration and end-user workflow completion

Status wording must remain consistent with the repository's authoritative capability definitions. Presentation does not change capability truth.

## Diagrams

Use Mermaid when architecture, ownership, lifecycle, dependency, or data flow is materially clearer in visual form.

Suitable subjects include:

- scene and entity ownership;
- asset loading and dependency flow;
- Audio runtime flow;
- Play-session lifecycle;
- renderer pipeline stages;
- hierarchy and prefab relationships;
- roadmap dependencies.

Keep diagrams bounded and readable. A diagram should explain a specific relationship and include only the details needed for that relationship.

## Code, commands, and paths

Use fenced code blocks for:

- commands;
- code examples;
- configuration;
- serialized formats;
- multi-line paths;
- diagnostic output when the exact structure matters.

Use inline code for short identifiers, API names, filenames, flags, and single commands.

Examples must match supported product behavior. Illustrative syntax that appears executable must be valid for the documented boundary.

## Collapsible detail

Use `<details>` blocks sparingly for long implementation notes, diagnostic examples, or advanced material that most readers do not need immediately.

Critical limitations, safety requirements, failure behavior, setup steps, support boundaries, and validation requirements must remain visible without expanding a collapsed section.

## Capability truth

Documentation presentation work must preserve:

- current capability status;
- explicit limitations;
- production and Foundation distinctions;
- ownership and lifetime requirements;
- failure behavior;
- platform scope;
- persistence boundaries;
- compatibility boundaries;
- validation and evidence requirements.

A documentation-only cleanup must preserve the supported capability boundary.

## Current, planned, and historical material

Keep current behavior visually separate from future direction. Headings such as **Available**, **In Progress**, and **Planned** should be used when they improve state clarity.

Historical implementation notes should remain only when they explain a current contract, migration, compatibility rule, or important design decision.

Source history and release records should carry implementation history that no longer helps explain the current product.

## Links and navigation

Link to the authoritative detailed document and keep duplicate overview text small.

Use descriptive link labels.

Long overview documents should point readers toward focused subsystem pages. Focused subsystem pages should link to relevant architecture, build, capability, roadmap, and validation documentation when useful.

## Visual restraint

"Prettier" means easier to read, navigate, and understand.

Avoid:

- decorative badge walls;
- excessive emoji;
- oversized banners;
- repeated visual elements with no informational purpose;
- ornamental section breaks that add scroll length without adding structure.

Henka branding should support identity and navigation without competing with technical content.

## Repository consistency

The documentation set should use consistent terminology and structure.

Repository-wide consistency includes:

- matching subsystem names;
- matching status labels;
- matching command names and flags;
- matching file-format names and versions;
- matching platform claims;
- matching roadmap sequence;
- matching ownership language;
- matching capitalization for product terms such as Audio, Scene View, Sandbox, and Play when they refer to named Henka concepts.

Overview documents should summarize. Subsystem documents should carry detailed contracts. Validation documents should explain evidence and acceptance paths.

## Validation

When documentation changes alongside product changes, run the repository's existing documentation truth, hygiene, integrity, and relevant product gates.

For documentation-only presentation work, review the diff for:

- accidental capability changes;
- removed limitations;
- broken links or anchors;
- altered commands or paths;
- duplicated or contradictory status wording;
- Mermaid syntax errors;
- Markdown formatting regressions;
- rhetorical comparison or defensive wording;
- stale references to superseded behavior;
- missing links to newly authoritative subsystem documents.

The governing principle is simple: preserve product truth and improve information design.

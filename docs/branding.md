# Henka Engine branding

The official project artwork is tracked under `assets/branding/`:

- `henka_engine_lockup.png` is the supplied full emblem, **HENKA**, and **ENGINE** lockup for spacious documentation or application-information surfaces.
- `henka_engine_emblem.png` is a faithful emblem-only crop derived from the supplied artwork for constrained presentation, including the SDL application and detached-tool-window icons.

These are official Henka Engine project assets supplied by the project owner. The runtime resolves the emblem relative to the executable directory, so the normal build and packaged Sandbox both own the resource without source-tree or developer-machine paths. Missing icon resources fail safely to the platform default and do not affect headless execution.

Branding uses contain-style presentation conventions: preserve the source aspect ratio, retain padding, and use the emblem when the full lockup would become unreadable. The artwork itself is not recolored, stretched, or overlaid on active Scene View content.

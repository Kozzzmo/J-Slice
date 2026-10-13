# J-Slice — project memory for Claude

Jungle / drum and bass break slicer and pattern generator plugin (VST3 / AU / standalone), built with **JUCE 8.0.15**
(fetched by CMake; 8.0.15+ bundles the VST3 SDK under the MIT licence — do not go back to older JUCE versions,
whose VST3 SDK needed a signed Steinberg agreement for closed-source sales). JUCE itself is used under the JUCE 8
licence (free "Starter" tier up to USD 20k revenue a year).

- CI: `.github/workflows/build.yml` (Windows, macOS universal + auval, Linux tests + pluginval); every push to
  `main` republishes the `latest` pre-release.
- Patterns: 16 slots A01..A16 (`PluginProcessor` `slots`, each = steps + loop length + generator settings) and a
  CHAIN; a slot clicked while playing is queued in the engine (`Engine::queuePattern`) and starts when the playing
  pattern ends (`patStartK` = its step 1). UI: `Source/UI/PatternBar`.
- Never bundle or demo copyrighted breaks (Amen, Think, Funky Drummer…): the user loads their own files.

## Working with the owner (read first)

- The owner (GitHub **Kozzzmo**) writes in **French**: answer in French.
- Git: commit as `Kozzzmo <Kozzzmo@users.noreply.github.com>`. **No `Co-Authored-By`, `Claude-Session` or any
  Claude attribution** in commits, PRs or files — the owner asked explicitly (history was rewritten to remove them).
- The code is **proprietary** (see `LICENSE`). Third-party code is allowed only under a permissive licence and must be
  listed in `THIRD_PARTY_NOTICES.txt`, which ships with every release.
- Product family: Kinetik (KDSP-16, KMSP-6, J-Slice, KDYN-10). Shared look: serif italic "Kinetik" logo + four-point
  star, skins NEON (default) / MIDNIGHT / RISO / CLASSIC, Instrument Serif + IBM Plex Mono + Archivo Black (OFL).
- Launch tracker (claude.ai artifact, tasks in its db collection `tasks`):
  https://claude.ai/artifact/3DAkMghoFDeC59JUTAcdkn — update task status with the ArtifactData tool when work
  from the list is done.

# Termic Amiga screenshot gallery

Current native UI: **0.1.4 Beta**, captured on the physical Amiga with MUI 5.
These are real production widgets populated by an isolated documentation
fixture. Projects, task states, activity, process metrics and account status
are synthetic. The Tiny view demonstrates profile selection and instructions;
it does not show a completed inference. No cloud requests, queued builds,
process termination or service restarts run during capture.

The Mac proxy is the unchanged companion application. Its own version label
is independent of the native 0.1.4 Beta release. The AmigaGuide refresh is pending a bridge recovery.
The [current manual](../amiga/native/Termic.guide) includes Offline Tiny.

## Workspace and Tiny

### Editable source, projects, tasks, Files and CLI

![Editable source, projects, tasks, Files and CLI](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/workspace-cli.png)

### Tiny selected in the Agent workspace

![Tiny selected in the Agent workspace](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/live-feedback.png)

### Offline Tiny 2.1 profile

![Offline Tiny 2.1 profile](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/tiny-profile.png)

### Compact editor at 340 x 280

![Compact editor at 340 x 280](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/editor-340x280.png)

### Compact Tiny task at 340 x 280

![Compact Tiny task at 340 x 280](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/tiny-340x280.png)

## Processors

### Integrated Processors tab

![Integrated Processors tab](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/processors.png)

### Optional ZZ9000 details

![Optional ZZ9000 details](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/zz9000.png)

### Compact Processors view at 340 x 280

![Compact Processors view at 340 x 280](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/processors-340x280.png)

## Agents and access

### Agent profiles

![Agent profiles](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/agents.png)

### Codex profile

![Codex profile](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/codex-profile.png)

### Claude profile

![Claude profile](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/claude-profile.png)

### Cursor profile

![Cursor profile](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/cursor-profile.png)

### Amiga MCP access

![Amiga MCP access](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/agent-access.png)

### Companion AI Proxy on macOS

![Companion AI Proxy on macOS](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/ai-proxy.png)

## Task tools

### Kanban board

![Kanban board](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/kanban.png)

### Task queue

![Task queue](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/parallel-queue.png)

### Coloured change review

![Coloured change review](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/review-diff.png)

### Broadcast instructions

![Broadcast instructions](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/broadcast.png)

## Preferences and help

### Application preferences

![Application preferences](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/preferences.png)

### Shipped prompt library

![Shipped prompt library](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/prompts.png)

### Keyboard shortcut reference

![Keyboard shortcut reference](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/keyboard-shortcuts.png)

### Shortcut preferences

![Shortcut preferences](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/shortcut-preferences.png)

### About Termic Amiga

![About Termic Amiga](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/about.png)

### AmigaGuide contents

Pending: the bridge paused while preparing the new MultiView capture.
The README retains its clearly labelled earlier guide image.

## Reproducing captures

Build `amiga/native/tests/test_gallery_window.c` with the GUI dependencies,
`-DTERMIC_EMBEDDED`, and `amiga/apps/process-monitor/main.c`. Build
`amiga/native/tests/inspect_chrome.c` as ChromeProbe. From the repository root,
with the bridge Python environment and an idle, unreserved Amiga:

```sh
python scripts/amiga/accept_gallery.py \
  --binary /path/to/GalleryCheck --probe /path/to/ChromeProbe \
  --output .context/gallery-new
```

The runner reserves the shared bridge, creates a fresh Acceptance drawer,
verifies transferred bytes, opens the fixture and captures each window only
while it is in front. It checks complete screenshot row transfer, source
preservation, Tiny selection and requested geometry. It closes its own
fixture afterwards. It does not replace the installed Termic application.
Small windows use scrollable panes; long content and controls may require
scrolling. Manual interaction testing and release installation tests remain
separate.

Capture the companion app with
`python scripts/proxy/app.py --screenshot-fixture .context/ai-proxy.png`.
After target recovery, capture the shipped guide in its own MultiView instance with
`python scripts/amiga/accept_guide_gallery.py --output .context/guide-new`.
Screenshots stay outside Git history and are attached to the release.

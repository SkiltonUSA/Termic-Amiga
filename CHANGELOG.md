# Termic Amiga changelog

## [0.1.4-beta] - 2026-11-18

Offline Tiny inference and an integrated Processors workspace tab.

### Features

- Bundle the original Tiny 2.1 model and native runtime for offline 68k
  question answering. Default tasks without a saved agent selection to Tiny;
  preserve existing selections and credentials.
- Open Process Monitor inside the main Processors tab. Pause sampling while
  hidden, retaining supervision of a previously confirmed service restart.
  Keep ZZ9000 board information optional.
- Add a reproducible Termic documentation training pipeline and separately
  labelled experimental model/checkpoint downloads. The candidate scores
  31/71 on unseen questions and is not the bundled default.

### Packaging and validation

- Provide the native LHA/SDK kit, a SHA-256-pinned AmiPkg entry, optional
  standalone monitor and checksums. Existing host proxies remain unchanged.
- 100 host tests pass. AmiPkg dependency resolution and a fresh host install
  plan are checked separately from physical Amiga acceptance.
- Physical installation, offline task execution and the revised Processors
  tab remain unverified on the Amiga for this release.

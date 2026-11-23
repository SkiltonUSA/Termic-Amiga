# Development screenshots

Screenshots are synthetic test evidence, not installable release content.
They must stay in gitignored `.context/screenshots/` or another local evidence
directory outside release staging. Do not commit PNGs, upload them as release
assets, or copy them into an application installation or distribution archive.
Older published screenshots have been archived locally and removed from release
assets. The README and user guides do not depend on those downloads.

Use the written `scripts/amiga/accept_gallery.py` suite for native captures;
its output directory must be outside release staging. It uses production MUI
widgets with synthetic data. Physical capture requires a reservation and an
idle target. For the proxy, use `scripts/proxy/smoke_bundle.py`; its temporary
screenshot is checked and deleted with the fixture directory.

See [release policy and validation](releases.md).

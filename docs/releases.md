# Release policy

Termic Amiga releases use versioned Git tags (`v0.2.0-beta`), matching native
version strings, clear release notes, separate platform downloads and SHA-256
checksums. Beta releases are marked as GitHub prereleases. This is the project's
packaging policy; the no-PNG rule is a user requirement, not a general GitHub rule.

## No PNGs or screenshots

PNG files must never be uploaded as release assets, copied to release staging,
or included in an installable archive or application distribution. Do not add
screenshots to the native kit, SDK input staging, Mac app distribution or
bridge package. Keep synthetic visual test evidence outside release directories,
under a gitignored local screenshots directory. Reviewed synthetic captures for
the README may be copied to `docs/screenshots/` in the repository. That directory
is excluded from Git source archives using `.gitattributes` (`export-ignore`).
This documentation exception never applies to installable packages or release
assets. Never use personal projects,
credentials or real activity logs as screenshots.

The native, monitor, proxy and gateway packaging scripts check their staged
file trees. Release validation also opens the ZIP/LHA archives and checks their
contents. The release-policy CI job tests these checks and rejects documentation
that depends on release PNG assets. It permits tracked PNG files only
under `docs/screenshots/` and requires that directory to be excluded from Git
source archives. A release-event job checks published download names against
the same allowlist. README images use repository-relative documentation paths;
PNG release download links are not allowed.

## Public download list

A release asset directory contains exactly:

- `Termic-Amiga-VERSION.lha`
- `Termic-Process-Monitor-VERSION.lha`
- `Termic-AI-Proxy-macOS.zip`
- `Amiga-MCP-MultiClient-macOS-arm64.zip`
- `termic-amiga.json`
- `SHA256SUMS`

Build logs, test evidence, credentials, local paths, training checkpoints and
screenshots do not belong in that directory. Use a clean `assets` subdirectory
separate from build work and evidence. Every asset except SHA256SUMS is covered
by the manifest. The AmiPkg entry must pin the exact native archive bytes.

Before upload:

```sh
python3 -m unittest discover -s scripts/amiga -p 'test_*.py'
python3 scripts/amiga/release_policy.py --repository . \
  --directory /path/to/assets --version 0.2.0-beta --lha /path/to/lha
```

Also run upstream AmiPkg validation, the packaged Proxy smoke/mailbox/desktop
checks, upstream gateway tests and the packaged multi-client gateway check.
Create a GitHub draft first, upload only these validated assets, download them
again and compare every checksum before publication. Then verify the public
AmiPkg archive URL. State any missing physical-Amiga or platform acceptance.
Do not call locally signed Mac bundles Developer ID signed or notarized.

## Older releases

The user requested that obsolete downloads be hidden. Convert old releases to
drafts after the replacement is ready, preserving their non-image assets and
Git tags. Draft downloads are visible to repository writers; source tags remain
public. This does not make old commits private. Screenshot assets are archived
locally outside release staging before removal from GitHub. Do not alter the
whole repository's visibility or delete release history to hide downloads.

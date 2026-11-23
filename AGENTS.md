# Termic Amiga release rules

Read docs/releases.md before packaging or publishing.

- Publish this project only to SkiltonUSA/Termic-Amiga. Never push the original
  desktop Termic repository as part of Amiga work.
- PNG files and screenshots must never be release assets, appear in release
  staging, or be copied into an installer or installable package. Keep synthetic
  screenshots in gitignored local evidence directories outside release staging.
  Reviewed synthetic README images may be committed under docs/screenshots/
  only; that directory must have export-ignore in .gitattributes.
- Use the explicit release asset list and SHA256SUMS. Run
  scripts/amiga/release_policy.py on staged downloads before any upload.
- Publish tested versioned artifacts with an accurate validation statement.
  Check uploaded bytes and the public AmiPkg archive URL before declaring success.
- Keep the Mac and Windows Proxies and multi-client Amiga-MCP gateway as separate downloads.
  Verify the gateway's source revision and packaged multi-client behavior.
- Do not change an upstream gateway's version to match Termic's application version.
- Older releases may be retained as drafts at the user's request. Preserve source
  tags and do not change repository visibility or rewrite Git history.

# Building and validating an AmiPkg release

The release consists of the native `.lha` installation kit and a pinned
`termic-amiga.json` AmiPkg entry. The entry is suitable for catalog review; it
does not by itself add Termic to the public AmiPkg catalog. No catalog submission
or signing is performed by these scripts.

## Prerequisites and installation behavior

The recipe requires AmigaOS 3.2, 68020+, **MUI 5.0 already installed**, and
`C:lha`. It declares dependencies on TextEditor.mcc 15.56 and AmiSSL 5.27. It
deliberately omits the catalog's MUI 3.8 package. MUI core is never installed or
replaced by the Termic kit.

The fresh-install destination is `SYS:Programs/Termic-Amiga`. The preflight
refuses an existing drawer so that a reinstall cannot overwrite configuration
or tasks. The post-install script assigns `Termic:`, extracts the pinned SDK
archives, installs the bundled native Git/MicroPython tools and runs
`TermicCheck`. Post-script failure is a warning in AmiPkg; read its output and
run `Termic:TermicCheck` again before launching. No User-Startup line is added;
TermicNative assigns its own program drawer when launched.

TextEditor is copied into the active `MUI:` drawer only if absent. An older
existing class is preserved and reported by TermicCheck. AmiSSL's catalog
package installs libraries; a full upstream AmiSSL installation with working
certificate configuration is still needed for verified HTTPS. The checker
verifies versions/files, not certificate validity, network connectivity, GitHub
login or API access. `TermicCheck --gui` checks only CPU, OS, MUI and TextEditor.

For an existing installation, close Termic and finish workers, back up the
installation, and replace only program binaries, Source, ReadMe, Termic.guide
and build/install scripts. Preserve Config, Projects, task drawers, Prompts,
ENV settings and keys. Run the dependency checker afterwards. The fresh-install
recipe intentionally does not automate upgrades into a populated drawer.

## Reproducible host commands

Supply original archives matching `amiga/native/sdk-lock.json`, the pinned
MicroPython files under `downloads/MicroPython`, and compiled Hunk binaries
named TermicNative, TermicAgent, TermicMCP, TermicStorage and TermicCheck.

```sh
python3 scripts/amiga/package_native.py \
  --downloads downloads --binaries binaries \
  --output Termic-Amiga-0.1.0-beta.lha --lha /path/to/lha

python3 scripts/amiga/package_amipkg.py \
  --archive Termic-Amiga-0.1.0-beta.lha \
  --archive-url https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.0-beta/Termic-Amiga-0.1.0-beta.lha \
  --version 0.1.0-beta --output termic-amiga.json
```

The URL must identify the exact bytes uploaded. Use a new output filename for
each build; neither script silently replaces an existing archive/entry. The kit
is built from tracked templates and verified inputs, never a live installation.
It contains an empty API key and no user tasks, queue history or job logs.

Clone the upstream authoring catalog and client separately, then validate:

```sh
python3 scripts/amiga/validate_amipkg.py \
  --archive Termic-Amiga-0.1.0-beta.lha --entry termic-amiga.json \
  --catalog /path/to/amiga-pkg --client /path/to/amipkg --lha /path/to/lha
```

This runs upstream `amigapkg.py validate`, then compiles the client's actual
JSON parser, dependency resolver, version comparison and recipe planner with
ASan/UBSan. It checks dependency order and minima, missing dependencies, CPU
requirements and the file plan. It executes that copy plan into a temporary
fresh host tree and checks binary headers, byte identity and empty credentials.
AmigaDOS scripts and installed system dependencies need separate target tests.
After publication, upstream `amigapkg.py validate termic-amiga.json
--check-archives` additionally downloads the advertised URL and verifies its pin.

Validation was developed against `amiga-pkg` commit
`434e2f992247991eb35711f6f5dfa678c2333777` and `amipkg` commit
`66cfb9888f7d6b28d1411ff254817ee14ed4e043`. Upstream sources:
[package authoring](https://github.com/thomas-luebker/amiga-pkg/blob/main/PACKAGING.md)
and [AmiPkg client](https://github.com/thomas-luebker/amipkg).

# Verification and limitations

The Amiga application was exercised on an A4000TX with a TF4060/68060,
Workbench 3.2.3, MUI 3.8, TextEditor.mcc 15.56, AmiSSL 5.27 and ZZ9000 RTG.
The screenshots show synthetic fixtures on that machine.

Verified through the written hardware suites:

- Project/task creation, panel navigation, local compilation and program execution.
- Embedded editing, backup saves and exact plain-source export from coloured text.
- Local Git initialization, commits, status and diffs on an FFS long-filename image.
- Shell/build failure reporting and preservation of the previous executable.
- Quit/reopen with last-workspace restoration.
- About opening from the editor, closing/reopening, and all three Amiga version tags.

The worker's stale-output regression failed before its fix and passed afterward.
Verified HTTPS returned the expected unauthenticated HTTP 401.
Authenticated cloud model/tool exchange has not been verified with a user key.

The existing long-filename image was prepared separately. Startup mounting and
refusal to recreate an active volume were exercised; automatic fresh-image
creation by `TermicStorage --create` has not been hardware-tested.
No ARM compute payload or firmware change is part of this application.
Automated checks and screenshots do not replace human interaction testing.

The API key is stored in plaintext in the local installation's Config drawer.
It is excluded from this repository and installation kits. Source and requested
compiler output are sent to the selected cloud API when an agent runs.

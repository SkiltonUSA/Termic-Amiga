# Build and scripts

The native C toolchain is VBCC, vasm, vlink and NDK3.2. MUI programs use
muimaster.library and may include mui_compat.h from the installed source path.
Keep code compatible with AmigaOS 3.x and use soft float on the 68060 target.

Build / CLI is the user's command runner. Its selector accepts:

- CLI: an AmigaDOS command, for example List.
- ARexx: a script path and arguments, for example Termic:Examples/hello.rexx.
  SYS:Rexxc/RX and a running RexxMast are required.
- MicroPython: a script path and arguments, for example Termic:Examples/hello.py.
  The runtime is Termic:Tools/MicroPython/micropython.

Commands run in the selected workspace. Interactive input uses a separate
Amiga CLI console. The editor supports C/H, Python, ARexx, text and Markdown.
The direct API agent can list/read/write C/H files and invoke the native build.
It cannot execute scripts or run the compiled program. Test program execution
only through an explicitly available and authorized user/hardware workflow.

BuildProject compiles main.c unless build-command.txt supplies a command that
produces Program.new. A failed build must retain the previous Program.
The project-specific build command is user configuration, not an agent tool.

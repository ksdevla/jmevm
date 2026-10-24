# jmevm

Java ME (CLDC 1.1, MIDP 2.0) on a virtual machine whose whole state is
plain data. Any moment of a app can be saved and loaded again, rewound, or
replayed exactly.

It has two parts:

- **The virtual machine** (`src/`, `include/`, `classlib/`), in C11.
  Runs are deterministic down to the emulated tick, and snapshots are
  validated before they are loaded. A compressed rewind history can be
  kept.
- **The MIDP layer** (`midp/`): screen, images, fonts, sound, record stores
  and input for MIDlets, with all their state in the VM's snapshots. It
  comes with:
  - an embedding library for apps that show the app themselves
    ([midp/platform/embed/jmevm_midp.h](midp/platform/embed/jmevm_midp.h));
  - a desktop runner, `jmevm-midp`, with a window, F5 save, F9 load and
    Backspace-held rewind.

**Status:** 3D (JSR 184, Mascot Capsule) and the camera are not there yet.

The VM's embedding API is [include/jmevm/jmevm.h](include/jmevm/jmevm.h).

## Building

Requires CMake 3.21+, a C11 compiler and a JDK (11 or newer) to compile the
class libraries and the Java tests. `-DJMEVM_CLASSLIB=OFF` builds the C code
alone; `-DJMEVM_MIDP=OFF` leaves out the MIDP layer.

On Windows with Visual Studio 2026 Build Tools:

```
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

`windows-asan` builds and tests the same with AddressSanitizer. The Android
presets (`android-arm64`, `android-x86_64`) need `ANDROID_NDK_HOME` and build
the libraries only.

## Running a app on the desktop

```
build/windows/midp/Release/jmevm-midp --window 2 app.jar
```

`classlib.jar` (CLDC and MIDP) is built next to the runner. Run it without
arguments to list the options: scripted keys, screenshots, frame logs,
input recording and replay, `--save-state-at MS:FILE`, `--load-state FILE`
and more.

## The VM alone

```
jmevm [--heap MB] [--epoch MS] [--gc-stress] [--print-ticks] [--boot PATH] -cp PATH MAIN [ARGS...]
```

`--boot` is the compiled class library (`build/windows/classlib/classes`).
Testing aids: `--snapshot-check STEP` and `--rewind-check STEP` save a
snapshot every STEP ticks and check that each one restores exactly (exit
code 4 if not). `--check-stats` prints the sizes.

## Layout

| Path | Contents |
|---|---|
| `include/jmevm/` | The VM's public API |
| `src/` | The VM |
| `classlib/` | The CLDC 1.1 class library (Java) |
| `tools/` | The `jmevm` command-line runner |
| `tests/` | VM tests: unit tests, Java programs and their expected output |
| `midp/src/` | MIDP natives (`midp_jme`), graphics and fonts, sound, ZIP and PNG |
| `midp/classlib/` | The MIDP classes (Java) |
| `midp/platform/embed/` | The embedding library |
| `midp/platform/host/` | The desktop runner |
| `midp/tests/` | MIDlet tests: output, frames, determinism, replay, save states |

## Licence

Apache-2.0 (see [LICENSE](LICENSE)), with the exceptions listed in
[NOTICE](NOTICE). Only permissively licensed code may be added; GPL code is
not accepted.

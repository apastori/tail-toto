# tail-toto

A GNU `tail`-compatible command written in C11. It prints the last part of
each file, or of standard input, to standard output (by default the last 10
lines). Input and output use raw `read(2)` / `write(2)`; regular files are
read backwards from the end, so only the tail is ever read. There is no
follow mode: `tail-toto` processes its inputs once and exits.

## Build

```sh
make            # build/tail-toto
make debug      # build/tail-toto-debug (ASan + UBSan)
make test       # build and run build/tests/test_core
make clean      # remove objects and binaries
make install    # optional: install -m 755 build/tail-toto /usr/local/bin
```

All artefacts go under `build/` (`build/tail-toto`, `build/tail-toto-debug`,
`build/tests/test_core`). `build/.gitkeep` and `build/tests/.gitkeep` keep
the directories in git; `make clean` leaves them in place.

### Linux / WSL

```sh
make clean && make && make test
./build/tail-toto /etc/passwd
```

### Windows

Use Git Bash or MSYS2 **UCRT64** with the UCRT64 `gcc`:

```sh
pacman -S make mingw-w64-ucrt-x86_64-gcc   # once, from MSYS2
export PATH="/c/msys64/ucrt64/bin:$PATH"
make clean && make && make test
./build/tail-toto README.md
```

The Makefile probes for `<io.h>` with `_setmode()`. When the probe succeeds
(Windows runtimes), stdin and stdout are switched to binary mode and files are
opened with `O_BINARY`, so bytes such as CRLF pass through unchanged. On Linux
the probe fails and plain POSIX I/O is used. Put `/c/msys64/ucrt64/bin` first
on `PATH` so the probe sees the UCRT64 toolchain.

The result is a native `build/tail-toto.exe` that runs in UCRT64, Git Bash,
cmd, and PowerShell.

## Usage

```sh
./build/tail-toto [OPTION]... [FILE]...
```

With no `FILE`, or when `FILE` is `-`, standard input is read.

```sh
./build/tail-toto file.txt              # last 10 lines
./build/tail-toto -n 3 file.txt         # last 3 lines (also -n3, --lines=3)
./build/tail-toto -n +5 file.txt        # from line 5 to the end
./build/tail-toto -c 100 file.txt       # last 100 bytes (also --bytes=100)
./build/tail-toto -c +10 file.txt       # from byte 10 to the end
seq 1 100 | ./build/tail-toto -n 2      # read standard input
./build/tail-toto a.txt b.txt           # "==> a.txt <==" headers per file
./build/tail-toto -q a.txt b.txt        # -q / --quiet / --silent: no headers
./build/tail-toto -v a.txt              # -v / --verbose: always headers
./build/tail-toto --help                # also --h
./build/tail-toto --version             # also --v
```

- `-v` means verbose, as in GNU `tail`. Version is only `--version` /
  `--v`; `-h` is an invalid option.
- `--help` anywhere on the command line (before `--`) wins over `--version`.
- Short flags can be bundled (`-qn2`); the last `-n` / `-c` and the last
  `-q` / `-v` win. `--` ends option parsing.
- Files that cannot be opened or read are reported and skipped; the remaining
  files are still printed.
- Not supported: `-f` / `-F` / `--follow`, `--retry`, `--pid`, `-s`, `-z`,
  size suffixes such as `K` or `M`, and the obsolete `tail -5` form.

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success: every input processed (or `--help` / `--version`) |
| `1` | Error: usage error, any unreadable input, or write failure |

Diagnostics go to stderr with GNU wording, for example:

```
tail-toto: cannot open 'missing.txt' for reading: No such file or directory
tail-toto: invalid number of lines: 'abc'
tail-toto: write error: No space left on device
```

## Layout

```
tail-toto/
├── LICENSE.txt
├── Makefile
├── README.md
├── c_version.txt
├── build/
│   ├── .gitkeep
│   └── tests/.gitkeep
├── include/   tail_toto.h + tail_toto_{emit,cli,io,tail}.h
├── src/       main.c + tail_toto_{emit,cli,io,tail,run}.c
└── tests/     test_runner.c + test_*.c / .h -> build/tests/test_core
```

On Windows builds the pipe-based unit tests print `SKIP:`; the parsing and
regular-file tests run on every platform.

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

### Syntax reference

Every accepted spelling of each option:

| Purpose | Accepted forms |
|---------|----------------|
| Last `NUM` lines (default 10) | `-n NUM`, `-nNUM`, `--lines=NUM`, `--lines NUM` |
| From line `NUM` to the end | `-n +NUM`, `-n+NUM`, `--lines=+NUM`, `--lines +NUM` |
| Last `NUM` bytes | `-c NUM`, `-cNUM`, `--bytes=NUM`, `--bytes NUM` |
| From byte `NUM` to the end | `-c +NUM`, `-c+NUM`, `--bytes=+NUM`, `--bytes +NUM` |
| Never print headers | `-q`, `--quiet`, `--silent` |
| Always print headers | `-v`, `--verbose` |
| Print usage and exit | `--help`, `--h` |
| Print version and exit | `--version`, `--v` |
| End of options | `--` |
| Read standard input | `-` as a `FILE` operand, or no `FILE` at all |

`NUM` is a decimal number:

- A leading `-` means the same as no sign: `-n -3` is `-n 3`.
- `+0` behaves as `+1` (the whole input); `-n 0` and `-c 0` print nothing.
- Values too large to represent are treated as "everything".
- Size suffixes (`K`, `M`, `b`, ...) are not accepted.

Combining options and files:

```sh
./build/tail-toto -qv a.txt            # bundled short flags
./build/tail-toto -qn 5 a.txt b.txt    # -n / -c may end a bundle ...
./build/tail-toto -qn5 a.txt b.txt     # ... and take the rest of the token
./build/tail-toto a.txt -n 3           # options may follow file names
./build/tail-toto -n 5 -c 20 a.txt     # last -n / -c wins: last 20 bytes
./build/tail-toto -q -v a.txt b.txt    # last -q / -v wins: headers shown
./build/tail-toto a.txt - b.txt        # "-" mixes stdin with files
./build/tail-toto -n 2 -- -n.txt       # after "--", everything is a file
./build/tail-toto -x --help            # help wins, other arguments ignored
```

- `-n` / `-c` take the rest of their token as `NUM`, so they must come last
  in a bundle: `-n5q` is rejected as an invalid number.
- `--help` / `--h` or `--version` / `--v` anywhere before `--` print and exit;
  help wins if both appear. After `--` they are file names.
- `-v` means verbose, as in GNU `tail`. Version is only `--version` /
  `--v`; `-h` is an invalid option.
- Long options must be spelled in full; abbreviations such as `--lin=3` are
  rejected.
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
├── include/   tail_toto.h + tail_toto_{emit,cli,io,tail,run}.h
├── src/       main.c + tail_toto_{emit,cli,io,tail,run}.c
└── tests/     test_runner.c + test_*.c / .h -> build/tests/test_core
```

On Windows builds the pipe-based unit tests print `SKIP:`; the parsing and
regular-file tests run on every platform.

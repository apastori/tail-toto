# tail-toto

A GNU `tail`-compatible command written in C11 (it matches GNU coreutils
`tail` 9.4, except for follow mode). It prints the last part of each file, or
of standard input, to standard output (by default the last 10 lines). Input
and output use raw `read(2)` / `write(2)`; regular files are read backwards
from the end, so only the tail is ever read. There is no follow mode:
`tail-toto` processes its inputs once and exits.

Versions:

- **v1.0.0** (git tag): POSIX-style subset — plain decimal NUM, no suffixes,
  long options spelled in full, `--h` / `--v` shortcuts.
- **v2.0.0** (current): GNU-compatible.

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

| Option | Meaning |
|--------|---------|
| `-n NUM`, `--lines=NUM` | print the last NUM lines (default 10) |
| `-n +NUM`, `--lines=+NUM` | print from line NUM to the end (`+0` is the same as `+1`) |
| `-c NUM`, `--bytes=NUM` | print the last NUM bytes |
| `-c +NUM`, `--bytes=+NUM` | print from byte NUM to the end |
| `-q`, `--quiet`, `--silent` | never print file name headers |
| `-v`, `--verbose` | always print file name headers |
| `-z`, `--zero-terminated` | line delimiter is NUL, not newline |
| `--retry` | accepted; prints a warning and is ignored (no follow mode) |
| `--pid=PID` | accepted; PID is validated, then a warning is printed and it is ignored |
| `-s N`, `--sleep-interval=N` | accepted; N (seconds, may be a decimal) is validated, then ignored |
| `--max-unchanged-stats=N` | accepted; N is validated, then ignored |
| `--help` | show help and exit |
| `--version` | show version and exit |

Options that take a value accept it attached or as the next argument:
`-n3`, `-n 3`, `--lines=3` and `--lines 3` are all the same.

NUM is a decimal number, optionally preceded by spaces, and optionally
followed by a multiplier suffix:

| Suffix | Value | Suffix | Value |
|--------|-------|--------|-------|
| `b` | 512 | | |
| `kB` | 1000 | `K`, `k`, `KiB` | 1024 |
| `MB` | 1000² | `M`, `m`, `MiB` | 1024² |
| `GB` | 1000³ | `G`, `GiB` | 1024³ |

and so on for `T`, `P`, `E`, `Z`, `Y`, `R`, `Q`. A suffix on its own means 1 of
it (`-c K` is 1024 bytes). Numbers too large to represent are rejected with
"Value too large for defined data type".

- A leading `-` on NUM means the same as no sign: `-n -3` is `-n 3`.
- `-n 0` and `-c 0` print nothing and exit 0 without opening any FILE (no
  headers, no "cannot open" errors), as GNU does.
- A leading `+` stays in effect for later `-n` / `-c` options, as in GNU:
  `-n +3 -n 2` prints from line 2.
- The last `-n` / `-c` wins, and the last `-q` / `-v` wins.
- With several FILEs, each one is preceded by `==> NAME <==`, with a blank line
  between files. Standard input is shown as `standard input`.
- Short options can be clustered (`-qv`, `-qn5`). `-n`, `-c` and `-s` take the
  rest of the cluster as their value, so they must come last in it.
- Long options may be shortened to any unique prefix (`--lin=3`, `--h`).
  `--v` is ambiguous (`--verbose` or `--version`), and so is `--s`
  (`--silent` or `--sleep-interval`).
- Options are processed in order: the first `--help` or `--version` wins, and
  an invalid option before it is reported instead.
- `-v` means verbose, not version; `-h` is an invalid option.
- Options can follow FILE names, unless `POSIXLY_CORRECT` is set (then the
  first FILE ends option parsing). `--` ends option parsing.
- Files that cannot be opened or read are reported and skipped; the remaining
  files are still printed.

### Obsolete syntax

The historical forms `-NUM` and `+NUM` work when they are the only option:
alone, with one FILE, or followed by `--` and at most one FILE.

```sh
./build/tail-toto -5 file.txt           # last 5 lines (-n 5)
./build/tail-toto +5 file.txt           # from line 5 (-n +5)
./build/tail-toto -20c file.txt         # last 20 bytes (-c 20)
./build/tail-toto -2b file.txt          # last 2 blocks of 512 bytes (-c 1024)
./build/tail-toto -l file.txt           # NUM defaults to 10
```

The letter after NUM may be `l` (lines, the default), `c` (bytes) or `b`
(512-byte blocks). Anywhere else a digit option is an error
(`option used in invalid context -- 5`), and other words starting with `+`
are file names.

### Examples

```sh
./build/tail-toto notes.txt                 # last 10 lines
./build/tail-toto -n 3 a.txt b.txt          # 3 lines of each, with headers
./build/tail-toto -q -n 3 a.txt b.txt       # same, without headers
./build/tail-toto -n +5 notes.txt           # from line 5 to the end
./build/tail-toto -c 1K data.bin            # last 1024 bytes
./build/tail-toto -c +10 data.bin           # from byte 10 to the end
find . -print0 | ./build/tail-toto -z -n 2  # last 2 NUL-terminated names
seq 100 | ./build/tail-toto -n 5            # from standard input
./build/tail-toto a.txt - b.txt             # "-" mixes stdin with files
./build/tail-toto -n 2 -- -n.txt            # after "--", everything is a file
```

### Differences from GNU tail

- No follow mode: `-f`, `-F` and `--follow` are rejected (GNU ignores `-f`
  when reading a pipe), and prefixes of `--follow` such as `--f` are
  unrecognized options.
- The obsolete forms with `f` (`-5f`, `+5f`) are not recognized.
- `_POSIX2_VERSION` is ignored; tail-toto always behaves like GNU's default.
- GNU's hidden `---presume-input-pipe` option is not supported.
- The program is called `tail-toto`; `--version` prints only
  `tail-toto <version>`, and the `--help` text is its own.
- A failed write is always reported as `write error: <reason>`. GNU reports
  failures of large outputs as `error writing 'standard output'` instead.
- The `<reason>` part of messages comes from the system C library, so it reads
  differently on Windows.

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success: every input processed (or `--help` / `--version`) |
| `1` | Error: usage error, any unreadable input, or write failure |

Diagnostics go to stderr with GNU wording, for example:

```
tail-toto: cannot open 'missing.txt' for reading: No such file or directory
tail-toto: invalid number of lines: 'abc'
tail-toto: option '--v' is ambiguous; possibilities: '--verbose' '--version'
tail-toto: warning: --retry ignored; --retry is useful only when following
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
├── include/   tail_toto.h + tail_toto_{emit,cli,count,io,tail,run}.h
├── src/       main.c + tail_toto_{emit,cli,count,io,tail,run}.c
└── tests/     test_runner.c + test_*.c / .h -> build/tests/test_core
```

On Windows builds the pipe-based unit tests print `SKIP:`; the parsing and
regular-file tests run on every platform.

# gitzip

> Copyright (C) 2026 gitzip contributors
>
> This file is part of gitzip.
>
> gitzip is free software: you can redistribute it and/or modify it under
> the terms of the GNU General Public License as published by the Free Software
> Foundation, either version 3 of the License, or (at your option) any later
> version.

`gitzip` is a C++17 archiver built on top of the vendored `LZMA SDK 26.00`.
It reads `.gitignore` rules, filters the source tree, and creates archives
without depending on a system-installed `7z` command.

## Features

- Archives a directory according to `.gitignore`
- Supports layered `.gitignore` files in nested directories
- Skips Git metadata for nested repositories, including `.git/` directories and `.git` files
- Produces self-contained `7z` archives
- Produces `tar.xz` archives by staging a temporary `tar` and compressing it with the embedded 7-Zip core
- Supports compression level, thread count, solid mode, and extra 7-Zip switches
- Includes a regression test script and GitHub Actions workflow

## Supported Formats

- `7z`
- `xz`

Notes:

- `xz` output is emitted as `.tar.xz` when archiving a directory tree.
- `zip` is not supported in this repository because the vendored SDK does not include the required `ZipHandler` update sources.
- Password protection is available only for `7z`.

## Build

```bash
cmake -S . -B build
cmake --build build -j2
```

## Install

Install into the default system prefix:

```bash
sudo cmake --install build
```

Or with Makefile generators:

```bash
cd build
sudo make install
```

If you want to test installation without touching the system, stage it into a custom prefix:

```bash
cmake --install build --prefix /tmp/gitzip-install
```

After installation, `gitzip` is available on `PATH` if the chosen `bin` directory is on your shell path.

## Test

Run the local regression script:

```bash
bash scripts/test.sh
```

The script validates:

- layered `.gitignore` handling
- nested Git repository metadata exclusion
- `7z` archive creation and contents
- `tar.xz` archive creation and contents
- `xz` password rejection behavior

## Usage

Archive the current directory as `7z`:

```bash
./build/gitzip
```

Archive an explicit source directory:

```bash
./build/gitzip --source . --output release.7z
```

List files that would be archived:

```bash
./build/gitzip --list-only
```

Create a high-compression `7z` archive:

```bash
./build/gitzip --output release.7z --level 9 --threads 8
```

Create a `tar.xz` archive:

```bash
./build/gitzip --format xz --output release.tar.xz
```

Pass extra switches to the embedded 7-Zip core:

```bash
./build/gitzip --output release.7z -- -m0=lzma2 -mx=9 -mfb=273
```

## CLI

- `--source <dir>`: source directory, default is the current directory
- `--output <file>`: output archive path
- `--ignore-file <file>`: root ignore file, default is `<source>/.gitignore`
- `--format <7z|xz>`: archive format
- `--level <1-9>`: compression level
- `--threads <N>`: compression thread count
- `--password <text>`: enable archive encryption for `7z`
- `--no-solid`: disable solid mode for `7z`
- `--list-only`: print the filtered file list without archiving
- `--verbose`: print ignored files and directories
- `--version`: show version and license information
- `-- <7zip switches...>`: pass raw switches to the embedded 7-Zip core
- `--help`: show help

## How Ignore Handling Works

`gitzip` evaluates `.gitignore` rules from the source root and from nested directories.
Rules closer to a file override parent rules, matching standard Git expectations for directory-level ignore files.

For nested repositories:

- `.git/` directories are always excluded
- `.git` files are always excluded
- nested `.gitignore` files are still read and applied to regular files under those directories

The current implementation intentionally does not read:

- `.git/info/exclude`
- global Git ignore configuration

## Third-Party Code

This repository vendors `LZMA SDK 26.00` under [third_party/lzma-sdk-26.00](/home/li/Desktop/playspace/gitzip/third_party/lzma-sdk-26.00).
According to upstream documentation, the SDK is placed in the public domain.
See:

- [third_party/README.md](/home/li/Desktop/playspace/gitzip/third_party/README.md)
- [third_party/lzma-sdk-26.00/DOC/lzma-sdk.txt](/home/li/Desktop/playspace/gitzip/third_party/lzma-sdk-26.00/DOC/lzma-sdk.txt)

## License

gitzip is licensed under GPLv3 or later.
See [COPYING](/home/li/Desktop/playspace/gitzip/COPYING), [LICENSE](/home/li/Desktop/playspace/gitzip/LICENSE), and [NOTICE](/home/li/Desktop/playspace/gitzip/NOTICE).

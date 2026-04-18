# Changelog

Copyright (C) 2026 gitzip contributors

This file is part of gitzip.

gitzip is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, with a simple versioned history for this repository.

## [Unreleased]

### Changed

- Switched the project license from Apache 2.0 to GPLv3 or later and updated repository notices

## [v0.0.1] - 2026-04-17

### Added

- Initial public release of `gitzip`
- Embedded 7-Zip core based on vendored `LZMA SDK 26.00`
- `7z` archive creation without requiring a system-installed `7z` binary
- `tar.xz` output support through temporary tar staging
- Layered `.gitignore` handling across nested directories
- Nested Git repository metadata exclusion for `.git/` directories and `.git` files
- `-h` and `--help` support with a full help page
- Install target for `cmake --install` and `make install`
- Local regression test script at `scripts/test.sh`
- GitHub Actions CI workflow
- Apache 2.0 project licensing with vendored third-party attribution notes

### Notes

- `zip` is not supported in this repository because the vendored SDK does not include the required update-side zip handler sources.
- Password protection is available only for `7z`.

# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, with a simple versioned history for this repository.

## [Unreleased]

- No unreleased changes yet.

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

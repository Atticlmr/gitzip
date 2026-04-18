#!/usr/bin/env bash
# Copyright (C) 2026 gitzip contributors
#
# This file is part of gitzip.
#
# gitzip is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gitzip is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gitzip.  If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BIN="$BUILD_DIR/gitzip"
SDK_7ZR="$ROOT_DIR/third_party/lzma-sdk-26.00/CPP/7zip/Bundles/Alone7z/_o/7zr"

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if ! grep -Fqx -- "$needle" <<<"$haystack"; then
    printf 'assertion failed: expected line not found: %s\n' "$needle" >&2
    exit 1
  fi
}

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  if grep -Fqx -- "$needle" <<<"$haystack"; then
    printf 'assertion failed: unexpected line found: %s\n' "$needle" >&2
    exit 1
  fi
}

assert_output_contains() {
  local haystack="$1"
  local needle="$2"
  if ! grep -Fq -- "$needle" <<<"$haystack"; then
    printf 'assertion failed: expected output fragment not found: %s\n' "$needle" >&2
    exit 1
  fi
}

printf '==> Configuring and building\n'
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j2

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE_DIR="$WORK_DIR/source"
mkdir -p \
  "$SOURCE_DIR/subrepo/.git" \
  "$SOURCE_DIR/subrepo/build" \
  "$SOURCE_DIR/subrepo2" \
  "$SOURCE_DIR/outerdir"

printf 'root-ignore.txt\nouterdir/\n' > "$SOURCE_DIR/.gitignore"
printf 'sub-ignore.txt\nbuild/\n!keep.txt\n' > "$SOURCE_DIR/subrepo/.gitignore"
printf 'child-ignore.txt\n' > "$SOURCE_DIR/subrepo2/.gitignore"

printf 'ignored\n' > "$SOURCE_DIR/root-ignore.txt"
printf 'kept\n' > "$SOURCE_DIR/root-keep.txt"
printf 'hidden\n' > "$SOURCE_DIR/outerdir/hidden.txt"
printf 'ignored\n' > "$SOURCE_DIR/subrepo/sub-ignore.txt"
printf 'kept\n' > "$SOURCE_DIR/subrepo/keep.txt"
printf 'kept\n' > "$SOURCE_DIR/subrepo/data.txt"
printf 'ignored\n' > "$SOURCE_DIR/subrepo/build/generated.txt"
printf 'git metadata\n' > "$SOURCE_DIR/subrepo/.git/config"
printf 'gitdir: ../.git/modules/subrepo2\n' > "$SOURCE_DIR/subrepo2/.git"
printf 'ignored\n' > "$SOURCE_DIR/subrepo2/child-ignore.txt"
printf 'kept\n' > "$SOURCE_DIR/subrepo2/child-keep.txt"

printf '==> Validating list-only output\n'
LIST_OUTPUT="$("$BIN" --source "$SOURCE_DIR" --list-only --verbose)"

assert_output_contains "$LIST_OUTPUT" 'ignored: root-ignore.txt'
assert_output_contains "$LIST_OUTPUT" 'ignored: outerdir/'
assert_output_contains "$LIST_OUTPUT" 'ignored: subrepo/sub-ignore.txt'
assert_output_contains "$LIST_OUTPUT" 'ignored: subrepo/build/'
assert_output_contains "$LIST_OUTPUT" 'ignored: subrepo2/child-ignore.txt'

assert_contains "$LIST_OUTPUT" '.gitignore'
assert_contains "$LIST_OUTPUT" 'root-keep.txt'
assert_contains "$LIST_OUTPUT" 'subrepo/.gitignore'
assert_contains "$LIST_OUTPUT" 'subrepo/data.txt'
assert_contains "$LIST_OUTPUT" 'subrepo/keep.txt'
assert_contains "$LIST_OUTPUT" 'subrepo2/.gitignore'
assert_contains "$LIST_OUTPUT" 'subrepo2/child-keep.txt'
assert_contains "$LIST_OUTPUT" 'Total files: 7'

assert_not_contains "$LIST_OUTPUT" 'root-ignore.txt'
assert_not_contains "$LIST_OUTPUT" 'outerdir/hidden.txt'
assert_not_contains "$LIST_OUTPUT" 'subrepo/sub-ignore.txt'
assert_not_contains "$LIST_OUTPUT" 'subrepo/build/generated.txt'
assert_not_contains "$LIST_OUTPUT" 'subrepo/.git/config'
assert_not_contains "$LIST_OUTPUT" 'subrepo2/.git'
assert_not_contains "$LIST_OUTPUT" 'subrepo2/child-ignore.txt'

printf '==> Validating help and version output\n'
HELP_OUTPUT="$("$BIN" --help)"
VERSION_OUTPUT="$("$BIN" --version)"

assert_output_contains "$HELP_OUTPUT" '--version'
assert_output_contains "$HELP_OUTPUT" 'Report bugs to: https://github.com/Atticlmr/gitzip/issues'
assert_output_contains "$HELP_OUTPUT" 'General help using GNU software: <https://www.gnu.org/gethelp/>'

assert_output_contains "$VERSION_OUTPUT" 'gitzip 0.1.0'
assert_output_contains "$VERSION_OUTPUT" 'License GPLv3+: GNU GPL version 3 or later'

printf '==> Creating and inspecting 7z archive\n'
ARCHIVE_7Z="$WORK_DIR/test.7z"
"$BIN" --source "$SOURCE_DIR" --output "$ARCHIVE_7Z" --level 5
LIST_7Z="$("$SDK_7ZR" l -ba "$ARCHIVE_7Z")"

assert_output_contains "$LIST_7Z" 'root-keep.txt'
assert_output_contains "$LIST_7Z" 'subrepo/data.txt'
assert_output_contains "$LIST_7Z" 'subrepo/keep.txt'
assert_output_contains "$LIST_7Z" 'subrepo2/child-keep.txt'
assert_output_contains "$LIST_7Z" 'subrepo/.gitignore'
assert_output_contains "$LIST_7Z" 'subrepo2/.gitignore'

if grep -Fq 'subrepo/.git/config' <<<"$LIST_7Z"; then
  printf 'assertion failed: git metadata leaked into 7z archive\n' >&2
  exit 1
fi

printf '==> Creating and inspecting tar.xz archive\n'
ARCHIVE_XZ="$WORK_DIR/test.tar.xz"
"$BIN" --source "$SOURCE_DIR" --format xz --output "$ARCHIVE_XZ" --level 5
LIST_XZ="$(tar -tf "$ARCHIVE_XZ")"

assert_contains "$LIST_XZ" '.gitignore'
assert_contains "$LIST_XZ" 'root-keep.txt'
assert_contains "$LIST_XZ" 'subrepo/.gitignore'
assert_contains "$LIST_XZ" 'subrepo/data.txt'
assert_contains "$LIST_XZ" 'subrepo/keep.txt'
assert_contains "$LIST_XZ" 'subrepo2/.gitignore'
assert_contains "$LIST_XZ" 'subrepo2/child-keep.txt'

assert_not_contains "$LIST_XZ" 'subrepo/.git/config'
assert_not_contains "$LIST_XZ" 'subrepo2/.git'
assert_not_contains "$LIST_XZ" 'subrepo/sub-ignore.txt'
assert_not_contains "$LIST_XZ" 'subrepo/build/generated.txt'

printf '==> Verifying xz password rejection\n'
set +e
PASSWORD_OUTPUT="$("$BIN" --source "$SOURCE_DIR" --format xz --password secret 2>&1)"
PASSWORD_STATUS=$?
set -e

if [[ $PASSWORD_STATUS -eq 0 ]]; then
  printf 'assertion failed: xz password invocation unexpectedly succeeded\n' >&2
  exit 1
fi
assert_output_contains "$PASSWORD_OUTPUT" 'Password encryption is only supported for 7z archives'

printf 'All tests passed.\n'

#!/usr/bin/env bash
set -euo pipefail

# ====== CONFIG ======
ROOT="${1:-$PWD}"
OUT="${2:-$ROOT/needed_code.txt}"

INCLUDE_TEST=true
INCLUDE_TINYLIBC=true

EXCLUDE_DIRS=(
  ".git"
  "build"
  ".vscode"
  ".idea"
  "out"
  "dist"
  "node_modules"
)

ALLOW_PREFIXES=(
  "arch/riscv/boot/"
  "arch/riscv/kernel/"
  "arch/riscv/include/"
  "include/os/"
  "init/"
  "kernel/"
  "drivers/"
  "libs/"
  "tools/"
)

if $INCLUDE_TEST; then
  ALLOW_PREFIXES+=("test/")
fi
if $INCLUDE_TINYLIBC; then
  ALLOW_PREFIXES+=("tiny_libc/")
fi

ROOT_FILES=("Makefile" "riscv.lds" "createimage" "README.md" "need.txt")
EXTS=("c" "h" "S" "s" "ld" "lds" "mk" "txt")

is_excluded() {
  local rel="$1"
  for d in "${EXCLUDE_DIRS[@]}"; do
    if [[ "$rel" == "$d/"* || "$rel" == */"$d/"* ]]; then
      return 0
    fi
  done
  return 1
}

is_allowed() {
  local rel="$1"
  for p in "${ALLOW_PREFIXES[@]}"; do
    if [[ "$rel" == "$p"* ]]; then
      return 0
    fi
  done
  return 1
}

has_wanted_ext_or_name() {
  local rel="$1"
  local base ext
  base="$(basename "$rel")"
  ext="${base##*.}"

  if [[ "$base" == "riscv.lds" ]]; then
    return 0
  fi

  for e in "${EXTS[@]}"; do
    if [[ "$ext" == "$e" ]]; then
      return 0
    fi
  done
  return 1
}

mkdir -p "$(dirname "$OUT")"
: > "$OUT"

{
  echo "### DUMP ROOT: $ROOT"
  echo "### GENERATED: $(date '+%F %T')"
  echo "### INCLUDE test/: $INCLUDE_TEST    INCLUDE tiny_libc/: $INCLUDE_TINYLIBC"
} >> "$OUT"

tmp_list="$(mktemp)"
trap 'rm -f "$tmp_list"' EXIT

for f in "${ROOT_FILES[@]}"; do
  if [[ -f "$ROOT/$f" ]]; then
    echo "$f" >> "$tmp_list"
  fi
done

while IFS= read -r rel; do
  if is_excluded "$rel"; then
    continue
  fi
  if is_allowed "$rel" && has_wanted_ext_or_name "$rel"; then
    echo "$rel" >> "$tmp_list"
  fi
done < <(
  cd "$ROOT"
  find . \
    $(for d in "${EXCLUDE_DIRS[@]}"; do echo -n " -path ./$d -prune -o"; done) \
    -type f -print \
  | sed 's|^\./||'
)

sort -u "$tmp_list" -o "$tmp_list"

file_count="$(wc -l < "$tmp_list" | tr -d ' ')"
echo "### FILE COUNT: $file_count" >> "$OUT"
echo "" >> "$OUT"

while IFS= read -r rel; do
  echo "===== $rel =====" >> "$OUT"
  full="$ROOT/$rel"
  if [[ -r "$full" ]]; then
    cat "$full" >> "$OUT"
  else
    echo "[[ERROR: failed to read $rel]]" >> "$OUT"
  fi
  echo "" >> "$OUT"
done < "$tmp_list"

echo "Done. Output: $OUT"

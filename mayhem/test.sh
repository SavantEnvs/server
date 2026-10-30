#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Runs MariaDB's own TAP unit tests for the low-level libraries (strings, dbug, mysys, json_lib,
# my_decimal, dynamic columns), pre-built by build.sh in mayhem/build-test with the project's normal flags.
# Each test binary asserts known-answer values (`ok N - ...` / `not ok N - ...`); a binary counts as passing
# only if it exits 0, prints a `1..N` plan, and reports exactly N results with none failing, so a neutered
# binary that just exits 0 fails here.
TEST_B="$SRC/mayhem/build-test/unittest"
TESTS="strings/strings-t strings/json-t json_lib/json_lib-t json_lib/json_normalize-t my_decimal/my_decimal-t
       mysys/base64-t mysys/ma_dyncol-t mysys/my_vsnprintf-t mysys/dynstring-t mysys/dynamic_array-t
       mysys/bitmap-t mysys/crc32-t mysys/byte_order-t mysys/my_getopt-t mysys/queues-t"

passed=0 failed=0 skipped=0
for t in $TESTS; do
  bin="$TEST_B/$t"
  if [ ! -x "$bin" ]; then echo "missing test binary $bin (mayhem/build.sh should have built it)" >&2; failed=$((failed+1)); continue; fi
  out=$(cd "$(dirname "$bin")" && "./$(basename "$bin")" 2>&1); rc=$?
  plan=$(printf '%s\n' "$out" | sed -n 's/^1\.\.\([0-9][0-9]*\)$/\1/p' | head -1)
  p=$(printf '%s\n' "$out" | grep -Ec '^ok [0-9]+' || true)
  s=$(printf '%s\n' "$out" | grep -Ec '^ok [0-9]+.*# skip' || true)
  f=$(printf '%s\n' "$out" | grep -Ec '^not ok [0-9]+' || true)
  if [ "$rc" -ne 0 ] || [ -z "$plan" ] || [ $((p + f)) -ne "$plan" ]; then
    echo "FAIL $t: rc=$rc plan=${plan:-none} ok=$p not_ok=$f" >&2
    printf '%s\n' "$out" | grep -E '^not ok' | head -5 >&2
    [ "$f" -gt 0 ] || f=1
  fi
  passed=$((passed + p - s)); skipped=$((skipped + s)); failed=$((failed + f))
  echo "$t: ok=$p skip=$s not_ok=$f"
done

emit_ctrf "mariadb-tap" "$passed" "$failed" "$skipped"

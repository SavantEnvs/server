#!/usr/bin/env bash
#
# mayhem/build.sh — build the fuzz harnesses (sanitized) and the project's unit tests (normal flags).
#
# Runs inside the commit image as `mayhem` in /mayhem. Only the server's low-level libraries
# (strings, dbug, mysys) are fuzzed; the rest of the tree needs the full server build and is not
# reachable from these harnesses. Configures with WITHOUT_SERVER so no server plugin/engine is built.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
# libFuzzer coverage feedback inside the fuzzed libraries (the fuzzer-no-link runtime hooks come from ASan when standalone).
FUZZ_COV="-fsanitize=fuzzer-no-link"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"

# The connector is a git submodule the config step needs. Fetched once, online, at image build;
# the air-gapped re-run finds it already checked out.
if [ ! -f libmariadb/CMakeLists.txt ]; then
  git submodule update --init libmariadb
fi

COMMON=(-G Ninja
        -DWITHOUT_SERVER=ON -DWITH_UNIT_TESTS=ON
        -DWITH_SSL=system -DWITH_ZLIB=system -DWITH_PCRE=system -DWITH_WSREP=OFF
        -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX")

# Reconfiguring on top of an existing CMakeCache.txt makes wsrep.cmake ignore -DWITH_WSREP=OFF and try to clone
# the wsrep-lib submodule (fails offline); dropping the cache keeps re-runs air-gapped. Objects are kept.
# 1) Sanitized libraries the harnesses link against.
# pointer-overflow is relaxed (everything else, incl. the rest of UBSan and ASan, stays halting): its only hit
# is the benign "applying zero offset to null pointer" in lazy collation init (strings/ctype-uca.c,
# my_uca_contraction_find), reached from any blob-supplied charset number.
RELAX="-fno-sanitize=pointer-overflow"
FUZZ_B="$SRC/mayhem/build-fuzz"
mkdir -p "$FUZZ_B"
rm -f "$FUZZ_B/CMakeCache.txt"
cmake -S "$SRC" -B "$FUZZ_B" "${COMMON[@]}" \
      -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $RELAX $DEBUG_FLAGS $FUZZ_COV" \
      -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS $RELAX $DEBUG_FLAGS $FUZZ_COV" > "$FUZZ_B/configure.log" 2>&1 \
  || { tail -50 "$FUZZ_B/configure.log"; exit 1; }
cmake --build "$FUZZ_B" -j"$MAYHEM_JOBS" --target strings dbug mysys

# 2) Harnesses: a libFuzzer binary and a run-once -standalone reproducer each.
INCLUDES=(-I"$SRC/include" -I"$FUZZ_B/include" -I"$SRC/include/providers" -I"$SRC/sql" -I"$SRC/mayhem/harnesses")
LIBS=(-Wl,--start-group "$FUZZ_B/mysys/libmysys.a" "$FUZZ_B/strings/libstrings.a" "$FUZZ_B/dbug/libdbug.a" -Wl,--end-group -lz -ldl -lpthread)

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$FUZZ_B/lsan_off.o"
$CC  $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_B/standalone_main.o"

for h in fuzz_json fuzz_json_doc fuzz_json_path fuzz_xml fuzz_decimal fuzz_dyncol; do
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $FUZZ_COV -O1 "${INCLUDES[@]}" -c "$SRC/mayhem/harnesses/$h.c" -o "$FUZZ_B/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $FUZZ_COV $LIB_FUZZING_ENGINE "$FUZZ_B/$h.o" "$FUZZ_B/lsan_off.o" "${LIBS[@]}" -o "$SRC/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $FUZZ_COV "$FUZZ_B/$h.o" "$FUZZ_B/standalone_main.o" "$FUZZ_B/lsan_off.o" "${LIBS[@]}" -o "$SRC/$h-standalone"
done

# 3) The project's own unit tests (TAP), project-normal flags, independent of the sanitized build.
TEST_B="$SRC/mayhem/build-test"
mkdir -p "$TEST_B"
rm -f "$TEST_B/CMakeCache.txt"
cmake -S "$SRC" -B "$TEST_B" "${COMMON[@]}" \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" > "$TEST_B/configure.log" 2>&1 \
  || { tail -50 "$TEST_B/configure.log"; exit 1; }
cmake --build "$TEST_B" -j"$MAYHEM_JOBS" --target \
      strings-t json-t json_lib-t json_normalize-t base64-t ma_dyncol-t my_decimal-t my_vsnprintf-t \
      dynstring-t dynamic_array-t bitmap-t crc32-t byte_order-t my_getopt-t queues-t

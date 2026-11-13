# Continuous integration

All CI lives in one file: `.github/workflows/ci.yml` (GitHub Actions).

## 1. What runs

CI runs on every push, on every pull request, and on demand from the Actions tab ("Run workflow"). A newer run on the same branch cancels an older one that is still running. Three jobs run in parallel on Ubuntu 24.04:

- **host-build**: builds skylink with CMake on Linux and runs the C unit tests in `build/tests/units2/unit_tests2`. Uploads the test output as `unit-test-output`.
- **python-tests**: runs the pytest suite in `python/tests` (frame parser and constructor). Uploads `pytest-results` (JUnit XML).
- **cross-compile**: compiles every source in `src/CMakeLists.txt` with `arm-none-eabi-gcc` for the STM32L476 (Cortex-M4F), with `SKY_DEBUG` defined as in `src/CMakeLists.txt`, checks compiler warnings against a baseline, and writes a flash size table to the job's summary page. Uploads `cross-compile-report` (warnings log and size table).

Artifacts are on the run's page in the Actions tab, under "Artifacts".

## 2. How results work

Each job compares its results to a list of known issues in `ci.yml`:

| What happens | What CI shows |
|---|---|
| A known issue is still there | Yellow **warning** annotation, job stays green |
| A new test failure, or a new or increased compiler warning | Red **error** annotation, job fails |
| A known issue is fixed | Blue **notice** saying to update `ci.yml`, job stays green |
| Tests crash or produce no results | Red error, job fails |

Annotations appear at the top of the run's page. A green job can still carry warnings, so read them.

## 3. Current known issues

**C unit tests (host-build)**
- `both_hmac_and_crc` fails: CRC32 and HMAC cannot be used together. See the TODO at `src/skylink_rx.c:144`.

**Python tests (python-tests)**
- `test_parse.py::test_parsing`, `test_variable_identity_lengths`, `test_parse_extensions` fail: the parser's header layout does not match the C code, and it slices an 8-byte auth tag where the real tag is 4 bytes (`python/skylink/`).
- `test_construct.py::test_hmac` is flaky, failing in about 1 of 3 runs: `corrupt()` in `python/tests/utils.py` flips random bits without a seed. A flaky failure is a warning, never a red job.

**Cross-compile warnings (cross-compile), 11 warnings in 6 groups**
- Tick signedness (8 warnings): `sky_tick_t` is `uint32_t` on FreeRTOS but `int32_t` on Linux. The clamp `peer_mac_remaining < 0` at `src/mac.c:196` is always false on target (`-Wtype-limits`), plus 5 `-Wsign-compare` in `src/mac.c` and 2 in `src/reliable_vc.c`.
- Missing `<stdlib.h>` (2 warnings): `src/utilities.c:133` calls `abs()` without the include (`-Wimplicit-function-declaration`, `-Wbuiltin-declaration-mismatch`).
- Vendored BLAKE3 (1 warning): `get_cpu_features` is unused on non-x86 in `src/ext/blake3/blake3_dispatch.c` (`-Wunused-function`).

## 4. Updating the baselines

When you fix a known issue, CI shows a notice. Edit `.github/workflows/ci.yml` in the same pull request:

| Job | Find | Edit |
|---|---|---|
| host-build | `KNOWN=both_hmac_and_crc` | Remove the known-failure handling once the test passes. The notice tells you when. |
| python-tests | `KNOWN = {` | Delete the fixed test's name from the set. |
| python-tests | `FLAKY = {` | Delete `test_hmac` once the test is seeded or made deterministic. |
| cross-compile | `BASELINE = {` | Each entry is `"file [-Wflag]": (count, "reason")`. Lower the count, or delete the entry when it reaches 0. |

Warnings are counted per file and warning type, not per line, so editing unrelated code does not change the baseline. Never raise a count to make CI green. Fix the new warning, or discuss it in review.

A listed Python test that no longer exists (deleted or renamed) produces a "not found in results" warning. Remove it from the list.

## 5. Running the same checks locally

Run all commands from the repository root on Ubuntu 24.04 (or similar). CI then judges the output against the known issues above. Locally you compare by eye.

**C build and unit tests.** Expect `1 failed, 91 passed, 92 total`, with `both_hmac_and_crc` failing.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libzmq3-dev
cmake -S . -B build && cmake --build build -j
build/tests/units2/unit_tests2
```

**Python tests.** Uses the same pinned versions as CI. Expect the 3 known `test_parse.py` failures, and sometimes `test_hmac`.

```bash
sudo apt-get install -y python3-venv
cd python
python3 -m venv .venv
.venv/bin/pip install construct==2.10.70 blake3==1.0.11 pytest==9.1.1
PYTHONPATH=.:tests .venv/bin/pytest -v tests
cd ..
```

**Cross-compile and sizes.** Built with `SKY_DEBUG`, as defined in `src/CMakeLists.txt`. Expect the 11 warnings listed above and a total of `25975` text, `4` data, `5` bss.

```bash
sudo apt-get install -y --no-install-recommends gcc-arm-none-eabi libnewlib-arm-none-eabi
mkdir -p /tmp/skylink-stubs /tmp/skylink-obj
printf '#include <stddef.h>\nvoid *pvPortMalloc(size_t size);\nvoid vPortFree(void *ptr);\n#define configASSERT(x) ((void)(x))\n' > /tmp/skylink-stubs/FreeRTOS.h
printf 'int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...);\n' > /tmp/skylink-stubs/SEGGER_RTT.h
for f in $(sed -n '/target_sources/,/)/p' src/CMakeLists.txt | grep -o '"[^"]*\.c"' | tr -d '"'); do
  mkdir -p "/tmp/skylink-obj/$(dirname "$f")"
  arm-none-eabi-gcc -DSKY_DEBUG -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -std=gnu11 -Os -Wall -Wextra \
    -ffunction-sections -fdata-sections \
    -Isrc -Iplatforms/freertos -I/tmp/skylink-stubs \
    -c "src/$f" -o "/tmp/skylink-obj/${f%.c}.o"
done
find /tmp/skylink-obj -name '*.o' | sort | xargs arm-none-eabi-size -t
```

If your machine is missing tools beyond these packages, run the commands inside `docker run --rm -it -v "$PWD":/src -w /src ubuntu:24.04` after `apt-get update`, dropping `sudo`.

## 6. Assumptions and limits

- **Target MCU.** STM32L476VGTx (Arm Cortex-M4 with single-precision FPU), the Aalto-3 flight MCU. The cross-compile uses `-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16`. The "no division instruction" comment in `src/utilities.c` refers to older hardware, not Aalto-3.
- **Float ABI.** The float ABI is intentionally set by each consuming project's toolchain, not by skylink. CI uses `-mfloat-abi=hard` as a representative STM32L476 setting. skylink has no floating-point code, but the setting still changes the compiled objects: with an FPU enabled, GCC can use FPU registers as spill space and allocates registers differently. Measured with `SKY_DEBUG`: `hard` and `softfp` both give 25975 B text, `soft` (no FPU) gives 26083 B. Objects built with different float ABIs cannot be linked together, so the firmware must build skylink with its own flags.
- **`SKY_DEBUG`.** `src/CMakeLists.txt` defines `SKY_DEBUG` as `PUBLIC`, so any CMake build of skylink, including the firmware's, compiles with it. CI does the same. A build without `SKY_DEBUG` is not checked.
- **Stub headers.** `FreeRTOS.h` and `SEGGER_RTT.h` are generated by the job. They declare only what `platforms/freertos/sky_platform.h` uses with `SKY_DEBUG`: `pvPortMalloc`, `vPortFree`, `configASSERT` (evaluates its argument and does nothing) and `SEGGER_RTT_printf`. Asserts and debug prints are therefore compiled but never fire.
- **Compile only, no link.** Missing symbols or linker problems on target are not detected.
- **Pinned Python versions.** `construct 2.10.70`, `blake3 1.0.11`, `pytest 9.1.1`, on the runner's system Python 3.12. Newer versions are not tested.
- **Sizes are per object file, before linking.** The real firmware differs: the linker drops unused sections and adds libc and FreeRTOS. Heap use (`SKY_MALLOC`) is not included. Use the totals to track changes, not as the flash footprint.

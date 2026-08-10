#!/usr/bin/env python3
"""Include-what-you-use checker built on clang-include-cleaner.

Checks that every source and header directly includes what it uses, with one
family-specific relaxation: a foo.cpp may rely on headers its matching foo.h
already includes directly (the pair is maintained together, so restating the
header's includes in the cpp is churn without benefit).

ESP32 handling: library code keeps ESP-IDF includes behind #ifdef ESP_PLATFORM
or in ESP-only directories, which host tooling normally can't parse. Instead
of cross-compiling, each file is analyzed up to twice -- a host pass with no
extra defines (ESP branches are preprocessed away) and an ESP pass with
-DESP_PLATFORM plus materialized stub headers on the include path (host-only
branches are preprocessed away; see esp_stubs.py). The stubs declare just the ESP-IDF symbols this
library family uses; their file names match the real headers, so symbol
attribution resolves to the same include line a real ESP-IDF build would want.

Escape hatches: the engine honors IWYU pragmas (// IWYU pragma: keep, export,
private). sdkconfig.h is never reported: its CONFIG_* macros are invisible to
usage analysis when unset in the default configuration.

Known limitation: the engine never suggests removing C-compatibility headers
(<cstring>, <cstdint>, ...), only pure C++ ones -- an unused <cstring> goes
unreported. It errs toward false negatives; anything it does report is real.

Usage: check_includes.py [--fix]
  --fix  apply unused-include removals in place (missing includes are always
         reported for manual fixing; automatic insertion cannot honor the
         matching-header relaxation). Edits are applied only after every
         analysis completes, and a host+esp dual-checked file is only edited
         when both passes agree on the removals.

Requires clang-include-cleaner (apt: clang-tools-18, brew: llvm).
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys

import esp_stubs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- Per-repo configuration --------------------------------------------------

# Host compile database, generated on demand into a directory owned by this
# tool -- sharing an IDE/dev build dir risks reading a db mid-regeneration or
# inheriting a stale CMake cache (e.g. an SDK path from before an Xcode
# upgrade). Files without an entry get flags interpolated from the nearest one.
BUILD_DIR = os.path.join(ROOT, "build", "check_includes")
CMAKE_SOURCE = os.path.join(ROOT, "host_examples", "opus_to_wav")
CMAKE_ARGS = []

# Directories checked in both passes (portable code with guarded ESP branches),
# only on host (host tools/tests), and only as ESP (ESP-IDF example apps).
CHECK_BOTH = ["src", "include"]
CHECK_HOST_ONLY = ["host_examples", "tests"]
CHECK_ESP_ONLY = ["examples"]

# Skip paths containing any of these segments (build trees, vendored code).
EXCLUDE_SEGMENTS = {"build", ".pio", "managed_components", "cmake-build"}
# Skip these repo-relative directories entirely.
# The benchmark examples are ESP-IDF firmware apps: they pull in ESP-IDF runtime
# APIs and libopus headers whose include graph is reconstructed by the real
# idf.py build, not by the host compile DB / shared esp_stubs.py. clang-include-
# cleaner can't compile them on the host (opus.h unresolved, esp_* symbols
# undeclared), so they're excluded here -- the library's own ESP branches under
# src/ are still checked via the esp pass. tests/qemu is the same case: an
# ESP-IDF firmware app built by PlatformIO, not by the host compile DB.
# tests/tools is excluded for a different reason -- measure_zerocopy.cpp is an
# opt-in instrument that #errors unless MICRO_OGG_DEMUXER_DEBUG is defined and
# calls debug-only APIs absent from a default build, so it cannot be compiled
# in the configuration this checker analyzes.
EXCLUDE_DIRS = [
    "examples/decode_benchmark",
    "examples/encode_benchmark",
    "tests/qemu",
    "tests/tools",
]
# Skip files matching any of these basename regexes (generated data headers).
EXCLUDE_BASENAMES = [r"^test_audio_.*\.h$"]

# Headers the engine must not report (suffix regexes for --ignore-headers).
# opus_types.h / opus_defines.h define symbols (opus_int32, OPUS_OK, ...) that
# the wrapper uses, but they are pulled in by "opus.h" -- libopus's documented
# public umbrella, which every source already includes. We can't add IWYU
# export pragmas to the vendored headers, so the umbrella is expressed here.
IGNORE_HEADERS = [
    "sdkconfig.h",
    "stdlib.h",
    "_abort.h",
    "_endian.h",
    "opus_defines.h",
    "opus_types.h",
]

# Project include roots appended to every invocation. Files without a
# compile-db entry (headers, ESP-only sources, tests the build didn't compile)
# get flags interpolated from the nearest entry, which may lack these roots.
# Duplicates of roots already in an entry's flags are harmless.
# tests/support holds the shared in-memory Ogg muxing helpers; the unit tests
# include them by bare name, and CTest adds the root only for its own targets.
EXTRA_INCLUDE_DIRS = ["src", "include", "tests/support"]

# Extra compiler args appended to every invocation, e.g. ["-xc++", "-std=gnu++14"]
# to force C++ when the compile db mixes C and C++ commands and flag
# interpolation for a header could pick a C entry. Empty for pure-C++ repos.
# tests/support/ogg_mux.h has no compile-db entry of its own and interpolates
# to a C entry, which loses the C++ standard library search paths, so C++ is
# forced here.
EXTRA_CLANG_ARGS = ["-xc++"]

SOURCE_EXTS = {".cpp", ".cc", ".c"}
HEADER_EXTS = {".h", ".hpp"}

# Stub headers are materialized from esp_stubs.py into the tool-owned build
# dir at startup; only the data module is checked in.
STUB_DIR = os.path.join(BUILD_DIR, "esp_stubs")

# ------------------------------------------------------------------------------

CHANGE_RE = re.compile(r"^([+-])\s+(.+?)(?:\s+@Line:(\d+))?$")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def find_binary():
    """Locate clang-include-cleaner. $CLANG_INCLUDE_CLEANER (CI pins the
    apt-versioned name) wins over PATH discovery and Homebrew fallbacks."""
    override = os.environ.get("CLANG_INCLUDE_CLEANER")
    if override:
        found = shutil.which(override)
        if not found:
            sys.exit(f"error: $CLANG_INCLUDE_CLEANER not executable: {override}")
        return found
    names = ["clang-include-cleaner"] + [
        f"clang-include-cleaner-{v}" for v in (21, 20, 19, 18)
    ]
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    for path in (
        "/opt/homebrew/opt/llvm/bin/clang-include-cleaner",
        "/usr/local/opt/llvm/bin/clang-include-cleaner",
    ):
        if os.access(path, os.X_OK):
            return path
    sys.exit(
        "error: clang-include-cleaner not found "
        "(apt-get install clang-tools-18 / brew install llvm)"
    )


def materialize_stubs():
    for rel, content in esp_stubs.STUBS.items():
        dst = os.path.join(STUB_DIR, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        try:
            with open(dst, encoding="utf-8") as f:
                if f.read() == content:
                    continue
        except OSError:
            pass
        with open(dst, "w", encoding="utf-8") as f:
            f.write(content)


def ensure_compile_db():
    if os.path.isfile(os.path.join(BUILD_DIR, "compile_commands.json")):
        return
    print("Generating compile_commands.json...")
    subprocess.run(
        ["cmake", "-B", BUILD_DIR, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
        + CMAKE_ARGS + [CMAKE_SOURCE],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def gather_files(dirs, prune=()):
    """Collect checkable files under dirs. prune lists the other categories'
    roots: a nested root (e.g. src/esp under src) belongs only to its own
    category's walk."""
    exclude_res = [re.compile(p) for p in EXCLUDE_BASENAMES]
    exclude_dirs = {os.path.join(ROOT, d) for d in EXCLUDE_DIRS}
    exclude_dirs |= {os.path.join(ROOT, d) for d in prune}
    files = []
    for d in dirs:
        top = os.path.join(ROOT, d)
        if not os.path.isdir(top):
            continue
        for dirpath, dirnames, filenames in os.walk(top):
            dirnames[:] = [
                n for n in dirnames
                if n not in EXCLUDE_SEGMENTS
                and os.path.join(dirpath, n) not in exclude_dirs
            ]
            for name in sorted(filenames):
                ext = os.path.splitext(name)[1]
                if ext not in SOURCE_EXTS | HEADER_EXTS:
                    continue
                if any(r.match(name) for r in exclude_res):
                    continue
                files.append(os.path.join(dirpath, name))
    return files


def matching_headers(cpp_path, headers):
    """Return the checked headers pairing with cpp_path: same stem, or the
    family's private-implementation variant (foo.cpp pairs with both foo.h
    and foo_impl.h). The pair is maintained together, so the cpp may rely on
    any of their direct includes."""
    stem = os.path.splitext(os.path.basename(cpp_path))[0]
    stems = {stem, stem + "_impl"}
    return [h for h in headers if os.path.splitext(os.path.basename(h))[0] in stems]


def direct_includes(path):
    """All #include targets textually present in the file (both branches of
    any #if -- deliberately conservative for the relaxation check)."""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return set(INCLUDE_RE.findall(f.read()))
    except OSError:
        return set()


def run_engine(binary, path, esp_pass, fix_removals=False):
    """Run one file through clang-include-cleaner. Returns (inserts, removes,
    error). inserts/removes are lists of display strings."""
    cmd = [binary, "-p", BUILD_DIR, "--ignore-headers=" + ",".join(IGNORE_HEADERS)]
    if fix_removals:
        cmd += ["--edit", "--disable-insert"]
    else:
        cmd += ["--print=changes"]
    for d in EXTRA_INCLUDE_DIRS:
        cmd.append(f"--extra-arg=-I{os.path.join(ROOT, d)}")
    for arg in EXTRA_CLANG_ARGS:
        cmd.append(f"--extra-arg={arg}")
    if esp_pass:
        cmd += ["--extra-arg=-DESP_PLATFORM", f"--extra-arg=-isystem{STUB_DIR}"]
    cmd.append(path)
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)

    stderr = proc.stderr
    if proc.returncode != 0 or "Skipping file" in stderr or "error:" in stderr:
        # Surface only the interesting lines, not clang's full include stacks.
        lines = [
            ln
            for ln in stderr.splitlines()
            if "error:" in ln or "Skipping file" in ln
        ]
        return [], [], "\n".join(lines[:5]) or f"exit code {proc.returncode}"

    inserts, removes = [], []
    for line in proc.stdout.splitlines():
        m = CHANGE_RE.match(line.strip())
        if not m:
            continue
        sign, header, lineno = m.groups()
        if sign == "+":
            inserts.append(header)
        else:
            removes.append(f"{header} (line {lineno})" if lineno else header)
    return inserts, removes, None


def spelling(header_display):
    """Normalize a suggestion like '<vector>' or '"foo.h"' to its path text."""
    return header_display.strip().strip('<>"')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="apply removals in place")
    args = parser.parse_args()

    binary = find_binary()
    ensure_compile_db()
    materialize_stubs()

    both = gather_files(CHECK_BOTH, prune=CHECK_HOST_ONLY + CHECK_ESP_ONLY)
    host_only = gather_files(CHECK_HOST_ONLY, prune=CHECK_ESP_ONLY)
    esp_only = gather_files(CHECK_ESP_ONLY, prune=CHECK_HOST_ONLY)

    headers = [f for f in both + host_only + esp_only
               if os.path.splitext(f)[1] in HEADER_EXTS]

    jobs = [(f, False) for f in both + host_only] + [(f, True) for f in both + esp_only]
    print(f"check-includes: {len(both) + len(host_only) + len(esp_only)} files, "
          f"{len(jobs)} checks ({os.path.basename(binary)})")

    failures = 0
    errors = 0
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 1) as pool:
        # Materialize every result before consuming any: --fix edits files in
        # place from the main thread, which must not happen while a worker may
        # still be analyzing the same file (dual-checked files are two jobs).
        results = list(pool.map(
            lambda job: (job, run_engine(binary, job[0], job[1])), jobs
        ))

    dual_checked = set(both)
    fix_requests = {}  # path -> {esp_pass: removes reported by that pass}
    for (path, esp_pass), (inserts, removes, error) in results:
        rel = os.path.relpath(path, ROOT)
        tag = "esp" if esp_pass else "host"
        if error:
            errors += 1
            print(f"ERROR {rel} [{tag}]: could not analyze\n  {error}")
            continue

        # Relaxation: a cpp may rely on its matching headers' direct
        # includes -- including the matching headers themselves (foo.cpp
        # reaching foo.h through foo_impl.h is fine).
        if inserts and os.path.splitext(path)[1] in SOURCE_EXTS:
            partners = matching_headers(path, headers)
            covered = set()
            for partner in partners:
                covered |= direct_includes(partner)
                covered.add(os.path.relpath(partner, ROOT))
            if covered:
                inserts = [
                    h for h in inserts
                    if spelling(h) not in covered
                    and not any(p.endswith(os.sep + spelling(h)) for p in covered)
                ]

        if not inserts and not removes:
            continue
        failures += 1
        print(f"FAIL {rel} [{tag}]")
        for h in inserts:
            print(f"  missing include: {h}")
        for h in removes:
            print(f"  unused include:  {h}")
        if args.fix and removes:
            fix_requests.setdefault(path, {})[esp_pass] = removes

    # Apply removals only now, with all analyses done. A dual-checked file is
    # edited only when both passes reported the same removals: --edit applies
    # one pass's whole view, and an include that only one pass calls unused may
    # be load-bearing (or invisible, if guarded) in the other.
    for path, per_pass in sorted(fix_requests.items()):
        rel = os.path.relpath(path, ROOT)
        if path in dual_checked and per_pass.get(False) != per_pass.get(True):
            print(f"skipped fix {rel}: host/esp passes disagree on removals; "
                  "fix manually")
            continue
        esp_pass = False if False in per_pass else True
        _, _, fix_err = run_engine(binary, path, esp_pass, fix_removals=True)
        print(f"fixed {rel}: removals applied" if not fix_err
              else f"fix failed {rel}: {fix_err}")

    if errors:
        print(f"\ncheck-includes: {errors} file(s) could not be analyzed "
              "(missing stub symbol? see script/esp_stubs.py)")
    if failures:
        print(f"check-includes: {failures} failing check(s)")
    if errors or failures:
        return 1
    print("check-includes passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

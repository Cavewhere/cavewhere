# AddressSanitizer (ASAN) on macOS Debug

CaveWhere and its submodules (`CMakeLists.txt`, `QQuickGit`, `monad`,
`sketch`) compile with `-fsanitize=address` on macOS Debug builds. This is
controlled by the `ASAN_ENABLED` CMake option, which defaults to `ON`.

## The PROJ container-overflow problem

With libc++, `-fsanitize=address` enables **container annotations**: ASAN
poisons the gap between a `std::vector`/`std::string`'s size and its capacity
so out-of-bounds access into that gap is caught.

PROJ is a Conan dependency built as a static `libproj.a` **without** ASAN. Its
inlined container operations don't update those annotations. When a container
crosses the instrumented (CaveWhere) ↔ non-instrumented (PROJ) boundary, the
annotations disagree and ASAN reports a bogus `container-overflow`.

The common workaround, `ASAN_OPTIONS=detect_container_overflow=0`, disables the
check **globally** — you lose real container-overflow detection everywhere. The
fix is to build PROJ with ASAN too, so the annotations stay consistent. See
issue #580.

## How the macOS Debug build gets its Conan packages

Unlike iOS/Android (where you run `conan install` by hand), the macOS kit uses
**Qt Creator's built-in Conan integration**. On each configure it regenerates
`build/<dir>/conan-dependencies/conan_host_profile` and runs:

```
conan install ... -pr <conan_host_profile> --build=missing -s build_type=Debug
```

You can't durably hand-edit that generated profile, and the ASAN conf only
reaches Conan if it's in the profile Qt Creator uses. But there's a lever:
`tools.build:cxxflags` is **not** part of a package's `package_id`. So an
ASAN-instrumented `libproj.a` built with the *same* settings lands in the
**same cache slot** the plain build would use — and Qt Creator's
`--build=missing` then reuses it instead of rebuilding a non-ASAN PROJ.

## Seed the cache with an ASAN PROJ

Run this once (repeat if the PROJ binary is ever evicted — see caveat). It
layers `profiles/proj-asan` (sanitizer flags scoped to `proj/*`) on top of the
exact profile Qt Creator generated, so the `package_id` is unchanged and only
PROJ is instrumented:

```bash
conan install . -o "&:system_qt=True" \
    -pr:h build/<your-macos-debug-dir>/conan-dependencies/conan_host_profile \
    -pr:h profiles/proj-asan -pr:b default \
    --build="proj/*" \
    -of $(mktemp -d)
```

Use the `--build="proj/*"` pattern (a bare `--build=proj` is treated as "already
installed" and does **not** force a rebuild). The `-of` output is throwaway —
only the Conan cache matters here. Because `libproj.a` is static and links into
the already-ASAN executable, there is no separate ASAN runtime to manage.

Verify it worked (a plain build has 0):

```bash
PKG=$(conan cache path proj/9.3.1:e70e2fbc7a15406b0489701426a1c63c843153d3)
nm -u "$PKG/lib/libproj.a" | grep -c __asan_    # thousands = instrumented
```

The `package_id` above is for the current macOS Debug config; if it changes,
read it from `conan graph info . -pr:h <profile> -pr:h profiles/proj-asan
--filter=package_id`.

## Configure and run

1. In Qt Creator, make sure the Debug kit configures with `-DASAN_ENABLED=ON`
   (the default; it is currently `OFF` in the cache — flip it back).
2. Reconfigure. Qt Creator's `conan install --build=missing` finds the seeded
   ASAN PROJ and reuses it.
3. Build and run **without** `ASAN_OPTIONS=detect_container_overflow=0` — the
   PROJ false positive is gone.

## Caveat: re-seed after eviction

Because the ASAN-ness is not encoded in the `package_id`, the instrumented and
plain PROJ share one cache slot. If that binary is ever evicted (`conan cache
clean`, a fresh cache, or an explicit `--build="proj/*"` without this profile),
Qt Creator's next `--build=missing` will rebuild a **non-ASAN** PROJ and the
false positive returns. Just re-run the seed command above to restore it.

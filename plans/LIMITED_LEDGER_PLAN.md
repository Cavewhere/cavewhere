# limited() follow-up: optional weight, deduced T, shipped Ledger

## Context

Commit `9dab7a1` in the `asyncfuture` submodule added `limited()`, a windowed
admission helper that runs one launcher per input while a count limit, a
per-run byte budget, and an optional process-wide `Ledger` hold. The code
review of that commit found no correctness bug but surfaced four API rough
edges and a list of untested paths. This plan covers the agreed changes.

Decisions already made in review:

- `weight` becomes optional for count-only use.
- `T` is deduced from the launcher's return type.
- `Ledger` stays abstract and replaceable, but the library ships a concrete
  `ByteLedger`. `Ledger` becomes a `QObject` so it can wake waiting runs.
- The first item of a window charges the ledger unconditionally, so an
  oversize item is visible to every run sharing the ledger.
- The global ledger *instance* belongs to the application, not the library.
  CaveWhere creates one `ByteLedger` and passes it to each `limited()` call.
- The README gains a section on `Ledger`, which it currently omits entirely.

All work is in the `asyncfuture` submodule: `asyncfuture.h`, a new
`asyncfuture.cpp`, `CMakeLists.txt`, `README.md`, and
`tests/asyncfutureunittests/limitedtests.{h,cpp}`. `asyncfuture.pri` and
`asyncfuture.qbs` are deleted.

## 1. Ledger as a QObject with wakeup

### Interface

```c++
class Ledger : public QObject {
    Q_OBJECT
public:
    explicit Ledger(QObject* parent = nullptr);

    // Charges bytes and returns true when they fit; returns false and
    // charges nothing otherwise.
    virtual bool tryCharge(qint64 bytes) = 0;
    // Charges bytes whether or not they fit. limited() uses it for the first
    // item of a window, so an oversize item is visible to every run.
    virtual void charge(qint64 bytes) = 0;
    // Returns bytes from an earlier charge. Implementations emit released()
    // after the bytes are back.
    virtual void release(qint64 bytes) = 0;

signals:
    // Emitted after release() returns bytes. limited() runs connect to it
    // and retry admission on their own context thread.
    void released();
};
```

### The library becomes a compiled static library

`asyncfuture.h` has no `Q_OBJECT` macro today. `DeferredFuture` derives from
`QObject` but declares no signals, so nothing in the header needs moc. The
`released()` signal on `Ledger` is the first thing that does, and a moc'd
class cannot live in a header-only `INTERFACE` target: each consumer would
have to run moc on the header itself, and two consumers in one link would
produce duplicate `staticMetaObject` symbols.

Decision: `asyncfuture` becomes a `STATIC` library with one source file,
`asyncfuture.cpp`, holding `Ledger` and `ByteLedger`. Everything else stays
in the header, since it is templates.

`asyncfuture/CMakeLists.txt` changes:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(asyncfuture STATIC
    asyncfuture.h
    asyncfuture.cpp
)
set_target_properties(asyncfuture PROPERTIES AUTOMOC ON)
target_link_libraries(asyncfuture PUBLIC Qt6::Core)
target_include_directories(asyncfuture PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

install(TARGETS asyncfuture
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
```

Consumers are unchanged: `cavewherelib/CMakeLists.txt` already names
`asyncfuture` in `target_link_libraries`, and the tests link it the same
way. The `export(...)` lines stay as they are.

Delete `asyncfuture.pri` and `asyncfuture.qbs`. Nothing in the CaveWhere tree
reads them, and a compiled library with moc would need each of them rewritten
to list the source and enable moc. This fork no longer supports qmake or qbs
builds; note that in the README's build section.

Costs of the change, all accepted:

- The library now calls `find_package(Qt6)` itself instead of relying on the
  consumer's Qt. The version floor matches the tests' `6.8`.
- Every platform build (macOS, iOS, Android) compiles one extra translation
  unit. Negligible.
- Anyone vendoring the header alone no longer gets `Ledger`. The templates
  still work from the header, so that use stays possible for everything
  except the ledger.

### Shipped implementation

```c++
class ByteLedger : public Ledger {
public:
    explicit ByteLedger(qint64 cap, QObject* parent = nullptr);

    bool tryCharge(qint64 bytes) override;
    void charge(qint64 bytes) override;
    void release(qint64 bytes) override;

    qint64 cap() const;
    qint64 charged() const;

private:
    mutable QMutex m_mutex;
    qint64 m_cap;
    qint64 m_charged = 0;
};
```

Rules:

- `tryCharge` refuses when `m_charged + bytes > m_cap` and records nothing.
- `charge` always adds, so `m_charged` can exceed `m_cap`.
- `release` subtracts under the mutex, then emits `released()` outside the
  mutex.
- `cap()` and `charged()` are for tests and diagnostics; they read under the
  mutex.
- Zero and negative byte counts are accepted and are no-ops for `tryCharge`
  (return true) and `release`.

### Ledger lifetime

Document in the header and README: the ledger must outlive every `limited()`
run that uses it. `Limit::ledger` stays a raw, non-owning `Ledger*` with a
`nullptr` default, so the upgrade compiles unchanged for existing callers.

## 2. limited() changes

### Deduce T

```c++
template <typename Input, typename Launch, typename Weight>
auto limited(QObject* context, const QList<Input>& inputs,
             Launch launch, Weight weight, Limit limit)
    -> Limited<typename Private::FutureValue<std::invoke_result_t<Launch, const Input&>>::type>;
```

Add a `Private::FutureValue<QFuture<T>>` trait (or reuse one if the header
already has it for `Observable`). A launcher that returns something other
than a `QFuture` fails with a `static_assert` naming the requirement.

### Optional weight

Add an overload without `weight`:

```c++
template <typename Input, typename Launch>
auto limited(QObject* context, const QList<Input>& inputs,
             Launch launch, Limit limit);
```

It forwards to the full overload with a weight functor that returns `1`.
Document that `Limit::budget` and `Limit::ledger` then count items.

Guard against the two overloads colliding: the four-argument form takes
`Limit` as its last parameter, so a call with a weight lambda and no `Limit`
fails to compile rather than silently binding the lambda as `Limit`.

### First item charges unconditionally

In `admit()`:

```c++
const bool isFirst = running == 0;
if (!isFirst && !fits(itemWeight)) {
    break;
}
if (limit.ledger != nullptr) {
    if (isFirst) {
        limit.ledger->charge(itemWeight);
    } else if (!limit.ledger->tryCharge(itemWeight)) {
        break;
    }
    ledgerCharges.replace(index, itemWeight);
}
```

This replaces `chargesLedger()` and the `charged` flag. The release path is
unchanged: `releaseLedger(index)` releases the recorded amount once and
zeroes it.

### Wakeup on release

In `limited()`, after the destroyed connection:

```c++
if (limit.ledger != nullptr) {
    state->onLedgerReleased = QObject::connect(
        limit.ledger, &Ledger::released, context, [state]() { state->admit(); });
}
```

Disconnect `onLedgerReleased` in both places `onContextDestroyed` is already
disconnected: `markSettled()` when the last item settles, and
`cancelEverything()`. The receiver is `context`, so the slot runs on the
context thread and Qt drops the connection when the context dies.

`admit()` already returns early when nothing can be admitted, so a wakeup
that finds the window full is cheap. The re-entrancy guard `admitting` now
has a real job: a `release()` inside a settle callback can emit `released()`
into a run on the same thread while that run is in `admit()`.

### Weight evaluation

Compute each input's weight once, on first admission attempt, and cache it in
a `QList<qint64> weights` on `State` (initialized to a sentinel, or a
parallel `QList<bool>`). Also move the `running >= limit.maxRunning` check
ahead of the weight call so a count-limited run never evaluates `weight` for
an item it cannot admit.

### Throwing launcher

Wrap the body of `admit()` so `admitting` is reset on every exit. A small
RAII struct in the anonymous namespace is enough:

```c++
struct ResetFlag {
    bool& flag;
    ~ResetFlag() { flag = false; }
};
```

Move the counter increments after `launch(input)` returns so a throw leaves
`running` and `inFlight` unchanged.

### Small fixes

- `cancelEverything()` iterates `deferreds` by `const` reference.
- README says `budget == 0` is count-only; code uses `<= 0`. Make the README
  say "zero or negative".

## 3. README

Add to the `limited()` section:

- The `Ledger` interface, `ByteLedger`, and when to use a ledger instead of
  `Limit::budget` (several runs sharing one cap versus one run).
- The first-item rule for the ledger: the first item always runs and is
  charged in full, so other runs hold back while it runs.
- The wakeup: a release in any run retries admission in every run sharing
  the ledger, on each run's own context thread.
- Lifetime: the ledger must outlive every run using it; the application owns
  it.
- The count-only overload without `weight`.
- The deduced `T`: `limited(this, inputs, launch, limit)` with no template
  argument.

Update the existing example to drop the explicit `<Result>` and show a
`ByteLedger` in the `Limit`.

## 4. Tests

Update the existing tests for the signature changes, then add these to
`LimitedTests`. Each uses the existing `Launcher` helper. `FakeLedger` gains
a `charge()` override and, since `Ledger` now emits `released()`, the fake
calls `emit released()` from its `release()`.

1. `test_cancel_admitted_item_propagates_and_frees_slot`
   Cancel `items.at(0)` while it runs. Verify the launched future for input 0
   is canceled, input 1 is admitted, and `all` finishes.
2. `test_ledger_released_on_context_destruction`
   Ledger with two charged items running. Destroy the context, settle both
   launched futures, verify `ledger.charged == 0` and `releaseCount == 2`.
3. `test_two_runs_share_one_ledger`
   Two runs, two contexts, one `ByteLedger` with room for three units of
   weight. Run A admits two, run B admits one and waits. Settle one of A's,
   verify B admits its next item without any of B's items settling. This is
   the wakeup test.
4. `test_ledger_first_item_charged_past_cap`
   Ledger cap 5, first item weight 10. Verify it runs and `charged == 10`,
   and a second run sharing the ledger admits nothing until it settles.
5. `test_empty_inputs`
   `items` is empty and `all` is finished and not canceled.
6. `test_launcher_returns_finished_future`
   Launcher returns `Deferred` already completed. Verify every input is
   admitted in order and `all` finishes, with no stall.
7. `test_cancel_all_with_running_and_queued`
   Two running, three queued. Cancel `all`. Verify both launched futures are
   canceled, all five items are canceled, and the launch count stays at two
   after the running ones settle.
8. `test_weight_called_once_per_input`
   Count the weight functor's invocations. Equals the number of inputs
   admitted or attempted, never more than once per input.
9. `test_count_only_overload`
   `limited(context, inputs, launch, Limit{2})` with no weight. Verify the
   count limit holds.
10. `test_type_deduced_from_launcher`
    Compile-time check: `static_assert(std::is_same_v<decltype(run),
    Limited<int>>)` in an existing test.

Keep every test safe for concurrent processes: no shared global ledger across
tests, each test owns its `ByteLedger`.

## 5. Commit sequence

Six commits in the `asyncfuture` submodule, each building and passing the
full `asyncfutureunittests` run on its own, followed by one commit in the
CaveWhere parent to bump the submodule pointer. Order matters: the build
change goes first so every later commit is already a compiled library, and
the behavior changes each land with their own tests.

1. **Build asyncfuture as a static library and drop qmake and qbs**
   `CMakeLists.txt` switches to `STATIC` with `AUTOMOC`, `find_package(Qt6)`,
   and the archive install rule. Add an empty `asyncfuture.cpp` that only
   includes the header. Delete `asyncfuture.pri` and `asyncfuture.qbs`.
   README build section says CMake only. No behavior change; tests pass
   unchanged. Verify CaveWhere still links by building `cavewherelib`.

2. **Deduce limited()'s result type and add a count-only overload**
   The `FutureValue` trait, the deduced return type, the overload without
   `weight`, and the `static_assert` for a non-future launcher. Existing
   tests drop `<int>`. Adds tests 9 and 10 from section 4. README example
   drops the explicit template argument.

3. **Cache weights and keep a throwing launcher from wedging limited()**
   Per-input weight cache, the count check ahead of the weight call, the
   scope guard on `admitting`, counter increments moved after `launch`, and
   the `const` reference in `cancelEverything`. Adds tests 5, 6, and 8.

4. **Add tests for cancel propagation and cancel-all with running items**
   Tests 1 and 7. Pure test commit against behavior that already exists;
   split out so a failure here points at the original `limited()` rather
   than at the ledger work.

5. **Make Ledger a QObject with released() and ship ByteLedger**
   `Ledger` gains `charge()` and the `released()` signal and moves its
   non-template parts to `asyncfuture.cpp`. `ByteLedger` lands beside it.
   `limited()` charges the first item unconditionally and connects to
   `released()` for wakeup, disconnecting in `markSettled()` and
   `cancelEverything()`. `FakeLedger` in the tests gains `charge()` and
   emits `released()`. Adds tests 2, 3, and 4. README gains the Ledger
   subsection and the `budget <= 0` wording fix.

6. **Document lifetime and threading for Ledger**
   Header comments and the README paragraph on who owns the ledger, that it
   must outlive every run, and that `released()` fans out to every run's
   own context thread. Can fold into commit 5 if it stays under a few lines.

Then in the CaveWhere parent:

7. **Bump asyncfuture for compiled Ledger and deduced limited()**
   Submodule pointer only, unless a CaveWhere call site already uses
   `limited<T>` with an explicit argument; the review found none. Run
   `cavewhere-test` and `cavewhere-qml-test` before pushing, per CLAUDE.md.

Each commit message follows the submodule's existing style: imperative
subject, body with what and why, and the test totals from the run.

## 6. Build and verify

From `asyncfuture/`:

```bash
cmake --build build/Qt_6_8_3_for_macOS-Release --target asyncfutureunittests
./build/Qt_6_8_3_for_macOS-Release/tests/asyncfutureunittests 2>&1 | tee /tmp/asyncfuture-test.log
```

Then rebuild the CaveWhere tree, since `cavewherelib` includes the header,
and run `cavewhere-test` to confirm nothing else called `limited<T>` with an
explicit argument.

## Out of scope

- A library-level global ledger singleton. The application owns the instance.
- Any `Ledger` that probes real system memory. The interface allows it; this
  plan ships only the fixed-cap `ByteLedger`.
- Renaming `limited` / `Limited` / `Limit`.

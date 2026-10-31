#!/usr/bin/env python3
"""Shared flaky-test ledger for CaveWhere.

Every checkout on this machine, every Claude Code session, and the nightly
runner write to one SQLite database (~/.cavewhere/flaky.sqlite by default), so
a flake seen in one place is visible from all of them.

  flaky.py run [cpp|qml|all] [-- test args]   run a suite, rerun each failure
                                              alone, record what happened
  flaky.py record --suite S --test T ...      record a failure by hand
  flaky.py ingest --suite S RUN.log ...       record a batch of runs of one
                                              binary (the nightly runner)
  flaky.py report                             flaky tests, most frequent first
  flaky.py show TEST                          every occurrence of one test
  flaky.py promote TEST [--post]              file or update its GitHub issue

A failure is "flaky" when the same build also passed it: a rerun alone passed,
or another run in the same ingest batch passed. It is "broken" when every
rerun failed too, and "unverified" when no rerun could be made.
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

kHome = Path(os.environ.get("CAVEWHERE_FLAKY_HOME", Path.home() / ".cavewhere"))
kDbPath = Path(os.environ.get("CAVEWHERE_FLAKY_DB", kHome / "flaky.sqlite"))
kLogRoot = kHome / "flaky-logs"

kSuites = {"cpp": "cavewhere-test", "qml": "cavewhere-qml-test"}
kDefaultReruns = 5
kMaxRerunTests = 8
kSuiteTimeoutMin = 60
kHungTestSec = 5 * 60
kSlowestShown = 5
kRerunTimeoutSec = 5 * 60
kKillGraceSec = 30
kMessageLimit = 600
kBusyTimeoutMs = 30000
kPromoteMinimum = 3
kIssueLabel = "flaky-test"
kQmlPrefix = "CaveWhere-qml-tests::"
kTimeoutExitCodes = (124, 137)  # gtimeout: timed out, killed after the grace period

kSchema = """
CREATE TABLE IF NOT EXISTS runs(
    id INTEGER PRIMARY KEY,
    started_at TEXT, finished_at TEXT, source TEXT,
    checkout TEXT, branch TEXT, commit_sha TEXT, dirty INTEGER,
    build_dir TEXT, binary_mtime TEXT, asan INTEGER,
    suite TEXT, args TEXT, rng_seed TEXT,
    exit_code INTEGER, timed_out INTEGER, not_run INTEGER,
    load_avg REAL, concurrent_runs INTEGER,
    log_path TEXT);
CREATE TABLE IF NOT EXISTS failures(
    id INTEGER PRIMARY KEY,
    run_id INTEGER REFERENCES runs(id),
    suite TEXT, test TEXT, kind TEXT,
    message TEXT, location TEXT,
    reruns INTEGER, rerun_failures INTEGER, outcome TEXT,
    log_path TEXT, recorded_at TEXT);
CREATE INDEX IF NOT EXISTS failures_test ON failures(suite, test);
CREATE TABLE IF NOT EXISTS issues(
    suite TEXT, test TEXT, number INTEGER,
    PRIMARY KEY(suite, test));
"""


# ---------------------------------------------------------------- database

def connect() -> sqlite3.Connection:
    kDbPath.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(kDbPath, timeout=kBusyTimeoutMs / 1000)
    db.row_factory = sqlite3.Row
    db.execute(f"PRAGMA busy_timeout={kBusyTimeoutMs}")
    db.execute("PRAGMA journal_mode=WAL")
    db.executescript(kSchema)
    columns = {row["name"] for row in db.execute("PRAGMA table_info(runs)")}
    if "not_run" not in columns:
        db.execute("ALTER TABLE runs ADD COLUMN not_run INTEGER")
    return db


def now() -> str:
    return dt.datetime.now().astimezone().isoformat(timespec="seconds")


# ---------------------------------------------------------------- environment

def git(checkout: Path, *args: str) -> str:
    result = subprocess.run(["git", "-C", str(checkout), *args],
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def checkout_info(checkout: Path) -> dict:
    return {
        "checkout": str(checkout),
        "branch": git(checkout, "rev-parse", "--abbrev-ref", "HEAD"),
        "commit_sha": git(checkout, "rev-parse", "--short=10", "HEAD"),
        "dirty": int(bool(git(checkout, "status", "--porcelain", "--untracked-files=no"))),
    }


def find_checkout(start: Path) -> Path:
    top = git(start, "rev-parse", "--show-toplevel")
    return Path(top) if top else start


def is_asan(binary: Path) -> bool:
    result = subprocess.run(["otool", "-L", str(binary)], capture_output=True, text=True)
    return "asan" in result.stdout


def concurrent_runs() -> int:
    count = 0
    for name in kSuites.values():
        result = subprocess.run(["pgrep", "-x", name], capture_output=True, text=True)
        count += len(result.stdout.split())
    return count


def stale_submodules(checkout: Path) -> list:
    """Submodules whose checkout differs from the commit HEAD pins."""
    lines = git(checkout, "submodule", "status").splitlines()
    return [line[1:].split()[1] for line in lines if line.startswith("+")]


def catch2_case_count(binary: Path, args: list, cwd: Path, env: dict) -> int | None:
    result = subprocess.run([str(binary), "--list-tests", "-r", "xml", *args],
                            cwd=cwd, env=env, capture_output=True, text=True)
    count = result.stdout.count("<TestCase>")
    return count if count else None


def newest_build(checkout: Path) -> Path | None:
    candidates = []
    for directory in (checkout / "build").glob("*/"):
        binaries = [directory / name for name in kSuites.values()]
        existing = [b for b in binaries if b.exists()]
        if existing:
            candidates.append((max(b.stat().st_mtime for b in existing), directory))
    return max(candidates)[1] if candidates else None


# ---------------------------------------------------------------- result parsing

@dataclass
class TestResult:
    name: str
    passed: bool | None = None
    message: str = ""
    location: str = ""
    kind: str = "fail"
    duration: float | None = None


@dataclass
class SuiteResult:
    tests: dict = field(default_factory=dict)
    unfinished: str | None = None
    rng_seed: str = ""
    caseCount: int = 0  # Catch2 allows two test cases with one name, so this can exceed len(tests)

    def failures(self) -> list:
        return [t for t in self.tests.values() if t.passed is False]

    def passed_count(self) -> int:
        if self.caseCount:
            return self.caseCount - len(self.failures())
        return sum(1 for t in self.tests.values() if t.passed)


def clip(text: str) -> str:
    text = re.sub(r"\s+", " ", text or "").strip()
    return text if len(text) <= kMessageLimit else text[:kMessageLimit] + "…"


def pull_events(path: Path):
    """Yields (event, element) pairs, tolerating a file cut short by a crash."""
    if not path.exists():
        return
    text = path.read_text(errors="replace")
    text = re.sub(r"<\?xml[^>]*\?>", "", text)
    parser = ET.XMLPullParser(events=("start", "end"))
    try:
        parser.feed("<root>" + text)
    except ET.ParseError:
        pass
    try:
        yield from parser.read_events()
    except ET.ParseError:
        return


def parse_catch2(path: Path) -> SuiteResult:
    result = SuiteResult()
    current = None
    sections = []
    for event, element in pull_events(path):
        tag = element.tag
        if event == "start":
            if tag == "Catch2TestRun":
                result.rng_seed = element.get("rng-seed", "")
            elif tag == "TestCase":
                result.caseCount += 1
                current = TestResult(element.get("name", ""))
                result.tests[current.name] = current
                result.unfinished = current.name
                sections = []
            elif tag == "Section":
                sections.append(element.get("name", ""))
            continue

        if current is None:
            continue
        if tag == "Section" and sections:
            sections.pop()
        elif tag in ("Expression", "FatalErrorCondition", "Exception", "Failure"):
            failed = tag != "Expression" or element.get("success") == "false"
            if failed and not current.message:
                if tag == "Expression":
                    original = element.findtext("Original", "")
                    expanded = element.findtext("Expanded", "")
                    detail = f"{element.get('type', '')}( {clip(original)} ) with expansion {clip(expanded)}"
                else:
                    detail = f"{tag}: {clip(element.text or '')}"
                if tag == "FatalErrorCondition":
                    current.kind = "crash"
                path_text = " / ".join(sections)
                current.message = clip(f"[{path_text}] {detail}" if path_text else detail)
                current.location = f"{element.get('filename', '')}:{element.get('line', '')}"
        elif tag == "OverallResult":
            current.passed = element.get("success") == "true"
            if element.get("durationInSeconds"):
                current.duration = float(element.get("durationInSeconds"))
        elif tag == "TestCase":
            result.unfinished = None
            current = None
    return result


def qml_test_name(raw: str) -> str:
    name = raw[len(kQmlPrefix):] if raw.startswith(kQmlPrefix) else raw
    return re.sub(r"\(\)$", "", name)


def parse_qtest(path: Path) -> SuiteResult:
    result = SuiteResult()
    current = None
    for event, element in pull_events(path):
        tag = element.tag
        if event == "start":
            if tag == "TestFunction":
                current = TestResult(qml_test_name(element.get("name", "")))
                result.tests.setdefault(current.name, current)
                current = result.tests[current.name]
                result.unfinished = current.name
            continue

        if current is None:
            continue
        if tag == "Incident":
            kind = element.get("type", "")
            if kind in ("fail", "xpass"):
                current.passed = False
                if not current.message:
                    tagText = element.findtext("DataTag", "")
                    description = element.findtext("Description", "")
                    current.message = clip(f"[{tagText}] {description}" if tagText else description)
                    current.location = f"{element.get('file', '')}:{element.get('line', '')}"
            elif kind in ("pass", "skip", "xfail") and current.passed is None:
                current.passed = True
        elif tag == "Message" and element.get("type") == "qfatal":
            current.passed = False
            current.kind = "crash"
            current.message = clip(element.findtext("Description", ""))
        elif tag == "TestFunction":
            result.unfinished = None
            current = None
    return result


def parse_qtest_text(log: Path) -> SuiteResult:
    """Fallback for a QML run that died before writing its XML."""
    result = SuiteResult()
    if not log.exists():
        return result
    for line in log.read_text(errors="replace").splitlines():
        match = re.match(r"(PASS|FAIL!)\s*:\s*(\S+?)(\(\))?\s(.*)$", line + " ")
        if match:
            name = qml_test_name(match.group(2))
            test = result.tests.setdefault(name, TestResult(name))
            if match.group(1) == "FAIL!":
                test.passed = False
                test.message = test.message or clip(match.group(4))
            elif test.passed is None:
                test.passed = True
    return result


# ---------------------------------------------------------------- running

@dataclass
class Invocation:
    exit_code: int
    timed_out: bool
    log: Path
    xml: Path


def suite_command(suite: str, binary: Path, xml: Path, args: list) -> list:
    if suite == "cpp":
        return [str(binary), "-d", "yes", "-r", "console", "-r", f"xml::out={xml}", *args]
    return [str(binary), "--platform", "offscreen", "-o", f"{xml},xml", "-o", "-,txt", *args]


def invoke(command: list, cwd: Path, log: Path, xml: Path, timeout: int, env: dict) -> Invocation:
    with open(log, "w") as out:
        process = subprocess.Popen(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True)
        try:
            return Invocation(process.wait(timeout=timeout), False, log, xml)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=kKillGraceSec)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            out.write(f"\nTIMEOUT: killed by flaky.py after {timeout}s\n")
            return Invocation(process.returncode, True, log, xml)


def parse(suite: str, invocation: Invocation) -> SuiteResult:
    if suite == "cpp":
        return parse_catch2(invocation.xml)
    result = parse_qtest(invocation.xml)
    return result if result.tests else parse_qtest_text(invocation.log)


def attribute_unexplained(suite: str, result: SuiteResult, invocation: Invocation) -> None:
    """A crash or hang that no test reported is pinned on the test left running."""
    if invocation.exit_code == 0 and not invocation.timed_out:
        return
    kind = "timeout" if invocation.timed_out else "crash"
    if result.unfinished:
        test = result.tests[result.unfinished]
        test.passed = False
        test.kind = kind
        test.message = test.message or f"{kind} while this test was running (exit {invocation.exit_code})"
    elif not result.failures():
        tail = invocation.log.read_text(errors="replace")[-kMessageLimit:] if invocation.log.exists() else ""
        name = f"<{kSuites[suite]} {kind} outside any test>"
        result.tests[name] = TestResult(name, False, clip(tail), "", kind)


def catch2_spec(name: str) -> str:
    return re.sub(r'([\\\[\],*~"])', r"\\\1", name)


def qml_files_by_case(checkout: Path) -> dict:
    files = {}
    for path in sorted((checkout / "test-qml").glob("*.qml")):
        for match in re.finditer(r'^\s*name:\s*"([^"]+)"', path.read_text(errors="replace"), re.M):
            files.setdefault(match.group(1), path)
    return files


def rerun_args(suite: str, test: str, qmlFiles: dict) -> list | None:
    if not test or test.startswith("<"):
        return None
    if suite == "cpp":
        return [catch2_spec(test)]
    case, _, function = test.partition("::")
    path = qmlFiles.get(case)
    if path is None:
        return None
    if function in ("", "initTestCase", "cleanupTestCase"):
        return ["-input", str(path)]
    return ["-input", str(path), f"{case}::{function}"]


def suite_env(binary: Path) -> dict:
    env = dict(os.environ)
    if is_asan(binary):
        env.setdefault("ASAN_OPTIONS", "detect_container_overflow=0")
    return env


# ---------------------------------------------------------------- recording

def insert_run(db, **values) -> int:
    columns = ", ".join(values)
    marks = ", ".join("?" for _ in values)
    return db.execute(f"INSERT INTO runs({columns}) VALUES({marks})", list(values.values())).lastrowid


def insert_failure(db, **values) -> int:
    values.setdefault("recorded_at", now())
    columns = ", ".join(values)
    marks = ", ".join("?" for _ in values)
    return db.execute(f"INSERT INTO failures({columns}) VALUES({marks})", list(values.values())).lastrowid


def keep_logs(runId: int, files: list) -> Path:
    destination = kLogRoot / f"run-{runId}"
    destination.mkdir(parents=True, exist_ok=True)
    for path in files:
        if path.exists():
            shutil.copy2(path, destination / path.name)
    return destination


def history(db, suite: str, test: str, beforeRunId: int) -> dict:
    row = db.execute(
        "SELECT COUNT(*) AS count, MIN(f.recorded_at) AS first, COUNT(DISTINCT r.checkout) AS checkouts "
        "FROM failures f JOIN runs r ON r.id = f.run_id "
        "WHERE f.suite = ? AND f.test = ? AND f.outcome = 'flaky' AND f.run_id < ?",
        (suite, test, beforeRunId)).fetchone()
    return dict(row)


def classify(reruns: int, rerunFailures: int) -> str:
    if reruns == 0:
        return "unverified"
    return "broken" if rerunFailures == reruns else "flaky"


# ---------------------------------------------------------------- commands

def run_suite(db, suite: str, buildDir: Path, checkout: Path, args: list, reruns: int, timeoutSec: int,
              workDir: Path) -> bool:
    binary = buildDir / kSuites[suite]
    if not binary.exists():
        print(f"{binary} does not exist; build it first.", file=sys.stderr)
        return False

    env = suite_env(binary)
    log = workDir / f"{suite}-suite.log"
    xml = workDir / f"{suite}-suite.xml"
    started = now()
    load = os.getloadavg()[0]
    alongside = concurrent_runs()
    print(f">>> {kSuites[suite]} {' '.join(args)}  (log: {log})", flush=True)

    invocation = invoke(suite_command(suite, binary, xml, args), checkout, log, xml, timeoutSec, env)
    result = parse(suite, invocation)
    attribute_unexplained(suite, result, invocation)
    failures = result.failures()
    stoppedEarly = invocation.timed_out or invocation.exit_code < 0
    notRun = None
    if suite == "cpp":
        total = catch2_case_count(binary, args, checkout, env)
        notRun = max(total - result.caseCount, 0) if total else None
        stoppedEarly = stoppedEarly or bool(notRun)

    # The timeout's kill lands in whichever test was running; only a long-running one is a hang.
    interrupted = None
    if invocation.timed_out:
        killed = [t for t in failures if t.kind != "fail"]
        if killed:
            interrupted = killed[-1]
            interrupted.kind = "timeout"
            if interrupted.duration is not None and interrupted.duration >= kHungTestSec:
                interrupted = None

    runId = insert_run(db, started_at=started, finished_at=now(), source="run",
                       **checkout_info(checkout), build_dir=str(buildDir),
                       binary_mtime=dt.datetime.fromtimestamp(binary.stat().st_mtime).isoformat(timespec="seconds"),
                       asan=int(is_asan(binary)), suite=suite, args=" ".join(args),
                       rng_seed=result.rng_seed, exit_code=invocation.exit_code,
                       timed_out=int(invocation.timed_out), not_run=notRun, load_avg=load,
                       concurrent_runs=alongside, log_path=str(log))
    db.commit()

    print(f"    {result.passed_count()} passed, {len(failures)} failed"
          + (f", {notRun} never ran" if notRun else "")
          + f", exit {invocation.exit_code}"
          + (", TIMED OUT" if invocation.timed_out else ""))
    if stoppedEarly:
        if invocation.timed_out:
            cause = "timed out"
        elif invocation.exit_code < 0:
            cause = f"was killed by signal {-invocation.exit_code}"
        else:
            cause = f"exited {invocation.exit_code}"
        print(f"    INCOMPLETE: the suite {cause} before it finished; "
              + (f"{notRun} test cases never ran." if notRun else "the tests after that point never ran."))
    if invocation.timed_out:
        timed = sorted((t for t in result.tests.values() if t.duration is not None),
                       key=lambda t: t.duration, reverse=True)[:kSlowestShown]
        if timed:
            print("    slowest tests in this run:")
            for test in timed:
                print(f"      {test.duration:8.1f}s  {test.name}")
    if not failures:
        return not stoppedEarly

    logDir = keep_logs(runId, [log, xml])
    db.execute("UPDATE runs SET log_path = ? WHERE id = ?", (str(logDir / log.name), runId))
    qmlFiles = qml_files_by_case(checkout) if suite == "qml" else {}
    allClear = True

    for index, failure in enumerate(failures):
        if failure is interrupted:
            insert_failure(db, run_id=runId, suite=suite, test=failure.name, kind=failure.kind,
                           message=failure.message, location=failure.location, reruns=0,
                           rerun_failures=0, outcome="interrupted", log_path=str(logDir))
            db.commit()
            ran = "" if failure.duration is None else f" after {failure.duration:.1f}s"
            print(f"  INTERRUPTED {failure.name}\n             was running{ran} when the suite timeout hit; "
                  "not counted as a failure")
            continue
        rerunArgs = rerun_args(suite, failure.name, qmlFiles) if index < kMaxRerunTests else None
        rerunFailures = 0
        rerunCount = 0
        failedLogs = []
        for attempt in range(reruns if rerunArgs is not None else 0):
            stem = f"{suite}-rerun-{index}-{attempt}"
            rerun = invoke(suite_command(suite, binary, workDir / f"{stem}.xml", rerunArgs), checkout,
                           workDir / f"{stem}.log", workDir / f"{stem}.xml", kRerunTimeoutSec, env)
            rerunResult = parse(suite, rerun)
            attribute_unexplained(suite, rerunResult, rerun)
            matched = rerunResult.tests.get(failure.name)
            if matched is None or matched.passed is None:
                continue
            rerunCount += 1
            if not matched.passed:
                rerunFailures += 1
                failedLogs += [rerun.log, rerun.xml]

        outcome = classify(rerunCount, rerunFailures)
        keep_logs(runId, failedLogs)
        seen = history(db, suite, failure.name, runId)
        insert_failure(db, run_id=runId, suite=suite, test=failure.name, kind=failure.kind,
                       message=failure.message, location=failure.location, reruns=rerunCount,
                       rerun_failures=rerunFailures, outcome=outcome, log_path=str(logDir))
        db.commit()
        allClear = allClear and outcome == "flaky"
        print_failure(failure, outcome, rerunCount, rerunFailures, seen, checkout_info(checkout)["dirty"])

    print(f"    recorded as run #{runId}; logs kept in {logDir}")
    return allClear and not stoppedEarly


def print_failure(failure: TestResult, outcome: str, reruns: int, rerunFailures: int, seen: dict, dirty: int) -> None:
    print(f"  {outcome.upper():10} {failure.name}  [{failure.kind}]")
    if failure.location.strip(":"):
        print(f"             at {failure.location}")
    if failure.message:
        print(f"             {failure.message}")
    if outcome == "unverified":
        print("             could not rerun it alone; treat it as a real failure")
        return
    alone = "passes alone, fails only inside the suite" if rerunFailures == 0 else "fails sometimes even alone"
    print(f"             reruns alone: {rerunFailures}/{reruns} failed ({alone if outcome == 'flaky' else 'fails every time'})")
    if outcome != "flaky":
        return
    if seen["count"]:
        print(f"             known flake: seen {seen['count']}x before, first {seen['first']}, "
              f"in {seen['checkouts']} checkout(s)")
    elif dirty:
        print("             FIRST time this test has flaked, on a tree with uncommitted changes: "
              "check whether your change introduced it")
    else:
        print("             first time this test has flaked")


def command_run(options) -> int:
    checkout = find_checkout(Path.cwd())
    buildDir = Path(options.build).resolve() if options.build else newest_build(checkout)
    if buildDir is None:
        print("No build directory with test binaries found under build/; pass --build.", file=sys.stderr)
        return 2
    suites = ["cpp", "qml"] if options.suite == "all" else [options.suite]
    if options.suite == "all" and options.args:
        print("Test arguments are suite specific; name one suite (cpp or qml) to pass them.", file=sys.stderr)
        return 2

    print(f"Build: {buildDir}\nLedger: {kDbPath}")
    for name in stale_submodules(checkout):
        print(f"WARNING: submodule {name} is checked out at a different commit than HEAD pins; "
              f"`git submodule update {name}` and rebuild, or its failures may be from the mismatch.")
    workDir = Path(tempfile.mkdtemp(prefix=f"cavewhere-flaky-{os.getpid()}-"))
    db = connect()
    allClear = True
    for suite in suites:
        allClear = run_suite(db, suite, buildDir, checkout, options.args, options.reruns,
                             options.timeout * 60, workDir) and allClear
    print("\nResult: " + ("every test ran; failures, if any, were flaky" if allClear
                          else "real failures present (BROKEN, UNVERIFIED, or an incomplete suite)"))
    return 0 if allClear else 1


def command_record(options) -> int:
    checkout = find_checkout(Path.cwd())
    db = connect()
    runId = insert_run(db, started_at=now(), finished_at=now(), source="manual",
                       **checkout_info(checkout), build_dir=options.build or "",
                       suite=options.suite, args="", rng_seed=options.seed or "")
    logDir = keep_logs(runId, [Path(p) for p in options.log]) if options.log else None
    insert_failure(db, run_id=runId, suite=options.suite, test=options.test, kind=options.kind,
                   message=clip(options.message or ""), location=options.location or "",
                   reruns=options.reruns, rerun_failures=options.rerun_failures,
                   outcome=classify(options.reruns, options.rerun_failures) if options.reruns else options.outcome,
                   log_path=str(logDir or ""))
    db.commit()
    print(f"Recorded {options.suite} '{options.test}' as run #{runId}.")
    return 0


def command_ingest(options) -> int:
    """Records a batch of runs of one binary; a failure is flaky if another run passed the test."""
    checkout = Path(options.checkout).resolve() if options.checkout else find_checkout(Path.cwd())
    info = checkout_info(checkout)
    asan = int(is_asan(Path(options.build) / kSuites[options.suite])) if options.build else 0
    db = connect()

    parsed = []
    for log in map(Path, options.logs):
        xmlPath = log.with_suffix(".xml")
        exitFile = log.with_suffix(".exit")
        exitCode = int(exitFile.read_text().strip() or 1) if exitFile.exists() else None
        invocation = Invocation(exitCode or 0, exitCode in kTimeoutExitCodes, log, xmlPath)
        result = parse(options.suite, invocation)
        attribute_unexplained(options.suite, result, invocation)
        parsed.append((xmlPath, log, exitCode, result))

    passedSomewhere = {name for _, _, _, result in parsed for name, test in result.tests.items() if test.passed}
    flakyCount = 0
    for xmlPath, log, exitCode, result in parsed:
        runId = insert_run(db, started_at=now(), finished_at=now(), source=options.source, **info,
                           build_dir=options.build or "", asan=asan, suite=options.suite,
                           rng_seed=result.rng_seed, exit_code=exitCode, log_path=str(log))
        failures = result.failures()
        logDir = keep_logs(runId, [log, xmlPath]) if failures else None
        for failure in failures:
            outcome = "flaky" if failure.name in passedSomewhere else "broken"
            flakyCount += outcome == "flaky"
            insert_failure(db, run_id=runId, suite=options.suite, test=failure.name, kind=failure.kind,
                           message=failure.message, location=failure.location, reruns=len(parsed) - 1,
                           rerun_failures=None, outcome=outcome, log_path=str(logDir))
    db.commit()
    print(f"Ingested {len(parsed)} {options.suite} runs; {flakyCount} flaky failure(s) recorded.")
    return 0


def command_report(options) -> int:
    db = connect()
    outcomes = ("flaky", "broken", "unverified", "interrupted") if options.all else ("flaky",)
    since = (dt.datetime.now().astimezone() - dt.timedelta(days=options.days)).isoformat(timespec="seconds")
    rows = db.execute(
        f"SELECT f.suite, f.test, COUNT(*) AS count, MAX(f.recorded_at) AS last, MIN(f.recorded_at) AS first, "
        f"COUNT(DISTINCT r.checkout) AS checkouts, SUM(r.asan) AS asan, "
        f"SUM(f.kind != 'fail') AS crashes, SUM(f.rerun_failures = 0) AS suiteOnly, "
        f"GROUP_CONCAT(DISTINCT f.outcome) AS outcomes, i.number AS issue "
        f"FROM failures f JOIN runs r ON r.id = f.run_id "
        f"LEFT JOIN issues i ON i.suite = f.suite AND i.test = f.test "
        f"WHERE f.outcome IN ({','.join('?' for _ in outcomes)}) AND f.recorded_at >= ? "
        f"GROUP BY f.suite, f.test ORDER BY count DESC, last DESC",
        (*outcomes, since)).fetchall()
    if not rows:
        print(f"No {'/'.join(outcomes)} failures in the last {options.days} days.")
        return 0
    print(f"{'count':>5}  {'suite':5}  {'last seen':19}  test")
    for row in rows:
        notes = []
        if row["suiteOnly"]:
            notes.append(f"{row['suiteOnly']} suite-only")
        if row["crashes"]:
            notes.append(f"{row['crashes']} crash/timeout")
        if row["asan"]:
            notes.append(f"{row['asan']} asan")
        if options.all:
            notes.append(row["outcomes"])
        if row["issue"]:
            notes.append(f"#{row['issue']}")
        print(f"{row['count']:>5}  {row['suite']:5}  {row['last'][:19]:19}  {row['test']}")
        if notes:
            print(f"{'':35}  {', '.join(notes)}")
    return 0


def occurrences(db, test: str) -> list:
    return db.execute(
        "SELECT f.*, r.checkout, r.branch, r.commit_sha, r.dirty, r.asan, r.load_avg, "
        "r.concurrent_runs, r.rng_seed, r.source, r.args "
        "FROM failures f JOIN runs r ON r.id = f.run_id "
        "WHERE f.test = ? OR f.test LIKE ? ORDER BY f.recorded_at",
        (test, f"%{test}%")).fetchall()


def command_show(options) -> int:
    db = connect()
    rows = occurrences(db, options.test)
    if not rows:
        print(f"No failures recorded for '{options.test}'.")
        return 1
    for row in rows:
        context = [f"{row['kind']}"
                   + ("" if row["rerun_failures"] is None else f" alone {row['rerun_failures']}/{row['reruns']}"),
                   f"{row['source']} on {row['branch']}@{row['commit_sha']}"
                   + (" (dirty)" if row["dirty"] else "") + f" in {row['checkout']}"]
        if row["asan"]:
            context.append("ASAN")
        if row["load_avg"] is not None:
            context.append(f"load {row['load_avg']:.1f}")
        if row["concurrent_runs"]:
            context.append(f"{row['concurrent_runs']} other suite processes")
        if row["rng_seed"]:
            context.append(f"seed {row['rng_seed']}")
        print(f"{row['recorded_at'][:19]}  {row['outcome']:10} {row['suite']} {row['test']}")
        print("    " + "; ".join(context))
        if (row["location"] or "").strip(":"):
            print(f"    at {row['location']}")
        if row["message"]:
            print(f"    {row['message']}")
        if row["log_path"]:
            print(f"    logs: {row['log_path']}")
    return 0


def issue_body(rows: list) -> str:
    first = rows[0]
    lines = [
        f"`{first['test']}` in `{kSuites[first['suite']]}` has failed {len(rows)} time(s) on builds that "
        f"also passed it, between {rows[0]['recorded_at'][:10]} and {rows[-1]['recorded_at'][:10]}.",
        "",
        "| When | Commit | Kind | Reruns alone | Build | Message |",
        "|---|---|---|---|---|---|",
    ]
    for row in rows:
        alone = "n/a" if row["rerun_failures"] is None else f"{row['rerun_failures']}/{row['reruns']} failed"
        build = "ASAN" if row["asan"] else "release"
        message = (row["message"] or "").replace("|", "\\|")
        lines.append(f"| {row['recorded_at'][:16]} | {row['commit_sha']} | {row['kind']} | {alone} | {build} | {message} |")
    lines += ["", f"Recorded by `scripts/flaky/flaky.py`; `flaky.py show '{first['test']}'` lists the local logs."]
    return "\n".join(lines)


def command_promote(options) -> int:
    db = connect()
    rows = [r for r in occurrences(db, options.test) if r["outcome"] == "flaky"]
    tests = {(r["suite"], r["test"]) for r in rows}
    if len(tests) != 1:
        names = "\n  ".join(f"{s} {t}" for s, t in sorted(tests)) or "(none)"
        print(f"'{options.test}' must match exactly one flaky test; it matches:\n  {names}", file=sys.stderr)
        return 1
    suite, test = tests.pop()
    if len(rows) < options.min and not options.force:
        print(f"Only {len(rows)} flaky occurrence(s); promote needs {options.min} (or --force).")
        return 1

    issue = db.execute("SELECT number FROM issues WHERE suite = ? AND test = ?", (suite, test)).fetchone()
    body = issue_body(rows)
    title = f"Flaky test: {test}"
    if not options.post:
        print(f"Would {'comment on #' + str(issue['number']) if issue else 'create'}: {title}\n\n{body}\n\n"
              "Pass --post to send it to GitHub.")
        return 0

    if issue:
        subprocess.run(["gh", "issue", "comment", str(issue["number"]), "--body", body], check=True)
        print(f"Commented on #{issue['number']}.")
        return 0
    subprocess.run(["gh", "label", "create", kIssueLabel, "--color", "fbca04",
                    "--description", "Test that fails intermittently"], capture_output=True)
    created = subprocess.run(["gh", "issue", "create", "--title", title, "--body", body, "--label", kIssueLabel],
                             check=True, capture_output=True, text=True)
    number = int(created.stdout.strip().rstrip("/").split("/")[-1])
    db.execute("INSERT OR REPLACE INTO issues(suite, test, number) VALUES(?, ?, ?)", (suite, test, number))
    db.commit()
    print(f"Created {created.stdout.strip()}")
    return 0


def command_link(options) -> int:
    db = connect()
    db.execute("INSERT OR REPLACE INTO issues(suite, test, number) VALUES(?, ?, ?)",
               (options.suite, options.test, options.issue))
    db.commit()
    print(f"Linked {options.suite} '{options.test}' to #{options.issue}.")
    return 0


# ---------------------------------------------------------------- main

def main() -> int:
    argv = sys.argv[1:]
    testArgs = []
    if "--" in argv:
        split = argv.index("--")
        argv, testArgs = argv[:split], argv[split + 1:]

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    run = commands.add_parser("run", help="run a suite and rerun each failure alone")
    run.add_argument("suite", nargs="?", default="all", choices=["cpp", "qml", "all"])
    run.add_argument("--build", help="build directory (default: newest under build/)")
    run.add_argument("--reruns", type=int, default=kDefaultReruns, help="isolated reruns per failure")
    run.add_argument("--timeout", type=int, default=kSuiteTimeoutMin, help="minutes before a suite run is killed")
    run.set_defaults(handler=command_run)

    record = commands.add_parser("record", help="record a failure found by hand")
    record.add_argument("--suite", required=True, choices=list(kSuites))
    record.add_argument("--test", required=True)
    record.add_argument("--message")
    record.add_argument("--location")
    record.add_argument("--kind", default="fail", choices=["fail", "crash", "timeout"])
    record.add_argument("--reruns", type=int, default=0, help="how many times you reran it alone")
    record.add_argument("--rerun-failures", type=int, default=0, help="how many of those reruns failed")
    record.add_argument("--outcome", default="flaky", choices=["flaky", "broken", "unverified", "interrupted"],
                        help="used only when --reruns is 0")
    record.add_argument("--seed", help="Catch2 rng seed of the failing run")
    record.add_argument("--build")
    record.add_argument("--log", action="append", default=[], help="log file to keep (repeatable)")
    record.set_defaults(handler=command_record)

    ingest = commands.add_parser("ingest", help="record a batch of runs of one binary")
    ingest.add_argument("--suite", required=True, choices=list(kSuites))
    ingest.add_argument("--source", default="nightly")
    ingest.add_argument("--build")
    ingest.add_argument("--checkout")
    ingest.add_argument("logs", nargs="+", help="run logs; NAME.xml and NAME.exit (exit code) sit beside each")
    ingest.set_defaults(handler=command_ingest)

    report = commands.add_parser("report", help="flaky tests, most frequent first")
    report.add_argument("--days", type=int, default=90)
    report.add_argument("--all", action="store_true", help="include broken and unverified failures")
    report.set_defaults(handler=command_report)

    show = commands.add_parser("show", help="every recorded occurrence of a test (substring match)")
    show.add_argument("test")
    show.set_defaults(handler=command_show)

    promote = commands.add_parser("promote", help="create or update the GitHub issue for a flaky test")
    promote.add_argument("test")
    promote.add_argument("--min", type=int, default=kPromoteMinimum)
    promote.add_argument("--force", action="store_true")
    promote.add_argument("--post", action="store_true", help="actually post (default prints the issue)")
    promote.set_defaults(handler=command_promote)

    link = commands.add_parser("link", help="attach an existing GitHub issue to a test")
    link.add_argument("--suite", required=True, choices=list(kSuites))
    link.add_argument("--test", required=True)
    link.add_argument("issue", type=int)
    link.set_defaults(handler=command_link)

    options = parser.parse_args(argv)
    options.args = testArgs
    return options.handler(options)


if __name__ == "__main__":
    sys.exit(main())

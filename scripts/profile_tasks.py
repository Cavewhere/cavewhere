#!/usr/bin/env python3
"""Records CaveWhere opening a project under Instruments and reports where the
task pool's time and memory go, per task context.

    scripts/profile_tasks.py \
        --build build/Qt_6_11_2_for_macOS-Release \
        --project ~/caves/marble.cwproj --project ~/caves/foo.cw \
        --cold --out build/profile/tasks/baseline

    scripts/profile_tasks.py --compare build/profile/tasks/baseline \
        build/profile/tasks/after-F1

Each project gets run A (Time Profiler, os_signpost and Thread State Trace) and
run B (Allocations and os_signpost), --runs times each. The tables are exported
with `xctrace export`, read with iterparse, and joined on thread and time: a
CPU sample or thread-state span belongs to the context whose task interval
covers it on that thread. The report goes to <out>/report.md and the raw
numbers to <out>/metrics.json, which --compare reads back.

Python 3 standard library only.
"""

import argparse
import bisect
import collections
import contextlib
import dataclasses
import json
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

SUBSYSTEM = "com.cavewhere.tasks"
TASK_KIND = "task"
BATCH_KIND = "batch"
UNTAGGED_CONTEXT = "untagged"
DEFAULT_RUNS = 3
DEFAULT_PAGE = "View"
DEFAULTS_DOMAIN = "com.cavewhere.CaveWhere"
SETTING_KEYS = ["threadCount", "memoryBudgetMb", "automaticUpdate"]
APP_NAME = "CaveWhere"
RECORDED_PROCESS_TYPES = ("launched", "attached")
# Key under which export_rows() keeps a row's own attributes, beside its cells
ROW_ATTRIBUTES = "@attributes"
# Darwin notification xctrace posts once it records; the app is held stopped until then
TRACING_STARTED_NOTIFICATION = "com.cavewhere.profile_tasks.started"
METRICS_FILE = "metrics.json"
REPORT_FILE = "report.md"
BUNDLE_SUFFIX = ".cw"
UNKNOWN = "unknown"
# Signpost field with the bytes a limiter charged a task, to set against footprint growth
CHARGED_BYTES_ARG = "charged_bytes"

NS_PER_US = 1_000
NS_PER_S = 1_000_000_000
BYTES_PER_MB = 1024 * 1024
# A sample this close to the largest footprint counts as the peak
PEAK_PLATEAU_FRACTION = 0.95

# Thread states in which a thread wants a core
WANTS_CORE_STATES = ("Running", "Runnable", "Preempted")
WAITING_STATES = ("Runnable", "Preempted")

THREAD_SUFFIX = re.compile(r"\s*\(0x[0-9a-f]+\).*$")
PROCESS_SUFFIX = re.compile(r"\s*\(\d+\)$")
# Qt names every QThreadPool worker "Thread (pooled)"; the task pool and the global pool share it
POOL_THREAD = "Thread (pooled)"
# A thread that carries the process name is a std::thread with no name:
# basisu's job_pool, which libktx creates per encode
MAIN_THREAD = "Main Thread"
RENDER_THREAD = "QSGRenderThread"
ENCODE_LANE_THREAD = "cw::ktx2::encodeLane"
THREAD_GROUPS = [("main", "Main thread"), ("render", "Render thread"),
                 ("encode_lane", "Encode lane"), ("pool", "Qt pool threads"),
                 ("basisu", "basisu job_pool (process-named)"), ("other", "Other named threads")]
THREAD_GROUP_METRICS = [("created", "Created"), ("running_s", "Running s"),
                        ("waiting_s", "Runnable + Preempted s"), ("at_peak", "At the runnable peak")]

RUN_KINDS = {"A": ("A",), "B": ("B",), "AB": ("A", "B")}
DEFAULT_RUN_KINDS = "AB"

TOP_STACKS = 3
ALLOCATION_ROWS = 15

TABLE_XPATH = '/trace-toc/run[1]/data/table[@schema="{}"]'
ALLOCATIONS_XPATH = '/trace-toc/run[1]/tracks/track[@name="Allocations"]/details/detail[@name="{}"]'

RUN_A_TABLES = ["os-signpost-interval", "os-signpost", "thread-state", "time-profile"]
RUN_B_TABLES = ["os-signpost-interval", "os-signpost"]
RUN_B_DETAILS = ["Statistics"]
RUN_B_PEAK_DETAILS = ["Allocations List"]
TIME_PROFILER_INSTRUMENTS = ["--template", "Time Profiler", "--instrument", "os_signpost",
                             "--instrument", "Thread State Trace"]
ALLOCATIONS_INSTRUMENTS = ["--template", "Allocations", "--instrument", "os_signpost"]


@dataclasses.dataclass
class Interval:
    kind: str
    start_ns: int
    end_ns: int
    tid: int
    args: dict

    @property
    def context(self):
        return self.args.get("ctx", UNTAGGED_CONTEXT)


@dataclasses.dataclass
class Event:
    time_ns: int
    tid: int
    name: str
    args: dict


@dataclasses.dataclass
class ThreadState:
    start_ns: int
    end_ns: int
    tid: int
    state: str
    name: str = ""
    process: str = ""


@dataclasses.dataclass
class Sample:
    time_ns: int
    tid: int
    weight_ns: int
    frames: list


@dataclasses.dataclass
class Delta:
    name: str
    baseline: float
    after: float
    delta: float


# --- Reading the xctrace export -------------------------------------------------


def parse_arguments(message):
    """The key=value pairs of a signpost message."""
    arguments = {}
    tokens = message.split()
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        # xctrace writes "ctx= scraps" when the value is a separate argument
        if not value and index < len(tokens) and "=" not in tokens[index]:
            value = tokens[index]
            index += 1
        arguments[key] = value
    return arguments


class Node:
    """A slim copy of an exported element, kept for later refs while the
    parsed tree is freed row by row."""

    __slots__ = ("tag", "attrib", "text", "children")

    def __init__(self, tag, attrib, text_value, children):
        self.tag = tag
        self.attrib = attrib
        self.text = text_value
        self.children = children

    def get(self, key, default=None):
        return self.attrib.get(key, default)

    def find(self, tag):
        return next((node for node in self.children if node.tag == tag), None)

    def findall(self, tag):
        return [node for node in self.children if node.tag == tag]

    def itertext(self):
        if self.text:
            yield self.text
        for node in self.children:
            yield from node.itertext()


def slim(element, registry):
    identifier = element.get("id")
    if identifier is not None and identifier in registry:
        return registry[identifier]
    attrib = {key: value for key, value in element.attrib.items() if key != "id"}
    return Node(element.tag, attrib, element.text,
                [slim(nested, registry) for nested in element])


def export_rows(source):
    """Each row of an export as {column mnemonic: node, or None for an empty
    cell}, with a resolve() that swaps a ref for the node it names.

    Only slim copies of id-carrying elements outlive their row, so memory grows
    with the distinct values rather than with the whole tree."""
    registry = {}
    columns = []
    parents = []

    def resolve(node):
        reference = node.get("ref")
        return registry[reference] if reference is not None else node

    for event, element in ElementTree.iterparse(source, events=("start", "end")):
        if event == "start":
            parents.append(element)
            continue
        parents.pop()
        identifier = element.get("id")
        if identifier is not None:
            registry[identifier] = slim(element, registry)
        if element.tag == "schema":
            columns = [column.findtext("mnemonic") for column in element.findall("col")]
        elif element.tag == "row":
            row = {ROW_ATTRIBUTES: dict(element.attrib)}
            for mnemonic, cell in zip(columns, element):
                row[mnemonic] = None if cell.tag == "sentinel" else resolve(slim(cell, registry))
            yield row, resolve
            element.clear()
            if parents:
                parents[-1].remove(element)


def child(element, tag, resolve):
    if element is None:
        return None
    found = element.find(tag)
    return resolve(found) if found is not None else None


def integer(element):
    return int(element.text) if element is not None and element.text else 0


def text(element):
    return element.text if element is not None and element.text else ""


def thread_id(thread, resolve):
    return integer(child(thread, "tid", resolve))


def row_pid(row, thread_column, resolve):
    process = row.get("process")
    if process is None:
        process = child(row.get(thread_column), "process", resolve)
    return integer(child(process, "pid", resolve))


def accepts(pid, wanted):
    return wanted is None or pid == wanted


def message_text(element):
    if element is None:
        return ""
    return element.get("fmt") or "".join(element.itertext())


def parse_intervals(source, pid=None):
    """The com.cavewhere.tasks intervals of an os-signpost-interval export."""
    intervals = []
    for row, resolve in export_rows(source):
        if text(row.get("subsystem")) != SUBSYSTEM:
            continue
        kind = text(row.get("name"))
        if kind not in (TASK_KIND, BATCH_KIND):
            continue
        if not accepts(row_pid(row, "start-thread", resolve), pid):
            continue
        start = integer(row.get("start"))
        intervals.append(Interval(kind, start, start + integer(row.get("duration")),
                                  thread_id(row.get("start-thread"), resolve),
                                  parse_arguments(message_text(row.get("start-message")))))
    return intervals


def parse_events(source, pid=None):
    """The com.cavewhere.tasks Event rows of an os-signpost export, in file order."""
    events = []
    for row, resolve in export_rows(source):
        if text(row.get("event-type")) != "Event" or text(row.get("subsystem")) != SUBSYSTEM:
            continue
        if not accepts(row_pid(row, "thread", resolve), pid):
            continue
        message = message_text(row.get("message"))
        words = message.split()
        name = words[0] if words and "=" not in words[0] else ""
        events.append(Event(integer(row.get("time")), thread_id(row.get("thread"), resolve),
                            name, parse_arguments(message)))
    return events


def thread_name(thread, resolve):
    """The thread's name as Instruments shows it, without the id and process suffix:
    "QSGRenderThread (0x1a2b) (CaveWhere, pid: 42)" is "QSGRenderThread"."""
    return THREAD_SUFFIX.sub("", resolve(thread).get("fmt") or "")


def process_name(thread, resolve):
    process = child(thread, "process", resolve)
    return PROCESS_SUFFIX.sub("", process.get("fmt") or "") if process is not None else ""


def parse_thread_states(source, pid=None):
    """The spans of a thread-state export, in file order."""
    states = []
    for row, resolve in export_rows(source):
        if not accepts(row_pid(row, "thread", resolve), pid):
            continue
        thread = row.get("thread")
        start = integer(row.get("start"))
        states.append(ThreadState(start, start + integer(row.get("duration")),
                                  thread_id(thread, resolve), text(row.get("state")),
                                  thread_name(thread, resolve), process_name(thread, resolve)))
    return states


def stack_frames(stack, resolve):
    backtrace = child(stack, "backtrace", resolve)
    if backtrace is None:
        return []
    return [resolve(frame).get("name", "") for frame in backtrace.findall("frame")]


def parse_samples(source, pid=None):
    """The CPU samples of a time-profile export, in file order."""
    samples = []
    for row, resolve in export_rows(source):
        if not accepts(row_pid(row, "thread", resolve), pid):
            continue
        samples.append(Sample(integer(row.get("time")), thread_id(row.get("thread"), resolve),
                              integer(row.get("weight")), stack_frames(row.get("stack"), resolve)))
    return samples


def parse_detail_rows(source):
    """The rows of an Allocations detail export as {column: displayed text}.

    xctrace 16 writes a detail row's cells as attributes of the row; the schema
    form with one child element per cell is also read."""
    rows = []
    for row, _ in export_rows(source):
        if row.get(ROW_ATTRIBUTES):
            rows.append(dict(row[ROW_ATTRIBUTES]))
        else:
            rows.append({name: (cell.get("fmt") or text(cell)) if cell is not None else ""
                         for name, cell in row.items() if name != ROW_ATTRIBUTES})
    return rows


def parse_launched_pid(source):
    """The pid of the launched or attached process named in an export --toc, or None."""
    for _, element in ElementTree.iterparse(source, events=("end",)):
        if element.tag == "process" and element.get("type") in RECORDED_PROCESS_TYPES and element.get("pid"):
            return int(element.get("pid"))
    return None


# --- Joining samples and thread states to task intervals --------------------------


def innermost_segments(intervals):
    """Disjoint, time-ordered (start, end, interval) spans of one thread, each
    owned by the innermost task interval covering it."""
    points = sorted({interval.start_ns for interval in intervals}
                    | {interval.end_ns for interval in intervals})
    by_start = sorted(intervals, key=lambda interval: interval.start_ns)
    active = []
    next_index = 0
    segments = []
    for left, right in zip(points, points[1:]):
        while next_index < len(by_start) and by_start[next_index].start_ns <= left:
            active.append(by_start[next_index])
            next_index += 1
        active = [interval for interval in active if interval.end_ns > left]
        if active:
            owner = max(active, key=lambda interval: (interval.start_ns, -interval.end_ns))
            segments.append((left, right, owner))
    return segments


def tasks_by(intervals, key):
    grouped = collections.defaultdict(list)
    for interval in intervals:
        if interval.kind == TASK_KIND:
            grouped[key(interval)].append(interval)
    return grouped


def segments_by_thread(intervals):
    """{tid: (segment starts, segments)} for bisecting by time."""
    segments = {}
    for tid, tasks in tasks_by(intervals, lambda interval: interval.tid).items():
        spans = innermost_segments(tasks)
        segments[tid] = ([span[0] for span in spans], spans)
    return segments


def assign_samples(samples, intervals):
    """The task interval covering each sample on its thread, or None."""
    segments = segments_by_thread(intervals)
    assigned = []
    for sample in samples:
        starts, spans = segments.get(sample.tid, ([], []))
        index = bisect.bisect_right(starts, sample.time_ns) - 1
        if index >= 0 and sample.time_ns < spans[index][1]:
            assigned.append(spans[index][2])
        else:
            assigned.append(None)
    return assigned


def state_time_by_context(states, intervals):
    """Nanoseconds per thread state inside each context's task intervals."""
    segments = segments_by_thread(intervals)
    by_context = collections.defaultdict(lambda: collections.defaultdict(int))
    for state in states:
        starts, spans = segments.get(state.tid, ([], []))
        index = max(0, bisect.bisect_right(starts, state.start_ns) - 1)
        while index < len(spans) and spans[index][0] < state.end_ns:
            left, right, owner = spans[index]
            overlap = min(right, state.end_ns) - max(left, state.start_ns)
            if overlap > 0:
                by_context[owner.context][state.state] += overlap
            index += 1
    return {context: dict(times) for context, times in by_context.items()}


# --- Whole-run statistics ---------------------------------------------------------


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(round(fraction * (len(ordered) - 1))))]


def queue_wait_percentiles(intervals):
    """Queue wait p50 and p95 in microseconds per context."""
    waits = collections.defaultdict(list)
    for interval in intervals:
        if interval.kind == TASK_KIND and "wait_us" in interval.args:
            waits[interval.context].append(int(interval.args["wait_us"]))
    return {context: {"p50": percentile(values, 0.5), "p95": percentile(values, 0.95)}
            for context, values in waits.items()}


def merged_spans(intervals):
    spans = []
    for interval in sorted(intervals, key=lambda interval: interval.start_ns):
        if spans and interval.start_ns <= spans[-1][1]:
            spans[-1][1] = max(spans[-1][1], interval.end_ns)
        else:
            spans.append([interval.start_ns, interval.end_ns])
    return spans


def shared_ns(first, second):
    total = 0
    i = j = 0
    while i < len(first) and j < len(second):
        total += max(0, min(first[i][1], second[j][1]) - max(first[i][0], second[j][0]))
        if first[i][1] < second[j][1]:
            i += 1
        else:
            j += 1
    return total


def overlap_matrix(intervals):
    """Seconds each pair of contexts spends running together."""
    spans = {context: merged_spans(tasks)
             for context, tasks in tasks_by(intervals, lambda interval: interval.context).items()}
    return {(first, second): shared_ns(spans[first], spans[second]) / NS_PER_S
            for first in spans for second in spans}


def median_spread(values):
    """The median of the runs and the distance between the extremes."""
    return statistics.median(values), max(values) - min(values)


def compare_metrics(baseline, after):
    """The metrics whose change clears the spread of both sets of runs."""
    deltas = []
    for name, baseline_values in baseline.items():
        if name not in after or not baseline_values or not after[name]:
            continue
        baseline_median, baseline_spread = median_spread(baseline_values)
        after_median, after_spread = median_spread(after[name])
        delta = after_median - baseline_median
        if abs(delta) > max(baseline_spread, after_spread):
            deltas.append(Delta(name, baseline_median, after_median, delta))
    return deltas


def write_metrics(folder, metrics):
    with open(Path(folder) / METRICS_FILE, "w") as output:
        json.dump(metrics, output, indent=1, sort_keys=True)


def load_metrics(folder):
    with open(Path(folder) / METRICS_FILE) as source:
        return json.load(source)


# --- Analysis of one recording ----------------------------------------------------


def peak_threads_wanting_core(states):
    """The most threads wanting a core at once, and the threads at that moment."""
    edges = []
    for state in states:
        if state.state in WANTS_CORE_STATES:
            edges.append((state.start_ns, True, state.tid))
            edges.append((state.end_ns, False, state.tid))
    # An end sorts before a start at the same time
    edges.sort()
    wanting = set()
    peak, at_peak = 0, set()
    for _, starts, tid in edges:
        if starts:
            wanting.add(tid)
        else:
            wanting.discard(tid)
        if len(wanting) > peak:
            peak, at_peak = len(wanting), set(wanting)
    return peak, at_peak


def thread_group(state):
    if state.name == MAIN_THREAD:
        return "main"
    if state.name == RENDER_THREAD:
        return "render"
    if state.name == ENCODE_LANE_THREAD:
        return "encode_lane"
    if state.name == POOL_THREAD:
        return "pool"
    if state.name == state.process:
        return "basisu"
    return "other"


def thread_metrics(states, at_peak):
    """Per thread group: threads created, time Running, time waiting for a core,
    and how many were wanting a core at the runnable peak."""
    groups = {group: {"created": set(), "running_s": 0, "waiting_s": 0, "at_peak": 0}
              for group, _ in THREAD_GROUPS}
    for state in states:
        values = groups[thread_group(state)]
        values["created"].add(state.tid)
        if state.state == "Running":
            values["running_s"] += state.end_ns - state.start_ns
        elif state.state in WAITING_STATES:
            values["waiting_s"] += state.end_ns - state.start_ns
    metrics = {}
    for group, values in groups.items():
        metrics["threads/{}/created".format(group)] = len(values["created"])
        metrics["threads/{}/running_s".format(group)] = values["running_s"] / NS_PER_S
        metrics["threads/{}/waiting_s".format(group)] = values["waiting_s"] / NS_PER_S
        metrics["threads/{}/at_peak".format(group)] = len(values["created"] & at_peak)
    return metrics


def memory_events(events):
    return sorted((event for event in events if event.name == "mem"), key=lambda event: event.time_ns)


def sample_at(memory, time_ns, key):
    """The key's value in the latest sample at or before time_ns, or None before
    the first sample or when that sample lacks the key."""
    times = [event.time_ns for event in memory]
    index = bisect.bisect_right(times, time_ns) - 1
    if index >= 0 and key in memory[index].args:
        return int(memory[index].args[key])
    return None


def footprint_metrics(events, intervals):
    """Peak footprint, when it happened, the budget left then and the
    contexts running at that moment."""
    memory = memory_events(events)
    if not memory:
        return {}, []
    largest = max(int(event.args.get("footprint", 0)) for event in memory)
    # The footprint plateaus once malloc keeps freed large blocks, so the peak is
    # the first sample on the plateau: that is when the blocks were live, and
    # where the B′ recording has to stop to list them
    peak = next(event for event in memory
                if int(event.args.get("footprint", 0)) >= largest * PEAK_PLATEAU_FRACTION)
    metrics = {"peak_footprint_mb": int(peak.args["footprint"]) / BYTES_PER_MB,
               "peak_time_s": peak.time_ns / NS_PER_S}
    if "available" in peak.args:
        metrics["available_at_peak_mb"] = int(peak.args["available"]) / BYTES_PER_MB
    gpu = [int(event.args["gpu"]) for event in memory if "gpu" in event.args]
    if gpu:
        metrics["peak_gpu_mb"] = max(gpu) / BYTES_PER_MB
    if "gpu" in peak.args:
        metrics["gpu_at_peak_mb"] = int(peak.args["gpu"]) / BYTES_PER_MB
    running = sorted({interval.context for interval in intervals
                      if interval.kind == TASK_KIND
                      and interval.start_ns <= peak.time_ns < interval.end_ns})
    return metrics, running


def analyze_timing(intervals, events, states, samples, cores):
    """The run A metrics as {name: number}, plus the details only the report shows."""
    metrics = {}
    details = {}

    idle = [event.time_ns for event in events if event.name == "idle"]
    if idle:
        metrics["time_to_idle_s"] = min(idle) / NS_PER_S
    by_context = tasks_by(intervals, lambda interval: interval.context)
    for context in ("scraps", "lidar"):
        if context in by_context:
            first_end = min(task.end_ns for task in by_context[context])
            metrics["first_{}_s".format(context)] = first_end / NS_PER_S

    footprint, details["running_at_peak"] = footprint_metrics(events, intervals)
    metrics.update(footprint)
    metrics["peak_threads_wanting_core"], at_peak = peak_threads_wanting_core(states)
    metrics.update(thread_metrics(states, at_peak))
    if cores is not None:
        metrics["cores"] = cores
    metrics["cpu_s"] = sum(sample.weight_ns for sample in samples) / NS_PER_S

    assigned = assign_samples(samples, intervals)
    dropped = {(event.args.get("ctx"), event.args.get("gen")) for event in events
               if event.name == "dropped"}
    dropped_ns = sum(sample.weight_ns for sample, owner in zip(samples, assigned)
                     if owner is not None and (owner.context, owner.args.get("gen")) in dropped)
    metrics["dropped_cpu_s"] = dropped_ns / NS_PER_S

    cpu_by_context = collections.defaultdict(int)
    stacks_by_context = collections.defaultdict(collections.Counter)
    for sample, owner in zip(samples, assigned):
        if owner is not None:
            cpu_by_context[owner.context] += sample.weight_ns
            if sample.frames:
                stacks_by_context[owner.context][sample.frames[0]] += sample.weight_ns

    peak_counts = collections.defaultdict(lambda: {"running": 0, "queued": 0})
    for event in events:
        if event.name == "queue":
            peak = peak_counts[event.args.get("ctx", UNTAGGED_CONTEXT)]
            peak["running"] = max(peak["running"], int(event.args.get("running", 0)))
            peak["queued"] = max(peak["queued"], int(event.args.get("queued", 0)))

    waits = queue_wait_percentiles(intervals)
    state_times = state_time_by_context(states, intervals)
    memory = memory_events(events)

    details["top_stacks"] = {}
    for context, tasks in by_context.items():
        first = min(task.start_ns for task in tasks)
        last = max(task.end_ns for task in tasks)
        times = state_times.get(context, {})
        values = {
            "tasks": len(tasks),
            "cpu_s": cpu_by_context[context] / NS_PER_S,
            "span_s": (last - first) / NS_PER_S,
            "peak_running": peak_counts[context]["running"],
            "peak_queued": peak_counts[context]["queued"],
            "wait_p50_us": waits.get(context, {}).get("p50", 0),
            "wait_p95_us": waits.get(context, {}).get("p95", 0),
            "running_s": times.get("Running", 0) / NS_PER_S,
            "waiting_s": sum(times.get(state, 0) for state in WAITING_STATES) / NS_PER_S,
            "charged_mb": sum(int(task.args.get(CHARGED_BYTES_ARG, 0)) for task in tasks) / BYTES_PER_MB,
        }
        # A context that starts before the sampler's first tick has no baseline
        for key in ("footprint", "gpu"):
            before, after = sample_at(memory, first, key), sample_at(memory, last, key)
            if before is not None and after is not None:
                values["{}_growth_mb".format(key)] = (after - before) / BYTES_PER_MB
        for name, value in values.items():
            metrics["{}/{}".format(context, name)] = value
        details["top_stacks"][context] = stacks_by_context[context].most_common(TOP_STACKS)

    for (first, second), seconds in overlap_matrix(intervals).items():
        if first < second:
            metrics["overlap/{}+{}".format(first, second)] = seconds
    return metrics, details


# --- Recording ---------------------------------------------------------------------


def xctrace(*arguments, log=None):
    """Runs xctrace; with log, its output and the launched app's stdout and
    stderr (where --profile-log writes) go to that file."""
    command = ["xcrun", "xctrace"] + list(arguments)
    if log is None:
        subprocess.run(command, check=True)
        return
    with open(log, "w") as output:
        subprocess.run(command, check=True, stdout=output, stderr=subprocess.STDOUT)


def export(trace, xpath, output):
    xctrace("export", "--input", str(trace), "--xpath", xpath, "--output", str(output))
    return output


def app_arguments(arguments, project):
    command = [project, "--page", arguments.page, "--profile-log", "--profile-tasks-quit"]
    redirty = getattr(arguments, "redirty", None)
    if redirty is not None:
        command += ["--profile-tasks-redirty", str(redirty)]
    return command


def record_on_device(arguments, project, command, log):
    xctrace(*(command + ["--device", arguments.device, "--launch", "--", APP_NAME]
              + app_arguments(arguments, project)), log=log)


def record_on_mac(arguments, project, command, log):
    """Starts the app itself and attaches xctrace to it. `xctrace --launch` resolves
    a CaveWhere.app path by name and refuses when other copies are installed. The
    app is held stopped until xctrace reports that it records, so the recording
    covers the project open from its first task, and its stdout and stderr (where
    --profile-log writes) land in the log."""
    executable = Path(arguments.build).resolve() / (APP_NAME + ".app") / "Contents" / "MacOS" / APP_NAME
    with open(log, "w") as output:
        app = subprocess.Popen([str(executable)] + app_arguments(arguments, project),
                               stdout=output, stderr=subprocess.STDOUT)
        os.kill(app.pid, signal.SIGSTOP)
        notification = "{}.{}".format(TRACING_STARTED_NOTIFICATION, app.pid)
        recorder = subprocess.Popen(["xcrun", "xctrace"] + command
                                    + ["--notify-tracing-started", notification, "--attach", str(app.pid)],
                                    stdout=output, stderr=subprocess.STDOUT)
        try:
            subprocess.run(["notifyutil", "-1", notification], check=True, stdout=subprocess.DEVNULL)
        finally:
            os.kill(app.pid, signal.SIGCONT)
        app.wait()
        recorder.wait()
    if recorder.returncode != 0:
        raise subprocess.CalledProcessError(recorder.returncode, recorder.args)


def record_one(arguments, project, trace, instruments, log, time_limit_s=None):
    if trace.exists():
        shutil.rmtree(trace)
    clear_cache(arguments, project)
    command = ["record", "--output", str(trace)] + instruments
    if time_limit_s is not None:
        command += ["--time-limit", "{}ms".format(int(time_limit_s * 1000))]
    if arguments.device:
        record_on_device(arguments, project, command, log)
    else:
        record_on_mac(arguments, project, command, log)
    toc = trace.with_suffix(".toc.xml")
    xctrace("export", "--input", str(trace), "--toc", "--output", str(toc))
    return toc


def record(arguments, project, prefix):
    """Records run A and run B (and B′ with --peak) for one project and exports
    their tables."""
    prefix = Path(prefix)
    exported = {}

    def run(name, instruments, tables, details, time_limit_s=None):
        trace = prefix.with_name("{}-{}.trace".format(prefix.name, name))
        exported[name + "-toc"] = record_one(arguments, project, trace, instruments,
                                             prefix.with_name("{}-{}.log".format(prefix.name, name)),
                                             time_limit_s)
        for xpath, items in ((TABLE_XPATH, tables), (ALLOCATIONS_XPATH, details)):
            for item in items:
                output = prefix.with_name("{}-{}-{}.xml".format(prefix.name, name, slug(item)))
                exported["{}-{}".format(name, item)] = export(trace, xpath.format(item), output)
        # An Allocations trace records every malloc, which runs to several GB per
        # recording; the exported tables carry everything the report reads from it
        if instruments is ALLOCATIONS_INSTRUMENTS and not arguments.keep_traces:
            shutil.rmtree(trace)

    kinds = RUN_KINDS[arguments.instruments]
    if "A" in kinds:
        run("A", TIME_PROFILER_INSTRUMENTS, RUN_A_TABLES, [])
    if "B" in kinds:
        run("B", ALLOCATIONS_INSTRUMENTS, RUN_B_TABLES, RUN_B_DETAILS)

    if arguments.peak and "B" in kinds:
        # Allocations slows the app, so B's own sampler places the peak
        with open(exported["B-os-signpost"], "rb") as source:
            events = parse_events(source, launched_pid(exported["B-toc"]))
        footprint, _ = footprint_metrics(events, [])
        if "peak_time_s" in footprint:
            run("Bpeak", ALLOCATIONS_INSTRUMENTS, [], RUN_B_PEAK_DETAILS, footprint["peak_time_s"])
    return exported


def clear_cache(arguments, project):
    if not arguments.cold:
        return
    if arguments.device:
        input("Delete {} from the Files app, copy it over again, then press Return: ".format(project))
        return
    cache = Path(project).expanduser().resolve().parent / ".cw_cache"
    if cache.exists():
        shutil.rmtree(cache)


def sign(build):
    """Ad hoc signs the app with get-task-allow, which Instruments needs to attach."""
    entitlements = Path(__file__).resolve().parent.parent / "entitlements.plist"
    subprocess.run(["codesign", "-f", "-s", "-", "--entitlements", str(entitlements),
                    str(Path(build) / (APP_NAME + ".app"))], check=True)


def slug(text_value):
    return "".join(character.lower() if character.isalnum() else "-" for character in text_value)


def launched_pid(toc):
    with open(toc, "rb") as source:
        return parse_launched_pid(source)


def read_exports(exported, run):
    """The parsed tables of one run kind; every table empty when it was not recorded."""
    if run + "-toc" not in exported:
        return {"intervals": [], "events": [], "states": [], "samples": []}
    pid = launched_pid(exported[run + "-toc"])

    def read(table, parser):
        key = "{}-{}".format(run, table)
        if key not in exported:
            return []
        with open(exported[key], "rb") as source:
            return parser(source, pid)

    return {
        "intervals": read("os-signpost-interval", parse_intervals),
        "events": read("os-signpost", parse_events),
        "states": read("thread-state", parse_thread_states),
        "samples": read("time-profile", parse_samples),
    }


def read_details(exported, key):
    if key not in exported:
        return []
    with open(exported[key], "rb") as source:
        return parse_detail_rows(source)


# --- Report ------------------------------------------------------------------------


def command_output(*command):
    try:
        return subprocess.run(command, capture_output=True, text=True).stdout.strip()
    except OSError:
        return ""


def cache_state(arguments, project):
    # A bundled .cw unpacks into a fresh temporary folder, cache included, on every open
    if Path(project).suffix == BUNDLE_SUFFIX:
        return "cold (bundle)"
    return "cold" if arguments.cold else "warm"


def header(arguments, project):
    # Only the Mac running the script can be asked; a device's own values are unknown here
    if arguments.device:
        os_version, cores = UNKNOWN, UNKNOWN
    else:
        os_version, cores = platform.mac_ver()[0] or platform.platform(), str(os.cpu_count())
    settings = {key: command_output("defaults", "read", DEFAULTS_DOMAIN, key) or "default"
                for key in SETTING_KEYS}
    return [
        ("Commit", command_output("git", "-C", str(Path(__file__).resolve().parent), "rev-parse", "HEAD")),
        ("Device", arguments.device or command_output("sysctl", "-n", "hw.model")),
        ("OS", os_version),
        ("Cores", cores),
        ("Thread count", settings["threadCount"]),
        ("Memory budget MB", settings["memoryBudgetMb"]),
        ("App Nap", "disabled by the runner" if not arguments.device else UNKNOWN),
        ("Automatic update", settings["automaticUpdate"]),
        ("Build", str(arguments.build)),
        ("Project", project),
        ("Cache", cache_state(arguments, project)),
        ("Runs", str(arguments.runs)),
        ("Redirty after s", redirty_label(arguments)),
    ]


def redirty_label(arguments):
    redirty = getattr(arguments, "redirty", None)
    return "none" if redirty is None else str(redirty)


def markdown(columns, rows):
    lines = ["| " + " | ".join(columns) + " |", "|" + "---|" * len(columns)]
    lines += ["| " + " | ".join(str(cell) for cell in row) + " |" for row in rows]
    return "\n".join(lines) + "\n"


def number(value):
    if isinstance(value, float) and value != int(value):
        return "{:.2f}".format(value)
    return "{:,}".format(int(value))


def summary(values):
    if not values:
        return "—"
    middle, spread = median_spread(values)
    return "{} ± {}".format(number(middle), number(spread)) if spread else number(middle)


def collect(runs):
    """{name: [value per run]} from a list of per-run metric dicts."""
    collected = collections.defaultdict(list)
    for run in runs:
        for name, value in run.items():
            collected[name].append(value)
    return dict(collected)


CONTEXT_COLUMNS = [
    ("tasks", "Tasks"), ("cpu_s", "CPU s"), ("span_s", "Wall span s"),
    ("peak_running", "Peak running"), ("peak_queued", "Peak queued"),
    ("wait_p50_us", "Wait p50 µs"), ("wait_p95_us", "Wait p95 µs"),
    ("running_s", "Running s"), ("waiting_s", "Runnable + Preempted s"),
    ("footprint_growth_mb", "Footprint growth MB"), ("charged_mb", "Charged MB"),
    ("gpu_growth_mb", "GPU growth MB"),
]

WHOLE_RUN_ROWS = [
    ("time_to_idle_s", "Time to idle s"), ("first_scraps_s", "First scraps task done s"),
    ("first_lidar_s", "First LiDAR task done s"), ("peak_footprint_mb", "Peak footprint MB"),
    ("peak_time_s", "Peak at s"), ("available_at_peak_mb", "Budget left at peak MB"),
    ("peak_gpu_mb", "Peak GPU MB"), ("gpu_at_peak_mb", "GPU at footprint peak MB"),
    ("peak_threads_wanting_core", "Peak runnable threads"), ("cores", "Cores"),
    ("cpu_s", "Total CPU s"), ("dropped_cpu_s", "CPU inside dropped generations s"),
]


FIRST_TASK_NOTE = ("First scraps and LiDAR task done is the end of the earliest task in that context: "
                   "the first stage of a chain, which can land well before the first mesh is delivered.\n")
THREAD_NOTE = ("Qt pool threads are the task pool and the global pool together, which Qt both names "
               "`Thread (pooled)`; basisu's job_pool threads, created per KTX2 encode, carry the process "
               "name. Created counts distinct thread ids over the run, so a pool that expires and recreates "
               "its workers counts each one.\n")
BATCH_NOTE = ("Only `task` intervals are attributed. Each item of a cwConcurrent::map/mapped batch "
              "(such as the octree decode and halo passes) is a task named `map` or `mapped` on its "
              "worker, so it counts toward its context like a run() task; the enclosing `batch` interval "
              "only marks the submission span and is skipped.\n")


def thread_report(timing):
    """The UI-thread table: who wanted a core, and for how long."""
    rows = []
    for group, label in THREAD_GROUPS:
        values = [summary(timing.get("threads/{}/{}".format(group, key), []))
                  for key, _ in THREAD_GROUP_METRICS]
        if any(value != "—" for value in values):
            rows.append([label] + values)
    return markdown(["Thread"] + [label for _, label in THREAD_GROUP_METRICS], rows)


def project_report(head, timing, timing_details, memory, memory_details, allocations, peak_allocations):
    parts = ["## {}\n".format(dict(head)["Project"]), markdown(["", ""], head)]
    if not timing:
        parts.append(memory_report(memory, memory_details, allocations, peak_allocations))
        return "\n".join(parts)

    parts.append("### Whole run (A)\n")
    parts.append(markdown(["Metric", "Median ± spread"],
                          [(label, summary(timing.get(name, []))) for name, label in WHOLE_RUN_ROWS
                           if name in timing]))
    parts.append(FIRST_TASK_NOTE)
    if timing_details:
        parts.append("Running at the peak (last run): {}\n".format(
            ", ".join(timing_details[-1]["running_at_peak"]) or "nothing"))

    contexts = sorted({name.split("/")[0] for name in timing
                       if "/" in name and not name.startswith("overlap/")})
    # gpu= appears only on Apple platforms, so its column shows only when some context has it
    columns = [(key, label) for key, label in CONTEXT_COLUMNS
               if key != "gpu_growth_mb"
               or any("{}/{}".format(context, key) in timing for context in contexts)]
    parts.append("### Per context (A)\n")
    parts.append(markdown(["Context"] + [label for _, label in columns],
                          [[context] + [summary(timing.get("{}/{}".format(context, key), []))
                                        for key, _ in columns] for context in contexts]))
    parts.append(BATCH_NOTE)
    if timing_details:
        parts.append("Top leaf frames by CPU, last run:\n")
        for context in contexts:
            stacks = timing_details[-1]["top_stacks"].get(context, [])
            listed = ", ".join("{} ({:.2f} s)".format(frame, ns / NS_PER_S) for frame, ns in stacks)
            parts.append("- {}: {}".format(context, listed or "no samples"))
        parts.append("")

    parts.append("### Overlap, seconds running together (A)\n")
    rows = []
    for first in contexts:
        row = [first]
        for second in contexts:
            pair = "overlap/{}+{}".format(*sorted((first, second)))
            row.append("—" if first == second else summary(timing.get(pair, [0])))
        rows.append(row)
    parts.append(markdown([""] + contexts, rows))

    parts.append("### Threads (A)\n")
    parts.append(thread_report(timing))
    parts.append(THREAD_NOTE)

    if memory:
        parts.append(memory_report(memory, memory_details, allocations, peak_allocations))
    return "\n".join(parts)


def memory_report(memory, memory_details, allocations, peak_allocations):
    parts = ["### Memory (B)\n"]
    parts.append(markdown(["Metric", "Median ± spread"],
                          [(label, summary(memory.get(name, []))) for name, label in WHOLE_RUN_ROWS
                           if name in memory]))
    if memory_details:
        parts.append("Running at the peak (last run): {}\n".format(
            ", ".join(memory_details[-1]["running_at_peak"]) or "nothing"))
    if allocations:
        columns = list(allocations[0])
        parts.append("Allocation statistics, last run:\n")
        parts.append(markdown(columns, [[row.get(column, "") for column in columns]
                                        for row in allocations[:ALLOCATION_ROWS]]))
    if peak_allocations:
        parts.append("Live allocations at the peak (B′), last run, grouped by caller:\n")
        parts.append(markdown(["MB", "Blocks", "Category", "Library", "Responsible caller"],
                              live_by_caller(peak_allocations)[:ALLOCATION_ROWS]))
    return "\n".join(parts)


def live_by_caller(rows):
    """The live allocation list summed per (category, library, caller), largest
    first. One row per block is unreadable: the list runs to hundreds of
    thousands of small blocks, and the question is who holds the bytes."""
    totals = collections.Counter()
    counts = collections.Counter()
    for row in rows:
        key = (row.get("category", ""), row.get("responsible-library", ""),
               row.get("responsible-caller", ""))
        totals[key] += int(row.get("size", "0") or 0)
        counts[key] += 1
    return [["{:.1f}".format(total / BYTES_PER_MB), counts[key], *key]
            for key, total in totals.most_common()]


APP_NAP_KEY = "NSAppSleepDisabled"


@contextlib.contextmanager
def app_nap_disabled(arguments):
    """macOS throttles an app whose window sits behind others (App Nap) to a background
    scheduling class: a recording made that way runs its workers at priority 4 instead of
    31, taking three times as long. The key is machine-wide, so the previous value is put
    back when the recordings end."""
    if arguments.device:
        yield
        return
    previous = command_output("defaults", "read", DEFAULTS_DOMAIN, APP_NAP_KEY)
    subprocess.run(["defaults", "write", DEFAULTS_DOMAIN, APP_NAP_KEY, "-bool", "YES"], check=True)
    try:
        yield
    finally:
        if previous:
            subprocess.run(["defaults", "write", DEFAULTS_DOMAIN, APP_NAP_KEY, "-bool", previous], check=False)
        else:
            subprocess.run(["defaults", "delete", DEFAULTS_DOMAIN, APP_NAP_KEY], check=False)


def record_projects(arguments, out, cores):
    all_metrics = {}
    sections = []
    for project in arguments.project:
        name = Path(project).stem
        timing_runs, timing_details, memory_runs, memory_details = [], [], [], []
        allocations, peak_allocations = [], []
        for run in range(arguments.runs):
            exported = record(arguments, project, out / "{}-run{}".format(name, run + 1))
            tables_a = read_exports(exported, "A")
            timing, details = analyze_timing(tables_a["intervals"], tables_a["events"],
                                             tables_a["states"], tables_a["samples"], cores)
            timing_runs.append(timing)
            timing_details.append(details)
            tables_b = read_exports(exported, "B")
            memory, running = footprint_metrics(tables_b["events"], tables_b["intervals"])
            memory_runs.append(memory)
            memory_details.append({"running_at_peak": running})
            allocations = read_details(exported, "B-Statistics")
            peak_allocations = read_details(exported, "Bpeak-Allocations List")

        timing = collect(timing_runs)
        memory = collect(memory_runs)
        all_metrics.update({"{}/A/{}".format(name, key): values for key, values in timing.items()})
        all_metrics.update({"{}/B/{}".format(name, key): values for key, values in memory.items()})
        sections.append(project_report(header(arguments, project), timing, timing_details,
                                       memory, memory_details, allocations, peak_allocations))

    return sections, all_metrics


def profile(arguments):
    out = Path(arguments.out)
    out.mkdir(parents=True, exist_ok=True)
    if not arguments.device:
        sign(arguments.build)

    cores = None if arguments.device else os.cpu_count()
    with app_nap_disabled(arguments):
        sections, all_metrics = record_projects(arguments, out, cores)
    write_metrics(out, all_metrics)
    (out / REPORT_FILE).write_text("# Task pool profile\n\n" + "\n".join(sections))
    print(out / REPORT_FILE)


def compare(baseline_folder, after_folder):
    deltas = compare_metrics(load_metrics(baseline_folder), load_metrics(after_folder))
    if not deltas:
        print("No difference clears the spread of the runs.")
        return
    print(markdown(["Metric", "Baseline", "After", "Delta"],
                   [(delta.name, number(delta.baseline), number(delta.after),
                     "{:+.2f}".format(delta.delta)) for delta in deltas]))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", help="the build directory holding CaveWhere.app")
    parser.add_argument("--project", action="append", default=[],
                        help="a project to open; repeat for several")
    parser.add_argument("--cold", action="store_true",
                        help="delete a .cwproj's .cw_cache before each recording "
                             "(a bundled .cw opens cold every time)")
    parser.add_argument("--out", help="the output folder for traces, report.md and metrics.json")
    parser.add_argument("--device", help="record on this connected device instead of the Mac")
    parser.add_argument("--runs", type=int, default=DEFAULT_RUNS,
                        help="recordings per project and run kind")
    parser.add_argument("--page", default=DEFAULT_PAGE, help="the page to open the app on")
    parser.add_argument("--redirty", type=float, default=None, metavar="SECONDS",
                        help="mark every scrap and LiDAR note dirty once, this many seconds after the project loads")
    parser.add_argument("--keep-traces", action="store_true",
                        help="keep the Allocations .trace bundles after their tables are exported")
    parser.add_argument("--peak", action="store_true",
                        help="also record B′: run B again, stopped where run B peaked")
    parser.add_argument("--instruments", choices=sorted(RUN_KINDS), default=DEFAULT_RUN_KINDS,
                        help="which recordings to make: A (Time Profiler), B (Allocations) or both")
    parser.add_argument("--compare", nargs=2, metavar=("BASELINE", "AFTER"),
                        help="print the differences between two output folders")
    arguments = parser.parse_args(argv)

    if arguments.compare:
        compare(*arguments.compare)
        return 0

    if not arguments.project or not arguments.out:
        parser.error("--project and --out are required unless --compare is given")
    if not arguments.device and not arguments.build:
        parser.error("--build is required when recording on the Mac")
    profile(arguments)
    return 0


if __name__ == "__main__":
    sys.exit(main())

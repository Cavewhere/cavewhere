#!/usr/bin/env python3
"""Replays an interaction recording against a CaveWhere build and reports what
the cw.profile.* logging categories wrote.

    scripts/profile_pointcloud.py \
        --build build/Qt_6_11_2_for_macOS_Release2 \
        --recording build/profile/recordings/spin-profile-ortho.cwrec \
        --gpu-budget-mb 1024 --out build/profile/spin/run-1024

The app is launched with --profile-log and the --profile-* overrides, its stderr
goes to <prefix>.log, `sample` and `top` traces are taken while it runs, and the
log is parsed into the render, picking and eviction tables.

The overrides persist through QSettings, so the three preferences are read with
`defaults read` before the launch and written back with `defaults write` however
the run ends.

Python 3 standard library only.
"""

import argparse
import os
import shutil
import statistics
import subprocess
import sys
import time

DEFAULTS_DOMAIN = "com.cavewhere.CaveWhere"

# --build-budget-mb reaches the octree builder through the environment: the
# build runs before any window is up, so no --profile-* option can carry it.
BUILD_BUDGET_VARIABLE = "CW_BUILD_MEMORY_BUDGET_MB"

# Read before a run and restored after it, whatever the app did to them.
PREFERENCE_KEYS = [
    "rendering.gpuMemoryBudgetMb",
    "rendering.screenSpaceErrorPx",
    "rendering.pointBudgetMillions",
]

# Every profile line is prefixed with the process time so a block's frame rate
# and a query's arrival can be read back out of the log.
MESSAGE_PATTERN = "%{time process}|%{message}"

SAMPLE_SECONDS = 10
TOP_SAMPLES = 40
POLL_SECONDS = 0.5
SHUTDOWN_TIMEOUT_SECONDS = 20.0
BUILD_TIMEOUT_SECONDS = 1800.0


def median(values):
    return statistics.median(values) if values else 0.0


def percentile(values, fraction):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(round(fraction * (len(ordered) - 1))))
    return ordered[index]


def number(value):
    if isinstance(value, float) and value != int(value):
        return "{:.1f}".format(value)
    return "{:,}".format(int(value))


def parse_log(path):
    """The profile lines of one run, grouped by kind. A build line names its
    pass before its fields, so it is kept as written."""
    lines = {"render": [], "pick": [], "load": [], "build": []}
    if not os.path.exists(path):
        return lines

    with open(path, "r", errors="replace") as log:
        for raw in log:
            seconds, _, rest = raw.rpartition("|")
            rest = rest.strip()
            kind, _, fields = rest.partition(" ")
            if kind not in lines:
                continue

            if kind == "build":
                lines[kind].append(rest)
                continue

            record = {"t": to_number(seconds.strip())}
            for pair in fields.split(" "):
                key, _, value = pair.partition("=")
                if value:
                    record[key] = to_value(value)
            lines[kind].append(record)
    return lines


def to_value(text):
    """A field's value: a number where it is one, the text itself otherwise
    (kind=exactHit has to survive as a name)."""
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return text


def to_number(text):
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return 0.0


def frames_per_second(blocks):
    """One frame rate per block, from the process time between its lines."""
    rates = []
    for previous, block in zip(blocks, blocks[1:]):
        elapsed = block["t"] - previous["t"]
        if elapsed > 0:
            rates.append(block.get("frames", 0) / elapsed)
    return rates


def render_table(blocks, cpu, caption):
    if not blocks:
        return "No cw.profile.render lines in " + caption + ".\n"

    def med(key):
        return median([block.get(key, 0) for block in blocks])

    def top(key):
        return max([block.get(key, 0) for block in blocks], default=0)

    rates = frames_per_second(blocks)
    rows = [[
        caption,
        "{:.0f} %".format(cpu["p90"]) if cpu else "-",
        "{:.1f}".format(median(rates)),
        "{} / {}".format(number(med("cutMed")), number(top("cutMax"))),
        number(med("resident")),
        "{:.3f}".format(med("sse")),
        "{:.0f} / {:.0f}".format(med("gpuMb"), med("budgetMb")),
        number(med("gatherMeanUs")),
        number(med("selectNodesMeanUs")),
        number(med("streamMeanUs")),
        number(med("enforceBudgetMeanUs")),
        number(med("publishStatsMeanUs")),
    ]]
    header = ["Run", "Process CPU", "fps", "sel med / max", "res", "sse",
              "GPU MB / budget", "gather us", "selectNodes us",
              "streamResources us", "enforceGpuBudget us",
              "publishPointCloudStats us"]
    return markdown(header, rows)


def pick_table(queries, caption):
    if not queries:
        return "No cw.profile.pick lines in " + caption + ".\n"

    groups = [
        ("exactHit, ray hits", lambda q: q.get("kind") == "exactHit" and q.get("hit") == 1),
        ("exactHit, ray misses", lambda q: q.get("kind") == "exactHit" and q.get("hit") == 0),
        ("nearestPoint, ray hits", lambda q: q.get("kind") == "nearestPoint" and q.get("hit") == 1),
        ("nearestPoint, ray misses", lambda q: q.get("kind") == "nearestPoint" and q.get("hit") == 0),
    ]

    rows = []
    for name, matches in groups:
        group = [query for query in queries if matches(query)]
        if not group:
            continue

        passing = [query.get("passing", 0) for query in group]
        points = [query.get("points", 0) for query in group]
        micros = [query.get("us", 0) for query in group]
        prunable = sum(query.get("prunable", 0) for query in group)
        rows.append([
            name,
            number(len(group)),
            number(median([query.get("nodes", 0) for query in group])),
            "{} / {}".format(number(median(passing)), number(percentile(passing, 0.95))),
            "{} / {}".format(number(median(points)), number(percentile(points, 0.95))),
            "{} / {}".format(number(median(micros)), number(percentile(micros, 0.95))),
            "{:.1f} %".format(100.0 * prunable / max(1, sum(passing))),
        ])

    # The log line carries node counts, so this column is a share of the nodes
    # that passed, where §1.2's scripted column is a share of the points.
    header = ["Query", "Queries", "Snapshot nodes", "Passing med / p95",
              "Points scanned med / p95", "us med / p95", "Prunable nodes"]
    return markdown(header, rows)


def eviction_table(blocks, loads, caption):
    if not blocks:
        return "No cw.profile.render lines in " + caption + ".\n"

    evictions = [block.get("evictions", 0) for block in blocks]
    uploads = [block.get("uploads", 0) for block in blocks]
    budget = median([block.get("budgetMb", 0) for block in blocks])
    ledger = median([block.get("gpuMb", 0) for block in blocks])

    span = blocks[-1]["t"] - blocks[0]["t"]
    rows = [[
        "{:.0f} MB".format(budget),
        "{:.0f} MB".format(ledger),
        "{:.2f}x".format(ledger / budget) if budget else "-",
        "{:.3f}".format(median([block.get("sse", 1.0) for block in blocks])),
        "{} / {}".format(number(median(evictions)), number(max(evictions, default=0))),
        "{} / {}".format(number(median(uploads)), number(max(uploads, default=0))),
        "{:.0f}".format(len(loads) / span) if span > 0 else "-",
    ]]
    header = ["Budget", "Ledger settles at", "Ratio", "sse settles at",
              "Evictions per block med / max", "Uploads per block med / max",
              "Loads/s"]
    return markdown(header, rows)


def load_table(loads, caption, peak_rss_bytes=None, wall_seconds=None):
    if not loads:
        return "No cw.profile.load lines in " + caption + ".\n"

    micros = [load.get("us", 0) for load in loads]
    total_bytes = sum(load.get("bytes", 0) for load in loads)
    rows = [[
        number(len(loads)),
        "{:.0f} MB".format(total_bytes / (1024.0 * 1024.0)),
        "{} / {}".format(number(median(micros)), number(percentile(micros, 0.95))),
        "{:.1f} s".format(wall_seconds) if wall_seconds is not None else "-",
        "{:.2f} GB".format(peak_rss_bytes / (1024.0 ** 3))
        if peak_rss_bytes is not None else "-",
    ]]
    header = ["Loads", "Bytes read", "us med / p95", "Wall", "Peak RSS"]
    return markdown(header, rows)


def markdown(header, rows):
    if not rows:
        return "(nothing to report)\n"

    widths = [len(name) for name in header]
    for row in rows:
        widths = [max(width, len(cell)) for width, cell in zip(widths, row)]

    def line(cells):
        return "| " + " | ".join(cell.ljust(width)
                                 for cell, width in zip(cells, widths)) + " |"

    out = [line(header), "| " + " | ".join("-" * width for width in widths) + " |"]
    out += [line(row) for row in rows]
    return "\n".join(out) + "\n"


def parse_top(path):
    """p90 and max of the process CPU column of a `top` trace."""
    if not os.path.exists(path):
        return None

    percentages = []
    with open(path, "r", errors="replace") as trace:
        for raw in trace:
            fields = raw.split()
            if len(fields) >= 2 and fields[0].isdigit():
                try:
                    percentages.append(float(fields[1]))
                except ValueError:
                    pass

    if not percentages:
        return None
    return {"p90": percentile(percentages, 0.90), "max": max(percentages)}


# `defaults write` stores a string unless it is told the type, and the app
# reads these back as an int and a double, so the type is read with the value
# and written back with it.
TYPE_FLAGS = {
    "integer": "-int",
    "float": "-float",
    "boolean": "-bool",
    "string": "-string",
}


def read_preferences():
    """The value and the type of each preference, or None where it is unset."""
    stored = {}
    for key in PREFERENCE_KEYS:
        value = subprocess.run(["defaults", "read", DEFAULTS_DOMAIN, key],
                               capture_output=True, text=True)
        if value.returncode != 0:
            stored[key] = None
            continue

        kind = subprocess.run(["defaults", "read-type", DEFAULTS_DOMAIN, key],
                              capture_output=True, text=True)
        # "Type is integer"
        name = kind.stdout.strip().rpartition(" ")[2] if kind.returncode == 0 else ""
        stored[key] = (value.stdout.strip(), TYPE_FLAGS.get(name, "-string"))
    return stored


def restore_preferences(stored):
    for key, entry in stored.items():
        if entry is None:
            subprocess.run(["defaults", "delete", DEFAULTS_DOMAIN, key],
                           capture_output=True, text=True)
        else:
            value, flag = entry
            subprocess.run(["defaults", "write", DEFAULTS_DOMAIN, key, flag, value],
                           capture_output=True, text=True)


def application_path(build):
    return os.path.join(build, "CaveWhere.app", "Contents", "MacOS", "CaveWhere")


def launch_environment(arguments):
    """The app's environment: the message pattern, ASan where the build needs
    it, and the octree build's memory budget."""
    environment = dict(os.environ)
    environment["QT_MESSAGE_PATTERN"] = MESSAGE_PATTERN
    if arguments.debug:
        environment["ASAN_OPTIONS"] = "detect_container_overflow=0"
    if arguments.build_budget_mb is not None:
        environment[BUILD_BUDGET_VARIABLE] = str(arguments.build_budget_mb)
    return environment


def trace_while_running(process, prefix):
    """A `sample` and a `top` trace of the run, taken while it is under way."""
    top = subprocess.Popen(
        ["top", "-pid", str(process.pid), "-stats", "pid,cpu,th",
         "-l", str(TOP_SAMPLES)],
        stdout=open(prefix + "-top.txt", "w"), stderr=subprocess.DEVNULL)
    sample = subprocess.Popen(
        ["sample", str(process.pid), str(SAMPLE_SECONDS),
         "-file", prefix + "-sample.txt"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return top, sample


def wait_for_traces(traces):
    for process in traces:
        try:
            process.wait(timeout=SHUTDOWN_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            process.kill()


def replay(arguments):
    prefix = arguments.out
    os.makedirs(os.path.dirname(os.path.abspath(prefix)) or ".", exist_ok=True)

    command = [application_path(arguments.build),
               "--profile-log",
               "--replay-interaction", arguments.recording,
               "--replay-exit",
               "--replay-speed", str(arguments.speed)]
    if arguments.gpu_budget_mb is not None:
        command += ["--profile-gpu-budget-mb", str(arguments.gpu_budget_mb)]
    if arguments.sse_px is not None:
        command += ["--profile-sse-px", str(arguments.sse_px)]
    if arguments.point_budget is not None:
        command += ["--profile-point-budget-millions", str(arguments.point_budget)]

    started = time.monotonic()
    with open(prefix + ".log", "w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                   env=launch_environment(arguments))
        traces = trace_while_running(process, prefix)
        process.wait()
    wait_for_traces(traces)
    wall = time.monotonic() - started

    lines = parse_log(prefix + ".log")
    caption = os.path.basename(arguments.recording)
    if arguments.gpu_budget_mb is not None:
        caption += " at {} MB".format(arguments.gpu_budget_mb)
    if arguments.sse_px is not None:
        caption += ", sse {}".format(arguments.sse_px)
    if arguments.point_budget is not None:
        caption += ", {} M points".format(arguments.point_budget)

    print("## Render thread — {} ({:.1f} s)\n".format(caption, wall))
    print(render_table(lines["render"], parse_top(prefix + "-top.txt"), caption))
    print("## Picking — {}\n".format(caption))
    print(pick_table(lines["pick"], caption))
    print("## Eviction and inflation — {}\n".format(caption))
    print(eviction_table(lines["render"], lines["load"], caption))
    print("## Node loads — {}\n".format(caption))
    print(load_table(lines["load"], caption, wall_seconds=wall))


def cache_directory(project):
    return os.path.join(os.path.dirname(os.path.abspath(project)), ".cw_cache")


def has_manifest(cache):
    for root, _, files in os.walk(cache):
        for name in files:
            if name.endswith("-manifest"):
                return os.path.join(root, name)
    return None


def resident_bytes(pid):
    """The process's resident size right now, from `ps`, or 0 once it is gone."""
    reading = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)],
                             capture_output=True, text=True)
    text = reading.stdout.strip()
    return int(text) * 1024 if text.isdigit() else 0


def stop(process):
    """Ask CaveWhere to quit, and make sure it is gone before returning."""
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=SHUTDOWN_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def survivors(launched_pid):
    """Any CaveWhere still running once this run's app is gone.

    The launched pid is dropped, so what is left was started by somebody else:
    the check this script owns is that it leaves nothing of its own behind.
    """
    found = subprocess.run(["pgrep", "-f", "CaveWhere.app"],
                           capture_output=True, text=True)
    return [pid for pid in found.stdout.split()
            if pid.isdigit() and int(pid) != launched_pid]


def build_only(arguments):
    """The first open of a project whose octree cache has been thrown away.

    CaveWhere is launched directly rather than under `/usr/bin/time -l`: a
    wrapper's pid is what the traces would follow, and terminating the wrapper
    leaves the app running with the whole build's memory still held. Peak RSS
    comes instead from the builder's own `build total peakRssBytes=` line (the
    task_info reading `time -l` prints as maximum resident set size) and from
    this runner's `ps` sampling below.
    """
    prefix = arguments.out
    project = arguments.build_only
    cache = cache_directory(project)

    if os.path.isdir(cache):
        shutil.rmtree(cache)

    command = [application_path(arguments.build), "--profile-log", project]
    started = time.monotonic()
    peak_rss_bytes = 0
    with open(prefix + ".log", "w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                   env=launch_environment(arguments))
        traces = trace_while_running(process, prefix)

        manifest = None
        while manifest is None and process.poll() is None:
            if time.monotonic() - started > BUILD_TIMEOUT_SECONDS:
                break
            peak_rss_bytes = max(peak_rss_bytes, resident_bytes(process.pid))
            manifest = has_manifest(cache)
            time.sleep(POLL_SECONDS)

        wall = time.monotonic() - started
        peak_rss_bytes = max(peak_rss_bytes, resident_bytes(process.pid))
        stop(process)
    wait_for_traces(traces)

    lines = parse_log(prefix + ".log")
    caption = os.path.basename(project)
    if arguments.build_budget_mb is not None:
        caption += " at a {} MB build budget".format(arguments.build_budget_mb)
    print("## Octree build — {} ({})\n".format(
        caption, "manifest written" if manifest else "no manifest"))
    for line in lines["build"]:
        print("    " + line)

    # The app is stopped at the manifest, so a run can end with no load line at
    # all: the runner's own wall and peak stand on their own here.
    print("    runner wall={:.1f}s peakRssBytes={} ({:.2f} GB, sampled with ps)".format(
        wall, peak_rss_bytes, peak_rss_bytes / (1024.0 ** 3)))
    print("")
    print(load_table(lines["load"], os.path.basename(project),
                     peak_rss_bytes=peak_rss_bytes, wall_seconds=wall))

    print("CaveWhere pid {} exited with {}.".format(process.pid, process.returncode))
    left = survivors(process.pid)
    if left:
        print("Another CaveWhere, which this run did not launch, is running as pid "
              + ", ".join(left) + ".")
    print("")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True,
                        help="build directory holding CaveWhere.app")
    parser.add_argument("--out", required=True,
                        help="prefix for the .log, -top.txt and -sample.txt files")
    parser.add_argument("--recording", help="the .cwrec file to replay")
    parser.add_argument("--gpu-budget-mb", type=int,
                        help="--profile-gpu-budget-mb override")
    parser.add_argument("--sse-px", type=float, help="--profile-sse-px override")
    parser.add_argument("--point-budget", type=int,
                        help="--profile-point-budget-millions override")
    parser.add_argument("--speed", type=float, default=1.0,
                        help="replay speed, 1.0 keeps the recorded timing")
    parser.add_argument("--debug", action="store_true",
                        help="the Debug build needs ASAN_OPTIONS")
    parser.add_argument("--build-budget-mb", type=int,
                        help="CW_BUILD_MEMORY_BUDGET_MB, the memory the octree "
                             "build's chunks may hold together")
    parser.add_argument("--build-only", metavar="PROJECT",
                        help="open PROJECT with its octree cache deleted and "
                             "report the build instead of replaying")
    arguments = parser.parse_args()

    if not arguments.build_only and not arguments.recording:
        parser.error("one of --recording or --build-only is required")

    stored = read_preferences()
    try:
        if arguments.build_only:
            build_only(arguments)
        else:
            replay(arguments)
    finally:
        restore_preferences(stored)


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Tests for profile_tasks.py.

Run with ``python3 -m unittest scripts/test_profile_tasks.py``.

The XML fixtures below follow what ``xctrace export --xpath`` wrote for a test
recording with xctrace 16.0: a <schema> of columns, then one <row> per entry
whose children follow the column order, with <sentinel/> for an empty cell.
Every element that repeats is written once with an id and later as
``<tag ref="id"/>``, so the parser must resolve references, including a ref
nested inside another element (a thread's process, a backtrace's frames).
Each fixture also carries rows from a second process (pid 200) or another
subsystem that the parser has to leave out.

The xctrace invocation itself is left untested; everything after the export
reads plain XML and is covered here.
"""

import contextlib
import io
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import profile_tasks as tasks  # noqa: E402

APP_PID = 100
MS = 1_000_000
MB = 1024 * 1024
SECOND = 1_000_000_000

INTERVAL_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/data[1]/table[20]'><schema name="os-signpost-interval"><col><mnemonic>start</mnemonic><name>Start</name><engineering-type>start-time</engineering-type></col><col><mnemonic>duration</mnemonic><name>Duration</name><engineering-type>duration</engineering-type></col><col><mnemonic>layout-qualifier</mnemonic><name>Layout ID</name><engineering-type>layout-id</engineering-type></col><col><mnemonic>name</mnemonic><name>Name</name><engineering-type>string</engineering-type></col><col><mnemonic>category</mnemonic><name>Category</name><engineering-type>category</engineering-type></col><col><mnemonic>subsystem</mnemonic><name>Subsystem</name><engineering-type>subsystem</engineering-type></col><col><mnemonic>identifier</mnemonic><name>Identifier</name><engineering-type>os-signpost-identifier</engineering-type></col><col><mnemonic>process</mnemonic><name>Start Process</name><engineering-type>process</engineering-type></col><col><mnemonic>end-process</mnemonic><name>End Process</name><engineering-type>process</engineering-type></col><col><mnemonic>start-thread</mnemonic><name>Start Thread</name><engineering-type>thread</engineering-type></col><col><mnemonic>end-thread</mnemonic><name>End Thread</name><engineering-type>thread</engineering-type></col><col><mnemonic>start-message</mnemonic><name>Start Message</name><engineering-type>os-log-metadata</engineering-type></col><col><mnemonic>end-message</mnemonic><name>End Message</name><engineering-type>os-log-metadata</engineering-type></col><col><mnemonic>start-backtrace</mnemonic><name>Start Backtrace</name><engineering-type>text-backtrace</engineering-type></col><col><mnemonic>end-backtrace</mnemonic><name>End Backtrace</name><engineering-type>text-backtrace</engineering-type></col><col><mnemonic>start-emit-location</mnemonic><name>Start Emit Location</name><engineering-type>return-location</engineering-type></col><col><mnemonic>end-emit-location</mnemonic><name>End Emit Location</name><engineering-type>return-location</engineering-type></col><col><mnemonic>signature</mnemonic><name>Signature</name><engineering-type>os-log-metadata</engineering-type></col></schema><row><start-time id="1" fmt="00:00.001.000">1000000</start-time><duration id="2" fmt="4.00 ms">4000000</duration><layout-id id="3" fmt="0">0</layout-id><string id="4" fmt="task">task</string><category id="5" fmt="tasks">tasks</category><subsystem id="6" fmt="com.cavewhere.tasks">com.cavewhere.tasks</subsystem><os-signpost-identifier id="7" fmt="0x1">1</os-signpost-identifier><process id="8" fmt="CaveWhere (100)"><pid id="9" fmt="100">100</pid><device-session id="10" fmt="TODO">TODO</device-session></process><process ref="8"/><thread id="11" fmt="Thread (0xb) (CaveWhere, pid: 100)"><tid id="12" fmt="0xb">11</tid><process ref="8"/></thread><thread ref="11"/><os-log-metadata id="13" fmt="ctx=lidar name=decode wait_us=250 gen=2"><string id="14" fmt="ctx=lidar name=decode wait_us=250 gen=2">ctx=lidar name=decode wait_us=250 gen=2</string></os-log-metadata><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/></row>
<row><start-time id="15" fmt="00:00.000.500">500000</start-time><duration id="16" fmt="10.00 ms">10000000</duration><layout-id ref="3"/><string id="17" fmt="batch">batch</string><category ref="5"/><subsystem ref="6"/><os-signpost-identifier id="18" fmt="0x2">2</os-signpost-identifier><process ref="8"/><process ref="8"/><thread id="19" fmt="Main Thread (0xa) (CaveWhere, pid: 100)"><tid id="20" fmt="0xa">10</tid><process ref="8"/></thread><thread ref="19"/><os-log-metadata id="21" fmt="ctx=scraps name=triangulate n=12 gen=1"><string id="22" fmt="ctx=scraps name=triangulate n=12 gen=1">ctx=scraps name=triangulate n=12 gen=1</string></os-log-metadata><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/></row>
<row><start-time id="23" fmt="00:00.006.000">6000000</start-time><duration ref="2"/><layout-id ref="3"/><string ref="4"/><category ref="5"/><subsystem ref="6"/><os-signpost-identifier id="24" fmt="0x3">3</os-signpost-identifier><process ref="8"/><process ref="8"/><thread ref="11"/><thread ref="11"/><os-log-metadata id="25" fmt="ctx= scraps  n= 0"><narrative-text id="26" fmt="ctx=">ctx=</narrative-text><string id="27" fmt="scraps">scraps</string><narrative-text id="28" fmt=" n="> n=</narrative-text><uint64 id="29" fmt="0">0</uint64></os-log-metadata><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/></row>
<row><start-time id="30" fmt="00:00.002.000">2000000</start-time><duration ref="2"/><layout-id ref="3"/><string id="31" fmt="frame">frame</string><category ref="5"/><subsystem id="32" fmt="com.cavewhere.render">com.cavewhere.render</subsystem><os-signpost-identifier id="33" fmt="0x4">4</os-signpost-identifier><process ref="8"/><process ref="8"/><thread ref="19"/><thread ref="19"/><os-log-metadata ref="13"/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/></row>
<row><start-time ref="30"/><duration ref="2"/><layout-id ref="3"/><string ref="4"/><category ref="5"/><subsystem ref="6"/><os-signpost-identifier id="34" fmt="0x5">5</os-signpost-identifier><process id="35" fmt="Other (200)"><pid id="36" fmt="200">200</pid><device-session ref="10"/></process><process ref="35"/><thread id="37" fmt="Thread (0x63) (Other, pid: 200)"><tid id="38" fmt="0x63">99</tid><process ref="35"/></thread><thread ref="37"/><os-log-metadata ref="13"/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/><sentinel/></row>
</node></trace-query-result>
"""

EVENT_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/data[1]/table[40]'><schema name="os-signpost"><col><mnemonic>time</mnemonic><name>Timestamp</name><engineering-type>event-time</engineering-type></col><col><mnemonic>thread</mnemonic><name>Thread</name><engineering-type>thread</engineering-type></col><col><mnemonic>process</mnemonic><name>Process</name><engineering-type>process</engineering-type></col><col><mnemonic>event-type</mnemonic><name>Event Type</name><engineering-type>event-type</engineering-type></col><col><mnemonic>scope</mnemonic><name>Scope</name><engineering-type>string</engineering-type></col><col><mnemonic>identifier</mnemonic><name>Signpost identifier</name><engineering-type>os-signpost-identifier</engineering-type></col><col><mnemonic>name</mnemonic><name>Name</name><engineering-type>signpost-name</engineering-type></col><col><mnemonic>format-string</mnemonic><name>Format String</name><engineering-type>format-string</engineering-type></col><col><mnemonic>backtrace</mnemonic><name>Backtrace</name><engineering-type>text-backtrace</engineering-type></col><col><mnemonic>subsystem</mnemonic><name>Subsystem</name><engineering-type>subsystem</engineering-type></col><col><mnemonic>category</mnemonic><name>Category</name><engineering-type>category</engineering-type></col><col><mnemonic>message</mnemonic><name>Message</name><engineering-type>os-log-metadata</engineering-type></col><col><mnemonic>emit-location</mnemonic><name>Emit Location</name><engineering-type>return-location</engineering-type></col></schema><row><event-time id="1" fmt="00:00.001.000">1000000</event-time><thread id="2" fmt="Thread (0xb) (CaveWhere, pid: 100)"><tid id="3" fmt="0xb">11</tid><process id="4" fmt="CaveWhere (100)"><pid id="5" fmt="100">100</pid><device-session id="6" fmt="TODO">TODO</device-session></process></thread><process ref="4"/><event-type id="7" fmt="Begin">Begin</event-type><string id="8" fmt="Process">Process</string><os-signpost-identifier id="9" fmt="0x1">1</os-signpost-identifier><signpost-name id="10" fmt="task">task</signpost-name><format-string id="11" fmt="%{public}s">%{public}s</format-string><sentinel/><subsystem id="12" fmt="com.cavewhere.tasks">com.cavewhere.tasks</subsystem><category id="13" fmt="tasks">tasks</category><os-log-metadata id="14" fmt="ctx=lidar name=decode wait_us=250 gen=2"><string id="15" fmt="ctx=lidar name=decode wait_us=250 gen=2">ctx=lidar name=decode wait_us=250 gen=2</string></os-log-metadata><sentinel/></row>
<row><event-time id="16" fmt="00:00.005.000">5000000</event-time><thread id="17" fmt="cwTaskTrace sampler (0xc) (CaveWhere, pid: 100)"><tid id="18" fmt="0xc">12</tid><process ref="4"/></thread><process ref="4"/><event-type id="19" fmt="Event">Event</event-type><string ref="8"/><os-signpost-identifier id="20" fmt="0xeeeeb0b5b2b2eee">1076122484758392558</os-signpost-identifier><signpost-name id="21" fmt="event">event</signpost-name><format-string ref="11"/><sentinel/><subsystem ref="12"/><category ref="13"/><os-log-metadata id="22" fmt="mem footprint=1048576 resident=2097152"><string id="23" fmt="mem footprint=1048576 resident=2097152">mem footprint=1048576 resident=2097152</string></os-log-metadata><sentinel/></row>
<row><event-time id="24" fmt="00:00.009.000">9000000</event-time><thread ref="2"/><process ref="4"/><event-type ref="19"/><string ref="8"/><os-signpost-identifier ref="20"/><signpost-name ref="21"/><format-string ref="11"/><sentinel/><subsystem ref="12"/><category ref="13"/><os-log-metadata id="25" fmt="dropped ctx=lidar gen=2"><string id="26" fmt="dropped ctx=lidar gen=2">dropped ctx=lidar gen=2</string></os-log-metadata><sentinel/></row>
<row><event-time id="27" fmt="00:00.010.000">10000000</event-time><thread id="28" fmt="Thread (0x63) (Other, pid: 200)"><tid id="29" fmt="0x63">99</tid><process id="30" fmt="Other (200)"><pid id="31" fmt="200">200</pid><device-session ref="6"/></process></thread><process ref="30"/><event-type ref="19"/><string ref="8"/><os-signpost-identifier ref="20"/><signpost-name ref="21"/><format-string ref="11"/><sentinel/><subsystem ref="12"/><category ref="13"/><os-log-metadata ref="22"/><sentinel/></row>
</node></trace-query-result>
"""

THREAD_STATE_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/data[1]/table[55]'><schema name="thread-state"><col><mnemonic>start</mnemonic><name>Start Time</name><engineering-type>start-time</engineering-type></col><col><mnemonic>thread</mnemonic><name>Thread</name><engineering-type>thread</engineering-type></col><col><mnemonic>state</mnemonic><name>State</name><engineering-type>thread-state</engineering-type></col><col><mnemonic>duration</mnemonic><name>Duration</name><engineering-type>duration</engineering-type></col><col><mnemonic>process</mnemonic><name>Process</name><engineering-type>process</engineering-type></col><col><mnemonic>core</mnemonic><name>Core</name><engineering-type>core</engineering-type></col><col><mnemonic>cputime</mnemonic><name>Running Time</name><engineering-type>duration-on-core</engineering-type></col><col><mnemonic>waittime</mnemonic><name>Wait Time</name><engineering-type>duration-waiting</engineering-type></col><col><mnemonic>priority</mnemonic><name>Priority</name><engineering-type>sched-priority</engineering-type></col><col><mnemonic>note</mnemonic><name>Note</name><engineering-type>narrative</engineering-type></col><col><mnemonic>summary</mnemonic><name>Summary</name><engineering-type>narrative</engineering-type></col></schema><row><start-time id="1" fmt="00:00.000.000">0</start-time><thread id="2" fmt="Thread (0x63) (Other, pid: 200)"><tid id="3" fmt="0x63">99</tid><process id="4" fmt="Other (200)"><pid id="5" fmt="200">200</pid><device-session id="6" fmt="TODO">TODO</device-session></process></thread><thread-state id="7" fmt="Running">Running</thread-state><duration id="8" fmt="10.00 ms">10000000</duration><process ref="4"/><sentinel/><sentinel/><sentinel/><sched-priority id="9" fmt="31">31</sched-priority><sentinel/><sentinel/></row>
<row><start-time ref="1"/><thread id="10" fmt="Thread (0xb) (CaveWhere, pid: 100)"><tid id="11" fmt="0xb">11</tid><process id="12" fmt="CaveWhere (100)"><pid id="13" fmt="100">100</pid><device-session ref="6"/></process></thread><thread-state ref="7"/><duration id="14" fmt="2.00 ms">2000000</duration><process ref="12"/><sentinel/><sentinel/><sentinel/><sched-priority ref="9"/><sentinel/><sentinel/></row>
<row><start-time id="15" fmt="00:00.002.000">2000000</start-time><thread ref="10"/><thread-state id="16" fmt="Runnable">Runnable</thread-state><duration id="17" fmt="1.00 ms">1000000</duration><process ref="12"/><sentinel/><sentinel/><sentinel/><sched-priority ref="9"/><sentinel/><sentinel/></row>
<row><start-time id="18" fmt="00:00.003.000">3000000</start-time><thread ref="10"/><thread-state ref="7"/><duration id="19" fmt="3.00 ms">3000000</duration><process ref="12"/><sentinel/><sentinel/><sentinel/><sched-priority ref="9"/><sentinel/><sentinel/></row>
<row><start-time id="20" fmt="00:00.006.000">6000000</start-time><thread ref="10"/><thread-state id="21" fmt="Preempted">Preempted</thread-state><duration ref="17"/><process ref="12"/><sentinel/><sentinel/><sentinel/><sched-priority ref="9"/><sentinel/><sentinel/></row>
</node></trace-query-result>
"""

TIME_PROFILE_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/data[1]/table[44]'><schema name="time-profile"><col><mnemonic>time</mnemonic><name>Sample Time</name><engineering-type>sample-time</engineering-type></col><col><mnemonic>thread</mnemonic><name>Thread</name><engineering-type>thread</engineering-type></col><col><mnemonic>process</mnemonic><name>Process</name><engineering-type>process</engineering-type></col><col><mnemonic>core</mnemonic><name>Core</name><engineering-type>core</engineering-type></col><col><mnemonic>thread-state</mnemonic><name>State</name><engineering-type>thread-state</engineering-type></col><col><mnemonic>weight</mnemonic><name>Weight</name><engineering-type>weight</engineering-type></col><col><mnemonic>stack</mnemonic><name>Backtrace</name><engineering-type>tagged-backtrace</engineering-type></col></schema><row><sample-time id="1" fmt="00:00.002.500">2500000</sample-time><thread id="2" fmt="Thread (0xb) (CaveWhere, pid: 100)"><tid id="3" fmt="0xb">11</tid><process id="4" fmt="CaveWhere (100)"><pid id="5" fmt="100">100</pid><device-session id="6" fmt="TODO">TODO</device-session></process></thread><process ref="4"/><core id="7" fmt="CPU 3 (P Core)">3</core><thread-state id="8" fmt="Running">Running</thread-state><weight id="9" fmt="1.00 ms">1000000</weight><tagged-backtrace id="10" fmt="decodeGltf"><backtrace id="11"><frame id="12" name="decodeGltf" addr="0x100"><binary id="13" name="cavewherelib" path="/x/cavewherelib"/></frame><frame id="14" name="cwConcurrent::run" addr="0x200"><binary ref="13"/></frame></backtrace><uint64 id="15" fmt="0">0</uint64></tagged-backtrace></row>
<row><sample-time id="16" fmt="00:00.003.500">3500000</sample-time><thread ref="2"/><process ref="4"/><core ref="7"/><thread-state ref="8"/><weight ref="9"/><tagged-backtrace id="17" fmt="morphVertices"><backtrace id="18"><frame id="19" name="morphVertices" addr="0x300"><binary ref="13"/></frame><frame ref="12"/><frame ref="14"/></backtrace><uint64 ref="15"/></tagged-backtrace></row>
<row><sample-time id="20" fmt="00:00.007.000">7000000</sample-time><thread ref="2"/><process ref="4"/><core ref="7"/><thread-state ref="8"/><weight ref="9"/><tagged-backtrace ref="10"/></row>
<row><sample-time id="21" fmt="00:00.004.000">4000000</sample-time><thread id="22" fmt="Thread (0x63) (Other, pid: 200)"><tid id="23" fmt="0x63">99</tid><process id="24" fmt="Other (200)"><pid id="25" fmt="200">200</pid><device-session ref="6"/></process></thread><process ref="24"/><core ref="7"/><thread-state ref="8"/><weight ref="9"/><tagged-backtrace ref="17"/></row>
</node></trace-query-result>
"""

TOC_XML = b"""<?xml version="1.0"?>
<trace-toc><run number="1"><info><target><device platform="macOS" model="Mac16,1" name="Studio" os-version="26.0 (25A354)" uuid="A1"/><process type="launched" return-exit-status="0" termination-reason="exit" name="CaveWhere" pid="4321"/></target><summary><start-date>2026-10-21T10:00:00Z</start-date></summary></info><processes><process name="kernel_task" pid="0" path="/kernel"/><process name="CaveWhere" pid="4321" path="/x/CaveWhere.app/Contents/MacOS/CaveWhere"/></processes><data><table schema="os-signpost-interval"/></data></run></trace-toc>
"""

TOC_WITHOUT_LAUNCH_XML = b"""<?xml version="1.0"?>
<trace-toc><run number="1"><info><target><device platform="macOS" name="Studio"/></target></info><processes><process name="CaveWhere" pid="4321"/></processes></run></trace-toc>
"""

STATISTICS_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/tracks[1]/track[1]/details[1]/detail[1]'><schema name="Statistics"><col><mnemonic>category</mnemonic><name>Category</name></col><col><mnemonic>persistent-bytes</mnemonic><name>Persistent Bytes</name></col><col><mnemonic>transient-count</mnemonic><name>Transient Count</name></col></schema><row><string id="1" fmt="All Heap Allocations">All Heap Allocations</string><size-in-bytes id="2" fmt="12.50 MiB">13107200</size-in-bytes><uint64 id="3" fmt="4">4</uint64></row>
<row><string id="4">Malloc 48.00 MiB</string><size-in-bytes ref="2"/><sentinel/></row>
</node></trace-query-result>
"""


def task(context, start_ns, end_ns, tid, **args):
    arguments = {"ctx": context}
    arguments.update({key: str(value) for key, value in args.items()})
    return tasks.Interval("task", start_ns, end_ns, tid, arguments)


def batch(context, start_ns, end_ns, tid):
    return tasks.Interval("batch", start_ns, end_ns, tid, {"ctx": context, "n": "4"})


class ArgumentTests(unittest.TestCase):
    def test_reads_single_string_message(self):
        self.assertEqual(
            tasks.parse_arguments("ctx=lidar name=decode wait_us=250 gen=2"),
            {"ctx": "lidar", "name": "decode", "wait_us": "250", "gen": "2"},
        )

    def test_reads_message_formatted_from_separate_arguments(self):
        # xctrace pads each formatted argument with spaces
        self.assertEqual(tasks.parse_arguments("ctx= scraps  n= 0"), {"ctx": "scraps", "n": "0"})

    def test_skips_leading_event_word(self):
        self.assertEqual(
            tasks.parse_arguments("mem footprint=10 resident=20"),
            {"footprint": "10", "resident": "20"},
        )


class ParseTests(unittest.TestCase):
    def test_signpost_intervals(self):
        intervals = tasks.parse_intervals(io.BytesIO(INTERVAL_XML), pid=APP_PID)
        intervals.sort(key=lambda interval: interval.start_ns)

        # The other subsystem's "frame" and pid 200's task are left out
        self.assertEqual(len(intervals), 3)

        scrapsBatch, lidarTask, scrapsTask = intervals
        self.assertEqual(scrapsBatch.kind, "batch")
        self.assertEqual(scrapsBatch.start_ns, 500_000)
        self.assertEqual(scrapsBatch.end_ns, 10_500_000)
        self.assertEqual(scrapsBatch.tid, 10)
        self.assertEqual(scrapsBatch.context, "scraps")
        self.assertEqual(scrapsBatch.args["n"], "12")

        self.assertEqual(lidarTask.kind, "task")
        self.assertEqual((lidarTask.start_ns, lidarTask.end_ns), (1 * MS, 5 * MS))
        self.assertEqual(lidarTask.tid, 11)
        self.assertEqual(lidarTask.context, "lidar")
        self.assertEqual(lidarTask.args["wait_us"], "250")
        self.assertEqual(lidarTask.args["gen"], "2")

        # Duration, name, thread and process all arrive as refs on this row
        self.assertEqual(scrapsTask.kind, "task")
        self.assertEqual((scrapsTask.start_ns, scrapsTask.end_ns), (6 * MS, 10 * MS))
        self.assertEqual(scrapsTask.tid, 11)
        self.assertEqual(scrapsTask.context, "scraps")

    def test_signpost_intervals_from_every_process_without_pid(self):
        intervals = tasks.parse_intervals(io.BytesIO(INTERVAL_XML))
        self.assertEqual(sorted(interval.tid for interval in intervals), [10, 11, 11, 99])

    def test_untagged_interval_reports_untagged(self):
        interval = tasks.Interval("task", 0, 1, 1, {"name": "run"})
        self.assertEqual(interval.context, "untagged")

    def test_signpost_events(self):
        events = tasks.parse_events(io.BytesIO(EVENT_XML), pid=APP_PID)

        # The Begin row is an interval edge, and pid 200's event is another process
        self.assertEqual([event.name for event in events], ["mem", "dropped"])

        memory, dropped = events
        self.assertEqual(memory.time_ns, 5 * MS)
        self.assertEqual(memory.tid, 12)
        self.assertEqual(memory.args, {"footprint": "1048576", "resident": "2097152"})
        self.assertEqual(dropped.time_ns, 9 * MS)
        self.assertEqual(dropped.tid, 11)
        self.assertEqual(dropped.args, {"ctx": "lidar", "gen": "2"})

    def test_thread_state(self):
        states = tasks.parse_thread_states(io.BytesIO(THREAD_STATE_XML), pid=APP_PID)

        self.assertEqual(
            [(state.start_ns, state.end_ns, state.tid, state.state) for state in states],
            [
                (0, 2 * MS, 11, "Running"),
                (2 * MS, 3 * MS, 11, "Runnable"),
                (3 * MS, 6 * MS, 11, "Running"),
                (6 * MS, 7 * MS, 11, "Preempted"),
            ],
        )

    def test_time_profile(self):
        samples = tasks.parse_samples(io.BytesIO(TIME_PROFILE_XML), pid=APP_PID)

        self.assertEqual([sample.time_ns for sample in samples], [2_500_000, 3_500_000, 7 * MS])
        self.assertTrue(all(sample.tid == 11 for sample in samples))
        self.assertTrue(all(sample.weight_ns == 1 * MS for sample in samples))

        # Leaf frame first; the second backtrace reuses two frames by ref, the
        # third sample reuses the whole first backtrace by ref
        self.assertEqual(samples[0].frames, ["decodeGltf", "cwConcurrent::run"])
        self.assertEqual(samples[1].frames, ["morphVertices", "decodeGltf", "cwConcurrent::run"])
        self.assertEqual(samples[2].frames, ["decodeGltf", "cwConcurrent::run"])


class JoinTests(unittest.TestCase):
    def setUp(self):
        self.lidar = task("lidar", 1 * MS, 5 * MS, 11)
        self.octree = task("octree", 2 * MS, 3 * MS, 11)
        self.otherThread = task("scraps", 1 * MS, 5 * MS, 12)
        # A batch spans submit to finish on the submitting thread; it covers no running work
        self.intervals = [batch("scraps", 0, 10 * MS, 11), self.lidar, self.octree, self.otherThread]

    def test_sample_belongs_to_covering_task_on_its_thread(self):
        samples = [
            tasks.Sample(1 * MS, 11, 1 * MS, []),          # start is inside
            tasks.Sample(2_500_000, 11, 1 * MS, []),       # nested task wins
            tasks.Sample(4 * MS, 11, 1 * MS, []),
            tasks.Sample(5 * MS, 11, 1 * MS, []),          # end is outside
            tasks.Sample(7 * MS, 11, 1 * MS, []),          # only the batch covers it
            tasks.Sample(500_000, 12, 1 * MS, []),         # before the task on that thread
            tasks.Sample(4 * MS, 13, 1 * MS, []),          # thread with no tasks
            tasks.Sample(4 * MS, 12, 1 * MS, []),
        ]

        assigned = tasks.assign_samples(samples, self.intervals)

        self.assertEqual(
            assigned,
            [self.lidar, self.octree, self.lidar, None, None, None, None, self.otherThread],
        )

    def test_thread_state_is_clipped_to_task_intervals(self):
        states = tasks.parse_thread_states(io.BytesIO(THREAD_STATE_XML), pid=APP_PID)

        byContext = tasks.state_time_by_context(states, [task("lidar", 1 * MS, 5 * MS, 11)])

        # Running 1-2 and 3-5 ms, Runnable 2-3 ms; the rest falls outside the task
        self.assertEqual(byContext, {"lidar": {"Running": 3 * MS, "Runnable": 1 * MS}})


class TocTests(unittest.TestCase):
    def test_launched_pid_comes_from_the_target(self):
        self.assertEqual(tasks.parse_launched_pid(io.BytesIO(TOC_XML)), 4321)

    def test_launched_pid_is_none_without_a_launched_process(self):
        self.assertIsNone(tasks.parse_launched_pid(io.BytesIO(TOC_WITHOUT_LAUNCH_XML)))


class DetailTests(unittest.TestCase):
    def test_statistics_rows_show_displayed_text(self):
        rows = tasks.parse_detail_rows(io.BytesIO(STATISTICS_XML))

        self.assertEqual(rows, [
            {"category": "All Heap Allocations", "persistent-bytes": "12.50 MiB", "transient-count": "4"},
            # A cell without fmt shows its text; a ref shows the fmt of the cell it names
            {"category": "Malloc 48.00 MiB", "persistent-bytes": "12.50 MiB", "transient-count": ""},
        ])


ATTRIBUTE_ROWS_XML = b"""<?xml version="1.0"?>
<trace-query-result>
<node xpath='//trace-toc[1]/run[1]/tracks[1]/track[1]/details[1]/detail[1]'><row category="All Heap Allocations" persistent-bytes="13107200" count-transient="4"/>
<row category="Malloc 48.00 MiB" persistent-bytes="0" count-transient="0"/>
</node></trace-query-result>
"""


class DetailAttributeRowTests(unittest.TestCase):
    def test_rows_written_as_attributes_are_read(self):
        rows = tasks.parse_detail_rows(io.BytesIO(ATTRIBUTE_ROWS_XML))

        self.assertEqual(rows, [
            {"category": "All Heap Allocations", "persistent-bytes": "13107200", "count-transient": "4"},
            {"category": "Malloc 48.00 MiB", "persistent-bytes": "0", "count-transient": "0"},
        ])


class ThreadPeakTests(unittest.TestCase):
    def test_peak_counts_threads_wanting_a_core_at_once(self):
        states = [
            tasks.ThreadState(0, 4 * MS, 1, "Running"),
            tasks.ThreadState(1 * MS, 3 * MS, 2, "Runnable"),
            tasks.ThreadState(2 * MS, 5 * MS, 3, "Preempted"),
            tasks.ThreadState(0, 10 * MS, 4, "Blocked"),
            # Starts as thread 1 stops, so the count stays at 3
            tasks.ThreadState(4 * MS, 6 * MS, 5, "Running"),
        ]

        self.assertEqual(tasks.peak_threads_wanting_core(states), (3, {1, 2, 3}))


def memory_event(time_ns, footprint, **args):
    arguments = {"footprint": str(footprint)}
    arguments.update({key: str(value) for key, value in args.items()})
    return tasks.Event(time_ns, 12, "mem", arguments)


class FootprintTests(unittest.TestCase):
    def test_peak_reports_budget_and_running_tasks(self):
        events = [
            memory_event(1 * MS, 100 * MB, available=900 * MB),
            memory_event(3 * MS, 300 * MB, available=700 * MB),
            memory_event(5 * MS, 200 * MB, available=800 * MB),
        ]
        intervals = [
            task("lidar", 2 * MS, 4 * MS, 11),
            task("scraps", 3 * MS, 6 * MS, 12),
            task("octree", 0, 3 * MS, 13),       # ends as the peak lands
            batch("git", 0, 10 * MS, 14),
        ]

        metrics, running = tasks.footprint_metrics(events, intervals)

        self.assertEqual(metrics, {"peak_footprint_mb": 300, "peak_time_s": 0.003,
                                   "available_at_peak_mb": 700})
        self.assertEqual(running, ["lidar", "scraps"])

    def test_peak_is_the_first_sample_on_the_plateau(self):
        # Freed large blocks keep the footprint up, so the first sample near the
        # top is when the blocks were live
        events = [
            memory_event(1 * MS, 100 * MB),
            memory_event(2 * MS, 290 * MB),
            memory_event(3 * MS, 300 * MB),
            memory_event(4 * MS, 295 * MB),
        ]

        metrics, _ = tasks.footprint_metrics(events, [])

        self.assertEqual(metrics["peak_footprint_mb"], 290)
        self.assertEqual(metrics["peak_time_s"], 0.002)

    def test_no_memory_events_reports_nothing(self):
        self.assertEqual(tasks.footprint_metrics([], [task("lidar", 0, 1, 1)]), ({}, []))

    def test_footprint_before_first_sample_is_unknown(self):
        events = [memory_event(2 * MS, 100 * MB), memory_event(4 * MS, 150 * MB)]
        memory = tasks.memory_events(events)

        self.assertIsNone(tasks.sample_at(memory, 1 * MS, "footprint"))
        self.assertEqual(tasks.sample_at(memory, 2 * MS, "footprint"), 100 * MB)
        self.assertEqual(tasks.sample_at(memory, 9 * MS, "footprint"), 150 * MB)


class AnalyzeTimingTests(unittest.TestCase):
    def setUp(self):
        self.intervals = tasks.parse_intervals(io.BytesIO(INTERVAL_XML), pid=APP_PID)
        # Starts before the sampler's first tick
        self.intervals.append(task("load", 100_000, 200_000, 13))
        self.events = tasks.parse_events(io.BytesIO(EVENT_XML), pid=APP_PID) + [
            memory_event(500_000, MB // 2),
            memory_event(7 * MS, 3 * MB),
            tasks.Event(2 * MS, 12, "queue", {"ctx": "lidar", "running": "3", "queued": "7"}),
            tasks.Event(4 * MS, 12, "queue", {"ctx": "lidar", "running": "1", "queued": "9"}),
            tasks.Event(12 * MS, 10, "idle", {}),
        ]
        self.states = tasks.parse_thread_states(io.BytesIO(THREAD_STATE_XML), pid=APP_PID)
        self.samples = tasks.parse_samples(io.BytesIO(TIME_PROFILE_XML), pid=APP_PID)

    def analyze(self, cores=8):
        return tasks.analyze_timing(self.intervals, self.events, self.states, self.samples, cores)

    def test_whole_run(self):
        metrics, details = self.analyze()

        self.assertAlmostEqual(metrics["time_to_idle_s"], 0.012)
        # The scraps batch ends later but only tasks count
        self.assertAlmostEqual(metrics["first_scraps_s"], 0.010)
        self.assertAlmostEqual(metrics["first_lidar_s"], 0.005)
        self.assertEqual(metrics["peak_footprint_mb"], 3)
        self.assertAlmostEqual(metrics["peak_time_s"], 0.007)
        self.assertEqual(details["running_at_peak"], ["scraps"])
        self.assertEqual(metrics["peak_threads_wanting_core"], 1)
        self.assertEqual(metrics["cores"], 8)
        self.assertAlmostEqual(metrics["cpu_s"], 0.003)

    def test_cpu_inside_dropped_generation(self):
        metrics, _ = self.analyze()

        # Both samples inside the gen=2 lidar task; the scraps sample has no dropped generation
        self.assertAlmostEqual(metrics["dropped_cpu_s"], 0.002)

    def test_per_context(self):
        metrics, details = self.analyze()

        self.assertEqual(metrics["lidar/tasks"], 1)
        self.assertAlmostEqual(metrics["lidar/cpu_s"], 0.002)
        self.assertAlmostEqual(metrics["lidar/span_s"], 0.004)
        self.assertEqual(metrics["lidar/peak_running"], 3)
        self.assertEqual(metrics["lidar/peak_queued"], 9)
        self.assertEqual(metrics["lidar/wait_p50_us"], 250)
        self.assertEqual(metrics["lidar/wait_p95_us"], 250)
        self.assertAlmostEqual(metrics["lidar/running_s"], 0.003)
        self.assertAlmostEqual(metrics["lidar/waiting_s"], 0.001)
        self.assertAlmostEqual(metrics["lidar/footprint_growth_mb"], 0.5)

        self.assertAlmostEqual(metrics["scraps/cpu_s"], 0.001)
        self.assertEqual(metrics["scraps/wait_p50_us"], 0)
        self.assertAlmostEqual(metrics["scraps/running_s"], 0.0)
        self.assertAlmostEqual(metrics["scraps/waiting_s"], 0.001)
        self.assertAlmostEqual(metrics["scraps/footprint_growth_mb"], 2.0)

        self.assertEqual(metrics["load/tasks"], 1)
        self.assertNotIn("load/footprint_growth_mb", metrics)

        self.assertEqual(sorted(details["top_stacks"]["lidar"]),
                         [("decodeGltf", 1 * MS), ("morphVertices", 1 * MS)])
        self.assertEqual(details["top_stacks"]["scraps"], [("decodeGltf", 1 * MS)])
        self.assertEqual(details["top_stacks"]["load"], [])

    def test_overlap_lists_each_pair_once(self):
        metrics, _ = self.analyze()

        self.assertEqual(sorted(name for name in metrics if name.startswith("overlap/")),
                         ["overlap/lidar+load", "overlap/lidar+scraps", "overlap/load+scraps"])
        self.assertEqual(metrics["overlap/lidar+scraps"], 0)

    def test_device_run_leaves_cores_out(self):
        metrics, _ = self.analyze(cores=None)

        self.assertNotIn("cores", metrics)


class HeaderTests(unittest.TestCase):
    def arguments(self, **values):
        defaults = {"device": None, "build": "build", "cold": False, "runs": 3}
        defaults.update(values)
        return tasks.argparse.Namespace(**defaults)

    def test_device_run_reports_unknown_os_and_cores(self):
        head = dict(tasks.header(self.arguments(device="Phone"), "marble.cwproj"))

        self.assertEqual(head["OS"], "unknown")
        self.assertEqual(head["Cores"], "unknown")
        self.assertEqual(head["Device"], "Phone")

    def test_bundle_always_opens_cold(self):
        self.assertEqual(tasks.cache_state(self.arguments(), "foo.cw"), "cold (bundle)")
        self.assertEqual(tasks.cache_state(self.arguments(), "marble.cwproj"), "warm")
        self.assertEqual(tasks.cache_state(self.arguments(cold=True), "marble.cwproj"), "cold")


class RedirtyTests(unittest.TestCase):
    def arguments(self, **values):
        defaults = {"device": None, "build": "build", "cold": False, "runs": 3, "page": "View"}
        defaults.update(values)
        return tasks.argparse.Namespace(**defaults)

    def test_app_arguments_pass_the_redirty_delay_through(self):
        command = tasks.app_arguments(self.arguments(redirty=1.5), "marble.cwproj")

        index = command.index("--profile-tasks-redirty")
        self.assertEqual(command[index + 1], "1.5")

    def test_app_arguments_leave_redirty_out_by_default(self):
        self.assertNotIn("--profile-tasks-redirty",
                         tasks.app_arguments(self.arguments(redirty=None), "marble.cwproj"))
        self.assertNotIn("--profile-tasks-redirty",
                         tasks.app_arguments(self.arguments(), "marble.cwproj"))

    def test_header_records_the_redirty_delay(self):
        head = dict(tasks.header(self.arguments(redirty=1.5), "marble.cwproj"))

        self.assertEqual(head["Redirty after s"], "1.5")

    def test_header_reports_none_without_a_redirty(self):
        self.assertEqual(dict(tasks.header(self.arguments(), "marble.cwproj"))["Redirty after s"], "none")

    def test_redirty_option_reads_seconds(self):
        with unittest.mock.patch.object(tasks, "profile") as profile:
            status = tasks.main(["--build", "build", "--project", "marble.cwproj",
                                 "--out", "out", "--redirty", "2.5"])

        self.assertEqual(status, 0)
        self.assertEqual(profile.call_args.args[0].redirty, 2.5)


class QueueWaitTests(unittest.TestCase):
    def test_percentiles_per_context(self):
        waits = [100 * index for index in range(21)]
        intervals = [task("lidar", 0, 1, 1, wait_us=wait) for wait in reversed(waits)]
        intervals.append(task("scraps", 0, 1, 2, wait_us=700))
        intervals.append(batch("lidar", 0, 1, 3))

        percentiles = tasks.queue_wait_percentiles(intervals)

        self.assertEqual(set(percentiles), {"lidar", "scraps"})
        self.assertEqual(percentiles["lidar"]["p50"], 1000)
        self.assertEqual(percentiles["lidar"]["p95"], 1900)
        self.assertEqual(percentiles["scraps"]["p50"], 700)
        self.assertEqual(percentiles["scraps"]["p95"], 700)


class OverlapTests(unittest.TestCase):
    def test_overlap_counts_seconds_both_contexts_run(self):
        intervals = [
            task("lidar", 0, 4 * SECOND, 1),
            task("lidar", 2 * SECOND, 6 * SECOND, 2),
            task("scraps", 3 * SECOND, 8 * SECOND, 3),
            task("octree", 10 * SECOND, 11 * SECOND, 4),
            batch("scraps", 0, 20 * SECOND, 5),
        ]

        matrix = tasks.overlap_matrix(intervals)

        # Two lidar tasks running together during 3-4 s still count as one second of lidar
        self.assertAlmostEqual(matrix[("lidar", "scraps")], 3.0)
        self.assertAlmostEqual(matrix[("scraps", "lidar")], 3.0)
        self.assertAlmostEqual(matrix[("lidar", "octree")], 0.0)
        self.assertAlmostEqual(matrix[("octree", "scraps")], 0.0)


class SpreadTests(unittest.TestCase):
    def test_median_and_spread(self):
        self.assertEqual(tasks.median_spread([3, 1, 2]), (2, 2))
        self.assertEqual(tasks.median_spread([1, 2, 3, 10]), (2.5, 9))
        self.assertEqual(tasks.median_spread([5.0]), (5.0, 0.0))


BASELINE_METRICS = {
    "time_to_idle_s": [10, 11, 12],
    "peak_footprint_mb": [500, 520, 510],
    "cpu_s": [30, 31, 32],
    "only_in_baseline": [1, 2, 3],
}

AFTER_METRICS = {
    "time_to_idle_s": [7, 8, 9],
    "peak_footprint_mb": [505, 515, 525],
    "cpu_s": [33, 33, 33],
    "only_in_after": [4, 5, 6],
}


class CompareTests(unittest.TestCase):
    def test_reports_only_deltas_that_clear_the_spread(self):
        deltas = tasks.compare_metrics(BASELINE_METRICS, AFTER_METRICS)

        # peak_footprint_mb moves 5 inside a spread of 20; cpu_s moves exactly its spread of 2
        self.assertEqual([delta.name for delta in deltas], ["time_to_idle_s"])
        self.assertEqual(deltas[0].baseline, 11)
        self.assertEqual(deltas[0].after, 8)
        self.assertEqual(deltas[0].delta, -3)

    def test_compare_option_reads_two_output_folders(self):
        with tempfile.TemporaryDirectory() as root:
            baseline = Path(root) / "baseline"
            after = Path(root) / "after"
            baseline.mkdir()
            after.mkdir()
            tasks.write_metrics(baseline, BASELINE_METRICS)
            tasks.write_metrics(after, AFTER_METRICS)
            self.assertEqual(tasks.load_metrics(baseline), BASELINE_METRICS)

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = tasks.main(["--compare", str(baseline), str(after)])

        self.assertEqual(status, 0)
        self.assertIn("time_to_idle_s", output.getvalue())
        self.assertNotIn("peak_footprint_mb", output.getvalue())
        self.assertNotIn("cpu_s", output.getvalue())


class GpuTests(unittest.TestCase):
    """The sampler's mem event carries gpu= on Apple platforms and leaves it out elsewhere."""

    def gpu_events(self):
        return [
            memory_event(1 * MS, 100 * MB, gpu=10 * MB),
            memory_event(5 * MS, 300 * MB, gpu=50 * MB),
            memory_event(7 * MS, 200 * MB, gpu=60 * MB),
        ]

    def plain_events(self):
        return [memory_event(1 * MS, 100 * MB), memory_event(5 * MS, 300 * MB)]

    def intervals(self):
        return [task("lidar", 2 * MS, 6 * MS, 11)]

    def report(self, timing, memory):
        return tasks.project_report([("Project", "marble.cwproj")], tasks.collect([timing]), [],
                                    tasks.collect([memory]), [], [], [])

    def test_reads_gpu_argument(self):
        self.assertEqual(
            tasks.parse_arguments("mem footprint=10 resident=20 gpu=30"),
            {"footprint": "10", "resident": "20", "gpu": "30"},
        )

    def test_peak_reports_largest_gpu_and_gpu_at_footprint_peak(self):
        metrics, _ = tasks.footprint_metrics(self.gpu_events(), [])

        self.assertEqual(metrics["peak_footprint_mb"], 300)
        self.assertEqual(metrics["peak_gpu_mb"], 60)
        self.assertEqual(metrics["gpu_at_peak_mb"], 50)

    def test_peak_without_gpu_leaves_gpu_out(self):
        metrics, _ = tasks.footprint_metrics(self.plain_events(), [])

        self.assertNotIn("peak_gpu_mb", metrics)
        self.assertNotIn("gpu_at_peak_mb", metrics)

    def test_context_reports_gpu_growth(self):
        metrics, _ = tasks.analyze_timing(self.intervals(), self.gpu_events(), [], [], 8)

        self.assertAlmostEqual(metrics["lidar/footprint_growth_mb"], 200)
        self.assertAlmostEqual(metrics["lidar/gpu_growth_mb"], 40)

    def test_context_without_gpu_leaves_gpu_growth_out(self):
        metrics, _ = tasks.analyze_timing(self.intervals(), self.plain_events(), [], [], 8)

        self.assertAlmostEqual(metrics["lidar/footprint_growth_mb"], 200)
        self.assertNotIn("lidar/gpu_growth_mb", metrics)

    def test_report_shows_gpu_rows_and_column(self):
        timing, _ = tasks.analyze_timing(self.intervals(), self.gpu_events(), [], [], 8)
        memory, _ = tasks.footprint_metrics(self.gpu_events(), self.intervals())

        report = self.report(timing, memory)

        self.assertIn("| Peak GPU MB | 60 |", report)
        self.assertIn("| GPU at footprint peak MB | 50 |", report)
        self.assertIn("GPU growth MB", report)
        lidar_row = next(line for line in report.splitlines() if line.startswith("| lidar |"))
        self.assertTrue(lidar_row.rstrip().endswith("| 40 |"), lidar_row)

    def test_report_without_gpu_leaves_gpu_out(self):
        timing, _ = tasks.analyze_timing(self.intervals(), self.plain_events(), [], [], 8)
        memory, _ = tasks.footprint_metrics(self.plain_events(), self.intervals())

        report = self.report(timing, memory)

        self.assertNotIn("GPU", report)
        self.assertIn("Footprint growth MB", report)


class ChargedTests(unittest.TestCase):
    """A limited() launcher's task carries charged_bytes=, the weight the limiter charged it."""

    def intervals(self, scale=1):
        return [
            task("scraps", 2 * MS, 4 * MS, 11, charged_bytes=128 * MB * scale),
            task("scraps", 4 * MS, 6 * MS, 11, charged_bytes=64 * MB * scale),
            task("lidar", 2 * MS, 6 * MS, 12),
        ]

    def test_reads_charged_argument(self):
        self.assertEqual(tasks.parse_arguments("ctx=scraps charged_bytes=30")[tasks.CHARGED_BYTES_ARG], "30")

    def test_context_sums_its_charged_weights(self):
        metrics, _ = tasks.analyze_timing(self.intervals(), [], [], [], 8)

        self.assertEqual(metrics["scraps/charged_mb"], 192)

    def test_context_without_charges_reports_zero(self):
        metrics, _ = tasks.analyze_timing(self.intervals(), [], [], [], 8)

        self.assertEqual(metrics["lidar/charged_mb"], 0)

    def test_report_shows_the_median_beside_footprint_growth(self):
        runs = [tasks.analyze_timing(self.intervals(scale), [], [], [], 8)[0] for scale in (1, 2, 2)]
        report = tasks.project_report([("Project", "synthetic.cwproj")], tasks.collect(runs), [], {}, [], [], [])

        lines = report.splitlines()
        columns = [cell.strip() for cell in next(line for line in lines
                                                 if line.startswith("| Context |")).strip("|").split("|")]
        self.assertEqual(columns.index("Charged MB"), columns.index("Footprint growth MB") + 1)
        scraps = [cell.strip() for cell in next(line for line in lines
                                                if line.startswith("| scraps |")).strip("|").split("|")]
        self.assertEqual(scraps[columns.index("Charged MB")], "384 ± 192")


if __name__ == "__main__":
    unittest.main()

import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId

    // Narrow, like the real sidebar, so a long name must wrap.
    width: 80
    height: 400

    ListModel {
        id: testModelId

        ListElement {
            nameRole: "Short"
            progressRole: 42
            numberOfStepsRole: 100
        }
        ListElement {
            nameRole: "Importing Compass data — Fisher Ridge System / Main Trunk 2019.dat"
            progressRole: 10
            numberOfStepsRole: 100
        }
    }

    TaskListView {
        id: taskListViewId

        anchors.fill: parent
        model: testModelId
    }

    CWTestCase {
        name: "TaskListView"
        when: windowShown

        // Names wrap instead of eliding, so a long name makes its row taller
        // than a short one. If the name elided, both rows would be a single
        // line and the same height.
        function test_longNameWrapsTaller() {
            tryCompare(taskListViewId, "count", 2);
            tryVerify(function() {
                return taskListViewId.itemAtIndex(0) !== null
                    && taskListViewId.itemAtIndex(1) !== null;
            });

            let shortRow = taskListViewId.itemAtIndex(0);
            let longRow = taskListViewId.itemAtIndex(1);

            // Poll: the wrapped height comes from the layout's polish pass, which
            // may not have settled the instant the delegates become non-null.
            tryVerify(function() { return longRow.height > shortRow.height; }, 5000,
                      "a long, wrapped name should make its row taller than a short one");
        }

        // Digit grouping is inserted manually (the app's C locale does no
        // grouping), so these are deterministic regardless of the test locale.
        function test_formatCount_data() {
            return [
                { tag: "zero",      value: 0,       expected: "0" },
                { tag: "twoDigit",  value: 42,      expected: "42" },
                { tag: "threeDigit",value: 100,     expected: "100" },
                { tag: "thousand",  value: 1000,    expected: "1,000" },
                { tag: "sixDigit",  value: 123456,  expected: "123,456" },
                { tag: "sevenDigit",value: 1234567, expected: "1,234,567" },
                { tag: "millions",  value: 2000000, expected: "2,000,000" },
            ];
        }

        function test_formatCount(data) {
            tryVerify(function() { return taskListViewId.itemAtIndex(0) !== null; });
            let row = taskListViewId.itemAtIndex(0);
            compare(row.formatCount(data.value), data.expected);
        }
    }
}

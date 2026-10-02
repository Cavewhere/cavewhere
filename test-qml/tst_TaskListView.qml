import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId

    // Narrow, like the real sidebar, so a long name must wrap.
    width: 80
    // Tall enough that every row below has a delegate; a row off the view
    // never gets built and nothing can be read off it.
    height: 900

    ListModel {
        id: testModelId

        ListElement {
            nameRole: "Short"
            progressRole: 42
            numberOfStepsRole: 100
            detailNameRole: ""
            detailProgressRole: 0
            detailTotalRole: 0
            treeBackedRole: false
        }
        ListElement {
            nameRole: "Importing Compass data — Fisher Ridge System / Main Trunk 2019.dat"
            progressRole: 10
            numberOfStepsRole: 100
            detailNameRole: ""
            detailProgressRole: 0
            detailTotalRole: 0
            treeBackedRole: false
        }
        // A tree-backed run whose deepest working leaf counts what it is doing
        ListElement {
            nameRole: "Updating Scraps"
            progressRole: 500
            numberOfStepsRole: 1000
            detailNameRole: "Morphing"
            detailProgressRole: 12400
            detailTotalRole: 301000
            treeBackedRole: true
        }
        // ...and one whose leaf can't say, so the mini-bar pulses instead
        ListElement {
            nameRole: "Triangulating LiDAR notes"
            progressRole: 250
            numberOfStepsRole: 1000
            detailNameRole: "Compressing texture"
            detailProgressRole: 0
            detailTotalRole: 0
            treeBackedRole: true
        }
        // A tree-backed run between two leaves: nothing has lived long enough
        // to name, so the detail line waits while the row keeps its percent.
        ListElement {
            nameRole: "Updating Scraps"
            progressRole: 300
            numberOfStepsRole: 1000
            detailNameRole: ""
            detailProgressRole: 0
            detailTotalRole: 0
            treeBackedRole: true
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
            tryCompare(taskListViewId, "count", testModelId.count);
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

        // The delegate's parts are reached by name: a test can't see the ids
        // inside a component.
        function findByName(item, objectName) {
            if (item.objectName === objectName) {
                return item;
            }
            for (let i = 0; i < item.children.length; i++) {
                let found = findByName(item.children[i], objectName);
                if (found !== null) {
                    return found;
                }
            }
            return null;
        }

        function rowAt(index) {
            tryVerify(function() { return taskListViewId.itemAtIndex(index) !== null; });
            return taskListViewId.itemAtIndex(index);
        }

        // A row with no tree behind it says nothing about a detail, so the
        // second bar and its caption stay off screen.
        function test_rowWithoutDetailHidesTheDetailLine() {
            let row = rowAt(0);
            tryCompare(findByName(row, "taskDetailLabel"), "visible", false);
            tryCompare(findByName(row, "taskDetailProgressBar"), "visible", false);
        }

        // ...and it keeps the plain "n / m" caption, since its steps are real.
        function test_rowWithoutDetailCountsItsSteps() {
            let row = rowAt(0);
            tryCompare(findByName(row, "taskProgressCaption"), "text", "42 / 100");
        }

        function test_countedDetailNamesTheLeafAndItsCounts() {
            let row = rowAt(2);
            tryCompare(findByName(row, "taskDetailLabel"), "visible", true);
            tryCompare(findByName(row, "taskDetailLabel"), "text", "Morphing 12,400 / 301,000");

            let bar = findByName(row, "taskDetailProgressBar");
            tryCompare(bar, "visible", true);
            tryCompare(bar, "indeterminate", false);
            fuzzyCompare(bar.value, 12400 / 301000, 0.0001);
        }

        // Promise units mean nothing to a person, so a tree-backed row reads
        // as a percent and leaves the real counts to the detail line.
        function test_treeBackedRowShowsAPercent() {
            let row = rowAt(2);
            tryCompare(findByName(row, "taskProgressCaption"), "text", "50%");
        }

        // The percent follows the tree, not the detail line: promise units must
        // never reach the row, even while no leaf qualifies for the line.
        function test_treeBackedRowWithoutADetailStillShowsAPercent() {
            let row = rowAt(4);
            tryCompare(findByName(row, "taskProgressCaption"), "text", "30%");
            tryCompare(findByName(row, "taskDetailLabel"), "visible", false);
            tryCompare(findByName(row, "taskDetailProgressBar"), "visible", false);
        }

        function test_opaqueDetailPulsesWithoutCounts() {
            let row = rowAt(3);
            tryCompare(findByName(row, "taskDetailLabel"), "visible", true);
            tryCompare(findByName(row, "taskDetailLabel"), "text", "Compressing texture");

            let bar = findByName(row, "taskDetailProgressBar");
            tryCompare(bar, "visible", true);
            tryCompare(bar, "indeterminate", true);
        }

        function test_formatCount(data) {
            tryVerify(function() { return taskListViewId.itemAtIndex(0) !== null; });
            let row = taskListViewId.itemAtIndex(0);
            compare(row.formatCount(data.value), data.expected);
        }
    }
}

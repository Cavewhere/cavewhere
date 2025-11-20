import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {

    width: 600
    height: 300

    Item {
        id: container
        width: 300
        height: 300

        Rectangle {
            id: rectId
            x: 100
            y: 100
            width: 10
            height: 10
            color: "red"
        }

        QuoteBox {
            id: quoteBoxId
            pointAtObject: rectId
            pointAtObjectPosition: Qt.point(rectId.width * 0.5, rectId.height)

            Rectangle {
                color: "green"
                width: 100
                height: 20
            }

            // Text {
            //     text: "I'm a QuoteBox"
            // }
        }

    }

    Image {
        id: testImageId
        width: 300
        height: 300
        anchors.left: container.right
        source: "qrc:/datasets/tst_QuoteBox/testQuoteBox.png"
    }

    TestCase {
        name: "QuoteBox"
        when: windowShown

        //This test is very fragile on different monitors
        // function test_drawnCorrectly() {

        //     let containerImg = grabImage(container);
        //     let testImage = grabImage(testImageId)

        //     //Verifies that it looks the same
        //     verify(containerImg.equals(testImage));

        //     //This can be used to update the testcase
        //     // containerImg.save("testQuoteBox.png")
        //     // wait(1000000);
        // }
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import cavewherelib
import QmlTestRecorder
import QtTest
import QtCore
import "qrc:/qml"

MainWindowTest {
    id: rootId

    Loader {
        id: welcomePageLoader
        anchors.fill: parent
        sourceComponent: WelcomePage {
            // id: welcomePage
            objectName: "welcome"
            anchors.fill: parent
            account: RootData.account
        }
    }

    TestCase {
        name: "WelcomePage"
        when: windowShown

        function initTestCase() {
        }

        function init() {
            // MapWhere.clearApplicationSettings();
            RootData.account.name = ""
            RootData.account.email = ""

            // welcomePageLoader.sourceComponent = null
            // welcomePageLoader.sourceComponent = welcomeComponent
            welcomePageLoader.active = false
            welcomePageLoader.active = true


            rootId.mainWindow.visible = false;
            // welcomePage.update();

            //Verify Given
            // verify(RootData.showWelcomeScreen);
            // verify(RootData.undoStack.index == 0);
            verify(RootData.account.name == "");
            verify(RootData.account.email == "");

            // if(!(RootData.pageModel.currentItem instanceof WelcomePage)) {
            //     RootData.pageModel.currentItem.gotoNextPage(PageModel.WelcomePage, StackPage.NextPageBehaviour.Replace);
            // }

            // let obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->PersonNameTextEdit")
            // let obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            // obj1.ignoreErrorUntilNextFocus = true
            // obj2.ignoreErrorUntilNextFocus = true

            personNameSpy.clear();
            emailSpy.clear();
        }

        function cleanup() {

        }

        /**
          Given
            1. That the welcome page is shown
            2. The QSettings are clear
            3. No areas exist
          When
            1. User clicks and enters "Philip" in the name field
            2. User clicks and enters "test@gmail.com in the valid email field
          Then
            1. Next button is enabled
            2. No errors are shown
            3. Model should be updated
          When
            A1. User press Next button
          Then
            A1. Next page should be Area's page
          */

        Settings {
            id: settings
            category: "account" //private settings values, could change
        }

        SignalSpy {
            id: personNameSpy
            signalName: "hasErrorChanged"
        }

        SignalSpy {
            id: emailSpy
            signalName: "hasErrorChanged"
        }

        function test_validInfo() {
            //When 1. User clicks and enters "Philip" in the name field
            let obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->PersonNameTextEdit")
            personNameSpy.target = obj1
            mouseClick(obj1)
            waitForRendering(obj1)

            keyClick("P")
            keyClick("h")
            keyClick("i")
            keyClick("l")
            keyClick("i")
            keyClick("p")

            //When 2. User clicks and enters "test@gmail.com in the valid email field
            let obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            emailSpy.target = obj2
            mouseClick(obj2)
            waitForRendering(obj2)

            keyClick("t")
            keyClick("e")
            keyClick("s")
            keyClick("t")
            keyClick(64, 33554432) //@, Shift+
            keyClick("g")
            keyClick("m")
            keyClick("a")
            keyClick("i")
            keyClick("l")
            keyClick(46, 0) //.
            keyClick("c")
            keyClick("o")
            keyClick("m")

            // wait(100000);

            //Then 2. Next button is enabled
            let nextButton = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->nextButton")
            verify(obj1.hasError == false, `obj.hasError:${obj1.hasError}`);
            verify(obj2.hasError == false);
            verify(personNameSpy.count == 0); //No error should be shown to the user
            verify(emailSpy.count == 0); //No error should be shown to the user

            //When A1. User press Next button
            mouseClick(nextButton)

            //Then 3. Model should be updated
            compare(RootData.account.name, "Philip")
            compare(RootData.account.email, "test@gmail.com")
            compare(settings.value("name", ""), "Philip");
            compare(settings.value("email", ""), "test@gmail.com");

            wait(100);
        }

        /**
          Given
            1. That the welcome page is shown
            2. The QSettings are clear
            3. No areas exist
          When
            1. User clicks on name TextField
            2. User Clicks on email TextField
          Then
            1. Next button should be disabled
            2. Name field should error
            3. Email field should be error free

          When
            A1. User clicks back to name

          Then
            A1. Next button is still disabled
            A2. Name field should error
            A3. Email field should error
          */
        function test_invalidInfo() {
            // RootData.account.name = ""
            // RootData.account.email = ""

            let nextButton = ObjectFinder.findObjectByChain(mainWindow, "rootParent->rootId->rootStackView->WelcomePage->NextButton")

            //When 1. User clicks on name TextField
            let obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->PersonNameTextEdit")
            mouseClick(obj1)
            waitForRendering(obj1)

            //When 2. User Clicks on email TextField
            let obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            mouseClick(obj2)
            waitForRendering(obj2)

            //Then
            //1. Next button should be disabled
            verify(RootData.account.name == "");
            verify(RootData.account.email == "");

            //2. Name field should error
            verify(obj1.hasError);

            //3. Email field should be error free
            verify(!obj2.hasError);

            //When A1. User clicks back to name
            mouseClick(obj1)
            waitForRendering(obj1)

            //Then
            //1. Next button should be disabled
            verify(RootData.account.name == "");
            verify(RootData.account.email == "");

            //2. Name field should error
            verify(obj1.hasError);

            //3. Email field should be error free
            verify(obj2.hasError);
        }

        /**
          Given
            1. That the welcome page is shown
            2. The QSettings are clear
            3. No areas exist
          When
            1. User Clicks on name TextField
            2. User Type "sauce"
            3. User Clicks on email TextField
            4. User Types "test" (invalid email)
            5. User Clicks on next button
          Then
            1. Name field shouldn't have an error
            2. Email should have invalid error

          When
            1. User Clicks on email
            2. User types "@gmail.com
            3. User Clicks next button

          Then
            1. Page should switch to Areas
          */
        function test_fixupBadEmail() {
            // RootData.account.name = ""
            // RootData.account.email = ""

            verify(RootData.account.name === "")
            verify(RootData.account.email === "")

            //When 1. User clicks on name TextField
            let obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->PersonNameTextEdit")
            console.log("obj1:" + obj1)

            mouseClick(obj1)
            waitForRendering(obj1)

            //When 2. User Type "sauce"
            keyClick("s")
            keyClick("a")
            keyClick("u")
            keyClick("c")
            keyClick("e")

            //When 2. User Clicks on email TextField
            let obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            mouseClick(obj2)
            waitForRendering(obj2)

            //When 4. User Types "test" (invalid email)
            keyClick("t")
            keyClick("e")
            keyClick("s")
            keyClick("t")

            //When 5. User Clicks on next button
            let nextButton = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->nextButton->label")
            mouseClick(nextButton)
            wait(300);

            //Then 1. Name field shouldn't have an error
            verify(!obj1.hasError)

            //Then 2. Email field should have an error
            verify(obj2.hasError)

            //Then. RootData hasn't changed
            verify(RootData.account.name === "")
            verify(RootData.account.email === "")

            //When 2. User Clicks on email TextField
            obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            mouseClick(obj2)
            waitForRendering(obj2)

            //When 2. User types "@gmail.com
            keyClick(64, 33554432) //@, Shift+
            keyClick("g")
            keyClick("m")
            keyClick("a")
            keyClick("i")
            keyClick("l")
            keyClick(46, 0) //.
            keyClick("c")
            keyClick("o")
            keyClick("m")

            //When 3. User Clicks next button
            mouseClick(nextButton)
            // verify(nextButton.enabled == false)

            verify(RootData.account.name === "sauce")
            verify(RootData.account.email === "test@gmail.com")
        }


        /**
          Given
            1. That the welcome page is shown
            2. The QSettings are clear
            3. No areas exist
          When
            1. User clicks on the next button
          Then
            1. Name field should have an error
            2. Email field should have an error
          */
        function test_showErrorOnBlockedNext() {
            //Sanity check
            let obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->PersonNameTextEdit")
            let obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->PersonEdit->EmailTextEdit")
            verify(!obj1.hasError)
            verify(!obj2.hasError)

            //When 5. User Clicks on next button
            let nextButton = ObjectFinder.findObjectByChain(rootId, "rootId->welcome->nextButton->label")
            mouseClick(nextButton)
            wait(300);

            //Then 1. Name field should have an error
            verify(obj1.hasError)

            //Then 2. Email field should have an error
            verify(obj2.hasError)

            compare(RootData.account.name, "");
            compare(RootData.account.email, "");
        }
    }
}


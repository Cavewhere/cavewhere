import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 300
    height: 300

    Scene {
        id: sceneId
    }

    EDLSettingsPanel {
        id: panelId
        objectName: "edlSettingsPanel"
        scene: sceneId
        anchors.fill: parent
    }

    TestCase {
        name: "EDLSettingsPanel"
        when: windowShown

        function test_slidersReflectSceneDefaults() {
            const strength = findChild(panelId, "edlStrengthSlider")
            const maxDarken = findChild(panelId, "edlMaxDarkenSlider")
            const radius = findChild(panelId, "edlRadiusSlider")

            verify(strength !== null, "strength slider should exist")
            verify(maxDarken !== null, "max darken slider should exist")
            verify(radius !== null, "radius slider should exist")

            compare(strength.value, sceneId.edl.strength)
            compare(maxDarken.value, sceneId.edl.maxDarken)
            compare(radius.value, sceneId.edl.radius)
        }

        function test_sceneChangePropagatesToSlider() {
            const strength = findChild(panelId, "edlStrengthSlider")
            sceneId.edl.strength = 2500
            tryCompare(strength, "value", 2500)
        }

        function test_movingSliderUpdatesScene() {
            const maxDarken = findChild(panelId, "edlMaxDarkenSlider")
            maxDarken.moved(12.5)
            compare(sceneId.edl.maxDarken, 12.5)

            const radius = findChild(panelId, "edlRadiusSlider")
            radius.moved(4.0)
            compare(sceneId.edl.radius, 4.0)
        }
    }
}

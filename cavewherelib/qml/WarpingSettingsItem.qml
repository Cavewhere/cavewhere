import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root

    property ScrapManager scrapManager: RootData.scrapManager
    property var warpingSettings: scrapManager ? scrapManager.warpingSettings : null

    function parseDistance(text) {
        var value = Number(text);
        if (isNaN(value)) {
            return null;
        }
        return value;
    }

    QC.GroupBox {
        title: "Warping and Morphing"
        Layout.fillWidth: true

        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true

            RowLayout {
                spacing: 6
                Layout.fillWidth: true

                InformationButton { showItemOnClick: gridResolutionHelp }

                Text { text: "Grid resolution (m)" }

                ClickTextInput {
                    Layout.fillWidth: true
                    validator: DistanceValidator {}
                    text: root.warpingSettings ? root.warpingSettings.gridResolutionMeters.toFixed(2) : ""
                    onFinishedEditting: (newText) => {
                        if(!root.warpingSettings) return;
                        const value = root.parseDistance(newText);
                        if(value === null || value <= 0.0) return;
                        root.warpingSettings.gridResolutionMeters = value
                    }
                }
            }

            HelpArea {
                id: gridResolutionHelp
                Layout.fillWidth: true
                text: "Sets the spacing of the triangulation grid on each scrap. Smaller values increase detail but add more triangles."
            }

            CheckableGroupBox {
                id: shotSpacingBox
                Layout.fillWidth: true
                checked: root.warpingSettings ? root.warpingSettings.useShotInterpolationSpacing : false
                text: "Shot interpolation spacing (m)"
                contentsVisible: checked

                onCheckedChanged: {
                    if(root.warpingSettings) {
                        root.warpingSettings.useShotInterpolationSpacing = checked;
                    }
                }

                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    InformationButton { showItemOnClick: shotSpacingHelp }

                    ClickTextInput {
                        Layout.fillWidth: true
                        validator: DistanceValidator {}
                        enabled: shotSpacingBox.checked
                        text: root.warpingSettings ? root.warpingSettings.shotInterpolationSpacingMeters.toFixed(2) : ""
                        onFinishedEditting: (newText) => {
                            if(!root.warpingSettings) return;
                            const value = root.parseDistance(newText);
                            if(value === null || value <= 0.0) return;
                            root.warpingSettings.shotInterpolationSpacingMeters = value;
                        }
                    }
                }
            }

            HelpArea {
                id: shotSpacingHelp
                Layout.fillWidth: true
                text: "Controls how densely dummy stations are inserted along survey shots for morphing. Smaller spacing gives smoother warps but costs performance."
            }

            CheckableGroupBox {
                id: closestStationsBox
                Layout.fillWidth: true
                checked: root.warpingSettings ? root.warpingSettings.useMaxClosestStations : false
                text: "Max closest stations"
                contentsVisible: checked

                onCheckedChanged: {
                    if(root.warpingSettings) {
                        root.warpingSettings.useMaxClosestStations = checked;
                    }
                }

                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    InformationButton { showItemOnClick: closestStationsHelp }

                    ClickNumberInput {
                        Layout.fillWidth: true
                        text: root.warpingSettings ? root.warpingSettings.maxClosestStations : 1
                        enabled: closestStationsBox.checked
                        onFinishedEditting: (newText) => {
                                                if(root.warpingSettings && Number(newText) >= 1) {
                                                    root.warpingSettings.maxClosestStations = newText
                                                }
                        }
                    }
                }
            }

            HelpArea {
                id: closestStationsHelp
                Layout.fillWidth: true
                text: "Number of nearby stations considered when warping texture points in plan view. More stations can stabilize the warp but may flatten sharp features."
            }

            CheckableGroupBox {
                id: smoothingBox
                Layout.fillWidth: true
                checked: root.warpingSettings ? root.warpingSettings.useSmoothingRadius : false
                text: "Smoothing radius (m)"
                contentsVisible: checked

                onCheckedChanged: {
                    if(root.warpingSettings) {
                        root.warpingSettings.useSmoothingRadius = checked;
                    }
                }

                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    InformationButton { showItemOnClick: smoothingRadiusHelp }

                    ClickTextInput {
                        Layout.fillWidth: true
                        validator: DistanceValidator {}
                        enabled: smoothingBox.checked
                        text: root.warpingSettings ? root.warpingSettings.smoothingRadiusMeters.toFixed(2) : ""
                        onFinishedEditting: (newText) => {
                            if(!root.warpingSettings) return;
                            const value = root.parseDistance(newText);
                            if(value === null || value <= 0.0) return;
                            root.warpingSettings.smoothingRadiusMeters = value;
                            // text = value.toFixed(2);
                        }
                    }
                }
            }

            HelpArea {
                id: smoothingRadiusHelp
                Layout.fillWidth: true
                text: "Applies Gaussian smoothing to Z values over this radius to reduce surface noise."
            }
        }
    }
}

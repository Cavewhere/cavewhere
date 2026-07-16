import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: itemId

    property RenderingSettings renderingSettings: RootData.settings.renderingSettings

    QC.GroupBox {
        title: "Anti-aliasing"
        Layout.fillWidth: true

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: msaaHelpId
                }

                QC.Label {
                    text: "MSAA samples"
                }

                QC.ComboBox {
                    id: msaaComboBoxId
                    objectName: "msaaSampleCountComboBox"

                    textRole: "text"
                    valueRole: "value"
                    // Built from the backend's supported MSAA levels (reported by
                    // cwRhiScene), so the list only ever offers counts the device
                    // accepts — platform dependent (e.g. Metal: 1/2/4, no 8).
                    model: {
                        let counts = itemId.renderingSettings.supportedSampleCounts;
                        let entries = [];
                        for (let i = 0; i < counts.length; ++i) {
                            let n = counts[i];
                            entries.push({ text: n === 1 ? "Off (1×)" : (n + "×"), value: n });
                        }
                        return entries;
                    }

                    // indexOfValue() isn't reactive and a currentIndex binding
                    // evaluates before the model is applied, so it would resolve to
                    // -1 (no selection). Set it once the model exists and re-sync
                    // whenever the setting or the supported list changes (e.g.
                    // Restore Defaults, or the backend reporting its sample counts).
                    function syncToSettings() {
                        currentIndex = indexOfValue(itemId.renderingSettings.sampleCount);
                    }

                    QQ.Component.onCompleted: syncToSettings()
                    onCountChanged: syncToSettings()
                    onActivated: {
                        itemId.renderingSettings.sampleCount = currentValue;
                    }

                    QQ.Connections {
                        target: itemId.renderingSettings
                        function onSampleCountChanged() { msaaComboBoxId.syncToSettings() }
                    }
                }
            }

            HelpArea {
                id: msaaHelpId
                Layout.fillWidth: true
                text: "Multisample anti-aliasing smooths jagged edges in the 3D view. Higher sample counts look better but cost more GPU time, especially with a point cloud visible (Eye-Dome Lighting runs once per sample). Off disables anti-aliasing. Changes apply immediately."
            }
        }
    }

    RestoreDefaultsButton {
        settings: itemId.renderingSettings
    }
}

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: itemId

    property RenderingSettings renderingSettings: RootData.settings.renderingSettings

    // The spin box ranges come from renderingSettings, the same limits its
    // setters clamp to; only the step sizes are a UI choice.
    readonly property int gpuMemoryBudgetStepMb: 256
    readonly property int cpuCacheBudgetStepMb: 64

    // QC.SpinBox counts in integers, so the screen-space error is held scaled by
    // this factor and shown with screenSpaceErrorDecimals places.
    readonly property int screenSpaceErrorScale: 10
    readonly property int screenSpaceErrorDecimals: 1
    readonly property real screenSpaceErrorStepPx: 0.1

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

    QC.GroupBox {
        title: "Memory"
        Layout.fillWidth: true

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: gpuBudgetHelpId
                }

                QC.Label {
                    text: "GPU memory budget (MB)"
                }

                QC.SpinBox {
                    id: gpuMemoryBudgetSpinBoxId
                    objectName: "gpuMemoryBudgetSpinBox"

                    from: itemId.renderingSettings.minimumGpuMemoryBudgetMb
                    to: itemId.renderingSettings.maximumGpuMemoryBudgetMb
                    stepSize: itemId.gpuMemoryBudgetStepMb
                    editable: true

                    // SpinBox assigns value imperatively when spun or edited, which
                    // would sever a value binding. Set it explicitly instead and
                    // re-sync whenever the setting changes (e.g. Restore Defaults).
                    function syncToSettings() {
                        value = itemId.renderingSettings.gpuMemoryBudgetMb;
                    }

                    QQ.Component.onCompleted: syncToSettings()
                    onValueModified: {
                        itemId.renderingSettings.gpuMemoryBudgetMb = value
                    }

                    QQ.Connections {
                        target: itemId.renderingSettings
                        function onGpuMemoryBudgetMbChanged() { gpuMemoryBudgetSpinBoxId.syncToSettings() }
                    }
                }
            }

            HelpArea {
                id: gpuBudgetHelpId
                Layout.fillWidth: true
                text: "How much video memory CaveWhere aims to keep its render resources under. When a project goes over, the render stats HUD colors its total and note textures give detail back until it fits again."
            }

            RowLayout {
                InformationButton {
                    showItemOnClick: cpuCacheBudgetHelpId
                }

                QC.Label {
                    text: "CPU staging budget (MB)"
                }

                QC.SpinBox {
                    id: cpuCacheBudgetSpinBoxId
                    objectName: "cpuCacheBudgetSpinBox"

                    from: itemId.renderingSettings.minimumCpuCacheBudgetMb
                    to: itemId.renderingSettings.maximumCpuCacheBudgetMb
                    stepSize: itemId.cpuCacheBudgetStepMb
                    editable: true

                    function syncToSettings() {
                        value = itemId.renderingSettings.cpuCacheBudgetMb;
                    }

                    QQ.Component.onCompleted: syncToSettings()
                    onValueModified: {
                        itemId.renderingSettings.cpuCacheBudgetMb = value
                    }

                    QQ.Connections {
                        target: itemId.renderingSettings
                        function onCpuCacheBudgetMbChanged() { cpuCacheBudgetSpinBoxId.syncToSettings() }
                    }
                }
            }

            HelpArea {
                id: cpuCacheBudgetHelpId
                Layout.fillWidth: true
                text: "How many megabytes of decoded note texture may wait in main memory on their way to the GPU. A smaller budget holds fewer loads at once, so detail arrives more slowly but the app's memory stays lower."
            }

            RowLayout {
                InformationButton {
                    showItemOnClick: uploadBudgetHelpId
                }

                QC.Label {
                    text: "Upload budget (MB per frame)"
                }

                QC.SpinBox {
                    id: uploadBudgetSpinBoxId
                    objectName: "uploadBudgetSpinBox"

                    from: itemId.renderingSettings.minimumUploadBudgetMbPerFrame
                    to: itemId.renderingSettings.maximumUploadBudgetMbPerFrame
                    editable: true

                    function syncToSettings() {
                        value = itemId.renderingSettings.uploadBudgetMbPerFrame;
                    }

                    QQ.Component.onCompleted: syncToSettings()
                    onValueModified: {
                        itemId.renderingSettings.uploadBudgetMbPerFrame = value
                    }

                    QQ.Connections {
                        target: itemId.renderingSettings
                        function onUploadBudgetMbPerFrameChanged() { uploadBudgetSpinBoxId.syncToSettings() }
                    }
                }
            }

            HelpArea {
                id: uploadBudgetHelpId
                Layout.fillWidth: true
                text: "How many megabytes of texture may be copied to the GPU in a single frame. A larger budget sharpens notes sooner after a camera move; a smaller one keeps those frames smoother."
            }
        }
    }

    QC.GroupBox {
        title: "Detail"
        Layout.fillWidth: true

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: screenSpaceErrorHelpId
                }

                QC.Label {
                    text: "Note texture detail (pixels of error)"
                }

                QC.SpinBox {
                    id: screenSpaceErrorSpinBoxId
                    objectName: "screenSpaceErrorSpinBox"

                    from: Math.round(itemId.renderingSettings.minimumScreenSpaceErrorPx * itemId.screenSpaceErrorScale)
                    to: Math.round(itemId.renderingSettings.maximumScreenSpaceErrorPx * itemId.screenSpaceErrorScale)
                    stepSize: Math.round(itemId.screenSpaceErrorStepPx * itemId.screenSpaceErrorScale)
                    editable: true

                    validator: QQ.DoubleValidator {
                        bottom: itemId.renderingSettings.minimumScreenSpaceErrorPx
                        top: itemId.renderingSettings.maximumScreenSpaceErrorPx
                        decimals: itemId.screenSpaceErrorDecimals
                        notation: QQ.DoubleValidator.StandardNotation
                    }

                    textFromValue: function(value, locale) {
                        return Number(value / itemId.screenSpaceErrorScale)
                            .toLocaleString(locale, 'f', itemId.screenSpaceErrorDecimals);
                    }

                    valueFromText: function(text, locale) {
                        return Math.round(Number.fromLocaleString(locale, text)
                                          * itemId.screenSpaceErrorScale);
                    }

                    function syncToSettings() {
                        value = Math.round(itemId.renderingSettings.screenSpaceErrorPx
                                           * itemId.screenSpaceErrorScale);
                    }

                    QQ.Component.onCompleted: syncToSettings()
                    onValueModified: {
                        itemId.renderingSettings.screenSpaceErrorPx = value / itemId.screenSpaceErrorScale
                    }

                    QQ.Connections {
                        target: itemId.renderingSettings
                        function onScreenSpaceErrorPxChanged() { screenSpaceErrorSpinBoxId.syncToSettings() }
                    }
                }
            }

            HelpArea {
                id: screenSpaceErrorHelpId
                Layout.fillWidth: true
                text: "How far a note texture may fall short of screen resolution before CaveWhere loads a sharper mip level. Smaller values keep notes crisper and use more video memory; larger values settle for softer notes and load less."
            }
        }
    }

    QC.GroupBox {
        title: "Debug"
        Layout.fillWidth: true

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: hudHelpId
                }

                QC.CheckBox {
                    objectName: "showRenderStatsHudCheckBox"
                    text: "Show render stats HUD"
                    checked: itemId.renderingSettings.showRenderStatsHud
                    onToggled: {
                        itemId.renderingSettings.showRenderStatsHud = checked
                    }
                }
            }

            HelpArea {
                id: hudHelpId
                Layout.fillWidth: true
                text: "Shows a panel in the 3D view listing how many bytes each kind of render resource holds on the GPU. Useful for tracking down which part of a project is filling video memory."
            }
        }
    }

    RestoreDefaultsButton {
        settings: itemId.renderingSettings
    }
}

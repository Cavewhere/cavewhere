import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import Cavewhere 1.0

ScrollViewPage {
    id: pageId

    property OpenGLSettings renderingSettings: rootData.settings.renderingSettings
    property JobSettings jobSettings: rootData.settings.jobSettings

    ColumnLayout {
        QC.GroupBox {
            title: "Job Settings"

            ColumnLayout {
                RowLayout {
                    InformationButton {
                        showItemOnClick: threadHelpAreaId
                    }

                    Text {
                        text: "Max Number of Threads"
                    }

                    QC.SpinBox {
                        from: 1
                        to: jobSettings.idleThreadCount
                        value: jobSettings.threadCount
                        onValueChanged: {
                            jobSettings.threadCount = value;
                        }
                    }
                }

                HelpArea {
                    id: threadHelpAreaId
                    Layout.fillWidth: true
                    text: "By default and the recommend setting, CaveWhere uses all the usable threads provide by your system. Decreasing Max Number of Threads will limit the number concurrent jobs that CaveWhere execute and will reduce CaveWhere's performance. If your computer is experiencing over heating issues from CaveWhere, reducing the Max Number of Threads may help."
                }

                Text {
                    text: "Usable threads: " + jobSettings.idleThreadCount
                }
            }
        }

        QC.GroupBox {
            title: "Texture Settings"
            ColumnLayout {
                id: textureColumnId
                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "Compression"
                    helpText: "When enabled, CaveWhere uses DXT1 compression OpenGL texture maps. This slightly reduces image quality of scraps, but reduces GPU memory by 1/4. When disabled, 4 times more GPU memory will be used but textures will look slightly better. It's recommended to leave this option enabled if supported.";
                    supported: renderingSettings.dxt1Supported
                    using: renderingSettings.useDXT1Compression
                    onUsingChanged: {
                        renderingSettings.useDXT1Compression = using
                    }
                }

                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "GPU Accelerated Compression"
                    helpText: "When enabled, CaveWhere will compress images on the GPU, instead of the CPU. When enabled, this can improve image loading preformance by 20 time vs thread CPU compression (when this option is disabled). It's recommended to leave this option enabled if supported."
                    supported: renderingSettings.gpuGeneratedDXT1Supported
                    using: renderingSettings.dxt1Algorithm === OpenGLSettings.DXT1_GPU
                    onUsingChanged: {
                        if(using) {
                            renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_GPU
                        } else {
                            renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_Squish
                        }
                    }
                }

                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "Anisotropy"
                    helpIcon: "qrc:/icons/Anisotropic_filtering_en.png"
                    helpText: "When enabled, CaveWhere improves texture rendering by reducing texture blurring between mipmaps at the cost of rendering preformance. Rendering preformance maybe improved by disabling this option."
                    supported: renderingSettings.anisotropySupported
                    using: renderingSettings.useAnisotropy
                    onUsingChanged: {
                        renderingSettings.useAnisotropy = using
                    }
                }
            }
        }

        CheckableGroupBox {
            id: mipmapsId
            text: "Use Mipmaps"
            checked: renderingSettings.useMipmaps
            onCheckedChanged: {
                renderingSettings.useMipmaps = checked
            }

            ColumnLayout {
                InformationButton {
                    showItemOnClick: mipmapHelpAreaId
                }

                HelpArea {
                    id: mipmapHelpAreaId
                    Layout.fillWidth: true
                    text: "When mipmaps are enabled, CaveWhere will pyramid textures to reduce rendering artifacts. Disabling this option will reduce texture memory use, but will reduce the quality of the renderer. It's recommended to keep this enabled."
                }

                ComboBoxWithInfo {
                    enabled: mipmapsId.enabled
                    text: "Magification Filter"
                    helpText: "When set to Nearest, zooming in on textures will render individual pixels. Using Linear, will cause the textures pixel to be blended together";
                    model: renderingSettings.magFilterModel
                    currentIndex: renderingSettings.magFilter
                    onCurrentIndexChanged: {
                        renderingSettings.magFilter = currentIndex
                    }
                }

                ComboBoxWithInfo {
                    enabled: mipmapsId.enabled
                    text: "Minification Filter"
                    helpText: "The renderer will filter between mipmaps when zoomed out. The default is Nearest Mipmap Linear";
                    model: renderingSettings.minFilterModel
                    currentIndex: renderingSettings.minFilter
                    onCurrentIndexChanged: {
                        renderingSettings.minFilter = currentIndex
                    }
                }
            }
        }

        QC.GroupBox {
            title: "Engine"

            ColumnLayout {

                Text {
                    visible: renderingSettings.needsRestart
                    text: "Restart Required!"
                }

                ComboBoxWithInfo {
                    text: "Renderer"
                    helpText: "The underlying OpenGL engine. It's recommended to use Automatic which should choose the correct renderer for you."
                    model: renderingSettings.rendererModel
                    currentIndex: renderingSettings.currentSupportedRenderer
                    onCurrentIndexChanged: {
                        renderingSettings.currentSupportedRenderer = currentIndex
                    }
                }

                CheckBoxWithInfo {
                    Layout.fillWidth: true
                    text: "Native Text Rendering"
                    helpText: "When enabled, CaveWhere uses native texture rendering. When disabled, CaveWhere uses OpenGL texture rendering (better performance). Unless you're seeing text rendering artifacts, it's recommended to keep this option disabled";
                    using: renderingSettings.useNativeTextRendering
                    onUsingChanged: {
                        renderingSettings.useNativeTextRendering = using
                    }
                }
            }
        }

        QC.GroupBox {
            title: "OpenGL Info"

            ColumnLayout {
                ResizeableScrollView {
                    implicitWidth: 400
                    implicitHeight: 150
                    QC.TextArea {
                        readOnly: true
                        selectByMouse: true
                        text: renderingSettings.allVersionInfo
                    }
                }
            }
        }
    }
}

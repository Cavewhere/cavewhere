import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import Cavewhere 1.0

ScrollViewPage {
    id: pageId

    ColumnLayout {
        QC.GroupBox {
            title: "Texture Settings"
            ColumnLayout {
                id: textureColumnId
                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "Compression"
                    helpText: "When enabled, CaveWhere uses DXT1 compression OpenGL texture maps. This slightly reduces image quality of scraps, but reduces GPU memory by 1/4. When disabled, 4 times more GPU memory will be used but textures will look slightly better. It's recommended to leave this option enabled if supported.";
                    supported: rootData.renderingSettings.dxt1Supported
                    using: rootData.renderingSettings.useDXT1Compression
                    onUsingChanged: {
                        rootData.renderingSettings.useDXT1Compression = using
                    }
                }

                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "GPU Accelerated Compression"
                    helpText: "When enabled, CaveWhere will compress images on the GPU, instead of the CPU. When enabled, this can improve image loading preformance by 20 time vs thread CPU compression (when this option is disabled). It's recommended to leave this option enabled if supported."
                    supported: rootData.renderingSettings.gpuGeneratedDXT1Supported
                    using: rootData.renderingSettings.dxt1Algorithm === OpenGLSettings.DXT1_GPU
                    onUsingChanged: {
                        if(using) {
                            rootData.renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_GPU
                        } else {
                            rootData.renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_Squish
                        }
                    }
                }

                SupportedCheckLabel {
                    Layout.fillWidth: true
                    text: "Anisotropy"
                    helpIcon: "qrc:/icons/Anisotropic_filtering_en.png"
                    helpText: "When enabled, CaveWhere improves texture rendering by reducing texture blurring between mipmaps at the cost of rendering preformance. Rendering preformance maybe improved by disabling this option."
                    supported: rootData.renderingSettings.anisotropySupported
                    using: rootData.renderingSettings.useAnisotropy
                    onUsingChanged: {
                        rootData.renderingSettings.useAnisotropy = using
                    }
                }
            }
        }

        CheckableGroupBox {
            id: mipmapsId
            text: "Use Mipmaps"
            checked: rootData.renderingSettings.useMipmaps
            onCheckedChanged: {
                rootData.renderingSettings.useMipmaps = checked
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
                    model: rootData.renderingSettings.magFilterModel
                    currentIndex: rootData.renderingSettings.magFilter
                    onCurrentIndexChanged: {
                        rootData.renderingSettings.magFilter = currentIndex
                    }
                }

                ComboBoxWithInfo {
                    enabled: mipmapsId.enabled
                    text: "Minification Filter"
                    helpText: "The renderer will filter between mipmaps when zoomed out. The default is Nearest Mipmap Linear";
                    model: rootData.renderingSettings.minFilterModel
                    currentIndex: rootData.renderingSettings.minFilter
                    onCurrentIndexChanged: {
                        rootData.renderingSettings.minFilter = currentIndex
                    }
                }
            }
        }

        QC.GroupBox {
            title: "Engine"

            ColumnLayout {

                Text {
                    visible: rootData.renderingSettings.needsRestart
                    text: "Restart Required!"
                }

                ComboBoxWithInfo {
                    text: "Renderer"
                    helpText: "The underlying OpenGL engine. It's recommended to use Automatic which should choose the correct renderer for you."
                    model: rootData.renderingSettings.rendererModel
                    currentIndex: rootData.renderingSettings.currentSupportedRenderer
                    onCurrentIndexChanged: {
                        rootData.renderingSettings.currentSupportedRenderer = currentIndex
                    }
                }

                CheckBoxWithInfo {
                    Layout.fillWidth: true
                    text: "Native Text Rendering"
                    helpText: "When enabled, CaveWhere uses native texture rendering. When disabled, CaveWhere uses OpenGL texture rendering (better performance). Unless you're seeing text rendering artifacts, it's recommended to keep this option disabled";
                    using: rootData.renderingSettings.useNativeTextRendering
                    onUsingChanged: {
                        rootData.renderingSettings.useNativeTextRendering = using
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
                        text: rootData.renderingSettings.allVersionInfo
                    }
                }
            }
        }
    }
}

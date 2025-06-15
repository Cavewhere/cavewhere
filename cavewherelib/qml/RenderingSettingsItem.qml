import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout  {
    id: settingsId
    // property OpenGLSettings renderingSettings: RootData.settings.renderingSettings

    // QC.GroupBox {
    //     title: "Texture Settings"
    //     ColumnLayout {
    //         id: textureColumnId
    //         SupportedCheckLabel {
    //             Layout.fillWidth: true
    //             text: "Compression"
    //             helpText: "When enabled, CaveWhere uses DXT1 compression OpenGL texture maps. This slightly reduces image quality of scraps, but reduces GPU memory by 1/4. When disabled, 4 times more GPU memory will be used but textures will look slightly better. It's recommended to leave this option enabled if supported.";
    //             supported: settingsId.renderingSettings.dxt1Supported
    //             using: settingsId.renderingSettings.useDXT1Compression
    //             onUsingChanged: {
    //                 settingsId.renderingSettings.useDXT1Compression = using
    //             }
    //         }

    //         SupportedCheckLabel {
    //             Layout.fillWidth: true
    //             text: "GPU Accelerated Compression"
    //             helpText: "When enabled, CaveWhere will compress images on the GPU, instead of the CPU. When enabled, this can improve image loading preformance by 20 time vs thread CPU compression (when this option is disabled). It's recommended to leave this option enabled if supported."
    //             supported: settingsId.renderingSettings.gpuGeneratedDXT1Supported
    //             using: settingsId.renderingSettings.dxt1Algorithm === OpenGLSettings.DXT1_GPU
    //             onUsingChanged: {
    //                 if(using) {
    //                     settingsId.renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_GPU
    //                 } else {
    //                     settingsId.renderingSettings.dxt1Algorithm = OpenGLSettings.DXT1_Squish
    //                 }
    //             }
    //         }

    //         SupportedCheckLabel {
    //             Layout.fillWidth: true
    //             text: "Anisotropy"
    //             helpIcon: "qrc:/icons/Anisotropic_filtering_en.png"
    //             helpText: "When enabled, CaveWhere improves texture rendering by reducing texture blurring between mipmaps at the cost of rendering preformance. Rendering preformance maybe improved by disabling this option."
    //             supported: settingsId.renderingSettings.anisotropySupported
    //             using: settingsId.renderingSettings.useAnisotropy
    //             onUsingChanged: {
    //                 settingsId.renderingSettings.useAnisotropy = using
    //             }
    //         }
    //     }
    // }

    // CheckableGroupBox {
    //     id: mipmapsId
    //     text: "Use Mipmaps"
    //     checked: settingsId.renderingSettings.useMipmaps
    //     onCheckedChanged: {
    //         settingsId.renderingSettings.useMipmaps = checked
    //     }

    //     ColumnLayout {
    //         InformationButton {
    //             showItemOnClick: mipmapHelpAreaId
    //         }

    //         HelpArea {
    //             id: mipmapHelpAreaId
    //             Layout.fillWidth: true
    //             text: "When mipmaps are enabled, CaveWhere will pyramid textures to reduce rendering artifacts. Disabling this option will reduce texture memory use, but will reduce the quality of the renderer. It's recommended to keep this enabled."
    //         }

    //         ComboBoxWithInfo {
    //             enabled: mipmapsId.enabled
    //             text: "Magification Filter"
    //             helpText: "When set to Nearest, zooming in on textures will render individual pixels. Using Linear, will cause the textures pixel to be blended together";
    //             model: settingsId.renderingSettings.magFilterModel
    //             currentIndex: settingsId.renderingSettings.magFilter
    //             onCurrentIndexChanged: {
    //                 settingsId.renderingSettings.magFilter = currentIndex
    //             }
    //         }

    //         ComboBoxWithInfo {
    //             enabled: mipmapsId.enabled
    //             text: "Minification Filter"
    //             helpText: "The renderer will filter between mipmaps when zoomed out. The default is Nearest Mipmap Linear";
    //             model: settingsId.renderingSettings.minFilterModel
    //             currentIndex: settingsId.renderingSettings.minFilter
    //             onCurrentIndexChanged: {
    //                 settingsId.renderingSettings.minFilter = currentIndex
    //             }
    //         }
    //     }
    // }

    // QC.GroupBox {
    //     title: "Engine"

    //     ColumnLayout {

    //         Text {
    //             visible: settingsId.renderingSettings.needsRestart
    //             text: "Restart Required!"
    //         }

    //         ComboBoxWithInfo {
    //             text: "Renderer"
    //             helpText: "The underlying OpenGL engine. It's recommended to use Automatic which should choose the correct renderer for you."
    //             model: settingsId.renderingSettings.rendererModel
    //             currentIndex: settingsId.renderingSettings.currentSupportedRenderer
    //             onCurrentIndexChanged: {
    //                 settingsId.renderingSettings.currentSupportedRenderer = currentIndex
    //             }
    //         }

    //         CheckBoxWithInfo {
    //             Layout.fillWidth: true
    //             text: "Native Text Rendering"
    //             helpText: "When enabled, CaveWhere uses native texture rendering. When disabled, CaveWhere uses OpenGL texture rendering (better performance). Unless you're seeing text rendering artifacts, it's recommended to keep this option disabled";
    //             using: settingsId.renderingSettings.useNativeTextRendering
    //             onUsingChanged: {
    //                 settingsId.renderingSettings.useNativeTextRendering = using
    //             }
    //         }
    //     }
    // }

    // QC.GroupBox {
    //     title: "OpenGL Info"

    //     ColumnLayout {
    //         ResizeableScrollView {
    //             implicitWidth: 400
    //             implicitHeight: 150
    //             QC.TextArea {
    //                 readOnly: true
    //                 selectByMouse: true
    //                 text: settingsId.renderingSettings.allVersionInfo
    //             }
    //         }
    //     }
    // }
}

#ifndef CWRENDERMATERIALSTATE_H
#define CWRENDERMATERIALSTATE_H

#include <QString>
#include <QFlags>

/**
 * @brief Describes high-level pipeline state for a renderable item.
 *
 * This stays on the GUI thread and gets copied to the RHI thread during
 * synchronization so that the backend can configure QRhi pipelines without
 * needing to know about front-end objects.
 */
struct cwRenderMaterialState
{
    enum class ShaderStage {
        Vertex   = 0x1,
        Fragment = 0x2
    };
    Q_DECLARE_FLAGS(ShaderStages, ShaderStage)

    enum class CullMode {
        None,
        Front,
        Back
    };

    enum class FrontFace {
        CCW,
        CW
    };

    enum class BlendMode {
        None,
        Alpha,
        Additive,
        PremultipliedAlpha
    };

    enum class RenderPass {
        Opaque,
        Transparent,
        ShadowMap,
        Overlay
    };

    QString vertexShader = QStringLiteral(":/shaders/scrap.vert.qsb");
    QString fragmentShader = QStringLiteral(":/shaders/scrap.frag.qsb");
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CCW;
    bool depthTest = true;
    bool depthWrite = true;
    BlendMode blendMode = BlendMode::None;
    RenderPass renderPass = RenderPass::Opaque;

    int globalUniformBinding = 0;
    ShaderStages globalUniformStages = ShaderStage::Vertex;

    int perDrawUniformBinding = -1;
    ShaderStages perDrawUniformStages = ShaderStage::Vertex;

    int textureBinding = 1;
    ShaderStages textureStages = ShaderStage::Fragment;

    bool operator==(const cwRenderMaterialState& other) const = default;

    inline bool wantsPerDrawUniform() const { return perDrawUniformBinding >= 0; }
    inline bool requiresBlending() const { return blendMode != BlendMode::None; }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(cwRenderMaterialState::ShaderStages)

#endif // CWRENDERMATERIALSTATE_H

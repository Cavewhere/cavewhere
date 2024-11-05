//Qt includes
#include <QQuickRhiItemRenderer>
#include <QQuaternion>
class QRhiBuffer;
class QRhiRenderPassDescriptor;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
class QRhiTextureRenderTarget;
class QRhiResourceUpdateBatch;

class cwCompassRenderer : public QQuickRhiItemRenderer
{
public:
    cwCompassRenderer();
    ~cwCompassRenderer();

    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem* item) override;
    void render(QRhiCommandBuffer* cb) override;

private:
    enum Direction {
        Top,
        Bottom
    };

    // Geometry and buffers
    struct Vertex {
        QVector3D position;
        QVector3D color;
    };
    // QVector<Vertex> m_compassVertices;
    int m_vertexCount;

    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_textureVertexBuffer = nullptr;

    // Textures and render targets
    QRhiTexture* m_compassTexture = nullptr;
    QRhiTextureRenderTarget* m_compassRenderTarget = nullptr;
    QRhiRenderPassDescriptor* m_compassRenderPassDesc = nullptr;

    QRhiTexture* m_shadowTexture = nullptr;
    QRhiTextureRenderTarget* m_shadowRenderTarget = nullptr;
    QRhiRenderPassDescriptor* m_shadowRenderPassDesc = nullptr;

    QRhiTexture* m_horizontalBlurTexture = nullptr;
    QRhiTextureRenderTarget* m_horizontalBlurRenderTarget = nullptr;
    QRhiRenderPassDescriptor* m_horizontalBlurRenderPassDesc = nullptr;

    // Pipelines
    QRhiGraphicsPipeline* m_compassPipeline = nullptr;
    QRhiShaderResourceBindings* m_compassBindings = nullptr;

    QRhiGraphicsPipeline* m_shadowPipeline = nullptr;
    QRhiShaderResourceBindings* m_shadowBindings = nullptr;

    QRhiGraphicsPipeline* m_blurPipeline = nullptr;
    QRhiShaderResourceBindings* m_blurBindings = nullptr;

    QRhiGraphicsPipeline* m_outputPipeline = nullptr;
    QRhiShaderResourceBindings* m_outputBindings = nullptr;

    // Uniforms and constants
    QRhiBuffer* m_uniformBuffer = nullptr;

    // Synchronization variables
    // QPointer<cwCamera> m_camera;
    QQuaternion m_rotation;

    bool m_resourcesInitialized = false;
    bool m_lookingDown = false;

    // Methods
    void initializeResources(QRhiCommandBuffer *cb);
    void initializeGeometry(QRhiCommandBuffer* cb);
    void generateStarGeometry(QVector<Vertex> &triangles, Direction direction);
    void createPipelines(QRhi *rhi);
    void renderShadow(QRhiCommandBuffer* cb);
    void renderCompass(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch *batch);
    void drawFramebuffer(QRhiCommandBuffer* cb, QRhiTexture* texture, QRhiGraphicsPipeline* pipeline, QRhiShaderResourceBindings* bindings);
    // void labelCompass();
};

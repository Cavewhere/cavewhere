#ifndef CWRHIOBJECT_H
#define CWRHIOBJECT_H

//Qt includes
#include "cwScene.h"
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

#include <rhi/qrhi.h>
#include <functional>
#include <QVector>

//Our includes
class cwRhiItemRenderer;
class cwRhiScene;
class cwRenderObject;

class cwRHIObject {

public:
    virtual ~cwRHIObject() {}

    struct RenderData {
        QRhiCommandBuffer* cb;
        cwRhiItemRenderer* renderer;
        cwRhiScene* scene;
        cwSceneUpdate::Flag updateFlag;
    };

    struct ResourceUpdateData {
        QRhiResourceUpdateBatch* resourceUpdateBatch;
        RenderData renderData;
    };

    struct SynchronizeData {
        cwRenderObject* object;
        cwRhiItemRenderer* renderer;
    };

    //For rendering
    enum class RenderPass : int {
        Opaque = 0,
        Transparent,
        Overlay,
        ShadowMap,
        Count
    };

    struct GatherContext {
        const RenderData* renderData;
        RenderPass renderPass;
        quint32 objectOrder = 0;
    };

    struct Drawable {
        enum class Type {
            Indexed,
            NonIndexed,
            Custom
        };

        Type type = Type::Indexed;
        QVector<QRhiCommandBuffer::VertexInput> vertexBindings;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;
        quint32 indexOffset = 0;
        qint32 vertexOffset = 0;
        quint32 indexCount = 0;
        quint32 vertexCount = 0;
        quint32 instanceCount = 1;
        quint32 firstInstance = 0;
        QRhiShaderResourceBindings* bindings = nullptr;
        std::function<void(const RenderData&)> customDraw;
    };

    struct PipelineState {
        QRhiGraphicsPipeline* pipeline = nullptr;
        quint64 sortKey = 0;
        bool operator==(const PipelineState& other) const = default;

    };

    struct PipelineBatch {
        PipelineState state;
        QVector<Drawable> drawables;
    };

    static quint64 makeSortKey(quint32 objectOrder, QRhiGraphicsPipeline* pipeline)
    {
        const quint64 orderPart = quint64(objectOrder) << 32;
        const quint64 pipelinePart = quint64(quintptr(pipeline)) & 0xffffffffull;
        return orderPart | pipelinePart;
    }

    virtual void initialize(const ResourceUpdateData& data) = 0;
    virtual void synchronize(const SynchronizeData& data) = 0;
    virtual void updateResources(const ResourceUpdateData& data) = 0;

    //Older rendering method
    virtual void render(const RenderData& data) = 0;

    //Gather render objects
    virtual bool gather(const GatherContext& context,
                        QVector<PipelineBatch>& batches) {
        Q_UNUSED(context);
        Q_UNUSED(batches);
        return false;
    };

    void setVisible(bool visible) { m_isVisible = visible; }
    bool isVisible() const { return m_isVisible; }

    static QShader loadShader(const QString& name);
private:
    bool m_isVisible = true;


protected:    
    static PipelineBatch& acquirePipelineBatch(QVector<PipelineBatch>& batches,
                                               const PipelineState& state)
    {
        for (auto& existing : batches) {
            if (existing.state.pipeline == state.pipeline &&
                existing.state.sortKey == state.sortKey) {
                return existing;
            }
        }

        batches.append(PipelineBatch{state, {}});
        return batches.last();
    }

};


#endif // CWRHIOBJECT_H

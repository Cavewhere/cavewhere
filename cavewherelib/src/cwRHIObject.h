#ifndef CWRHIOBJECT_H
#define CWRHIOBJECT_H

//Qt includes
#include "cwScene.h"
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

#include <rhi/qrhi.h>

//Our includes
class cwRhiItemRenderer;
class cwRenderObject;

class cwRHIObject {

public:
    virtual ~cwRHIObject() {}

    struct RenderData {
        QRhiCommandBuffer* cb;
        cwRhiItemRenderer* renderer;
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
        Opaque
    };

    struct GatherContext {
        const RenderData* renderData;
        RenderPass renderPass;
    };

    struct Drawable {

    };

    struct PipelineKey {
        quint64 vertexShaderId = 0;
        quint64 fragmentShaderId = 0;
        quint64 vertexLayoutId = 0;

    };

    struct PipelineSet {
        PipelineKey key;
        QVector<Drawable> items;
    };

    virtual void initialize(const ResourceUpdateData& data) = 0;
    virtual void synchronize(const SynchronizeData& data) = 0;
    virtual void updateResources(const ResourceUpdateData& data) = 0;

    //Older rendering method
    virtual void render(const RenderData& data) = 0;

    //Gather render objects
    virtual void gather(const GatherContext& context,
                        QVector<PipelineSet>& drawables) {};

    void setVisible(bool visible) { m_isVisible = visible; }
    bool isVisible() const { return m_isVisible; }

    static QShader loadShader(const QString& name);
private:
    bool m_isVisible = true;


protected:    

};


#endif // CWRHIOBJECT_H

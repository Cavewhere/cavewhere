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

    virtual void initialize(const ResourceUpdateData& data) = 0;
    virtual void synchronize(const SynchronizeData& data) = 0;
    virtual void updateResources(const ResourceUpdateData& data) = 0;
    virtual void render(const RenderData& data) = 0;

    static QShader loadShader(const QString& name);
private:
         // QPointer<cwGLObject> m_guiObject;

protected:    

};


#endif // CWRHIOBJECT_H

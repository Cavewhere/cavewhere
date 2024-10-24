#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwSceneUpdate.h"
class cwScene;
class cwRHIObject;
class cwRenderObject;

/**
 * @brief The cwRhiScene class
 *
 * The backend renderer for the scene object. Renders to Qt RHI
 */
class cwRhiScene {
public:
    friend class cwRhiItemRenderer;

    ~cwRhiScene();

    QMatrix4x4 viewMatrix() const { return m_viewMatrix; }
    QMatrix4x4 projectionMatrix() const { return m_projectionCorrectedMatrix; }
    QMatrix4x4 viewProjectionMatrix() const { return m_viewProjectionMatrix; }
    float devicePixelRatio() const { return m_devicePixelRatio; }
    QRhiBuffer* globalUniformBuffer() const { return m_globalUniformBuffer; }

private:
    void initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer);
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

    QList<cwRHIObject*> m_rhiObjectsToInitilize;
    QList<cwRHIObject*> m_rhiObjects;
    QList<cwRHIObject*> m_rhiNeedResourceUpdate;

    //Should only be used in synchroize
    QHash<cwRenderObject*, cwRHIObject*> m_rhiObjectLookup;

    struct GlobalUniform {
        float viewProjectionMatrix[16];
        float viewMatrix[16];
        float projectionMatrix[16];
        float devicePixelRatio;
    };

    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_projectionCorrectedMatrix;
    QMatrix4x4 m_viewProjectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_devicePixelRatio;

    QRhiBuffer* m_globalUniformBuffer = nullptr;

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }


};


#endif // CWRHISCENE_H

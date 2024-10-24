/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENE_H
#define CWSCENE_H

//Qt includes
#include "cwGLObject.h"
#include <QObject>
#include <QList>
#include <QQueue>
#include <QQmlEngine>
#include <QHash>
#include <rhi/qrhi.h>
class QRhiCommandBuffer;
class QPainter;

//Our includes
class cwRenderObject;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;
class cwRhiItemRenderer;
class QRhiBuffer;
class QRhiShaderResourceBindings;

namespace cwSceneUpdate {
enum class Flag : int {
    None = 0,
    ViewMatrix = 0x1,
    ProjectionMatrix = 0x2,
    DevicePixelRatio = 0x4,
};

inline Flag operator|(Flag lhs, Flag rhs) {
    return static_cast<cwSceneUpdate::Flag>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
inline Flag operator&(Flag lhs, Flag rhs) {
    return static_cast<cwSceneUpdate::Flag>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
inline Flag& operator|=(Flag& lhs, Flag rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool isFlagSet(cwSceneUpdate::Flag combinedFlags, cwSceneUpdate::Flag flagToTest) {
    return (combinedFlags & flagToTest) == flagToTest;
}

// Helper function to convert the enum to a readable string
QString flagToString(Flag flag);

// Overload operator<< for QDebug to handle cwSceneUpdate::Flag
inline QDebug operator<<(QDebug dbg, Flag flag) {
    dbg.nospace() << flagToString(flag);
    return dbg.space();
}

};

/**
 * @brief The cwScene class
 */
class cwScene : public QObject
{
    friend class cwSceneRenderer;

    Q_OBJECT
    QML_NAMED_ELEMENT(Scene)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    void addItem(cwRenderObject* item);
    void removeItem(cwRenderObject* item);
    void updateItem(cwRenderObject* item);

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    void update();

    void releaseResources();

signals:
    void cameraChanged();
    void needsRendering();

public slots:

private:
    //Items to render
    QList<cwRenderObject*> m_renderingObjects;
    QList<cwRenderObject*> m_newRenderObjects;
    QList<cwRenderObject*> m_toDeleteRenderObjects;
    QSet<cwRenderObject*> m_toUpdateRenderObjects;

    //For interaction
    cwGeometryItersecter* GeometryItersecter;

    //The main camera for the viewer
    cwCamera* Camera;

    //cwSceneUpdate::Flag flags
    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;

};

/**
 * @brief The cwSceneRenderer class
 *
 * The backend renderer for the scene object. Renders to Qt RHI
 */
class cwSceneRenderer {
public:
    friend class cwRhiItemRenderer;

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

    //     cwScene* scene;

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
    // QRhiShaderResourceBindings* m_srb = nullptr;

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }


};



#endif // CWSCENE_H

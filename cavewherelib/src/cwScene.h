/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENE_H
#define CWSCENE_H

//Qt includes
#include <QObject>
#include <QList>
#include <QQueue>
#include <QQmlEngine>
#include <QHash>

//Our includes
#include "cwSceneUpdate.h"
#include "CaveWhereLibExport.h"
#include <QSet>
class cwRenderObject;
class cwRenderPointCloud;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;
class cwRhiItemRenderer;


/**
 * @brief The cwScene class
 */
class CAVEWHERE_LIB_EXPORT cwScene : public QObject
{
    friend class cwRhiScene;

    Q_OBJECT
    QML_NAMED_ELEMENT(Scene)

    Q_PROPERTY(float pointCloudGapFudge READ pointCloudGapFudge WRITE setPointCloudGapFudge NOTIFY pointCloudGapFudgeChanged)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    void addItem(cwRenderObject* item);
    void removeItem(cwRenderObject* item);
    void updateItem(cwRenderObject* item);

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    float pointCloudGapFudge() const { return m_pointCloudGapFudge; }

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    void update();

    void releaseResources();

    // Live pointers held in queues that cwRhiScene::synchroize() dereferences.
    int pendingItemCount() const;

public slots:
    void setPointCloudGapFudge(float gapFudge);

signals:
    void cameraChanged();
    void needsRendering();
    void pointCloudGapFudgeChanged(float gapFudge);

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

    float m_pointCloudGapFudge = 2.0f;

    // Separate set because m_newRenderObjects / m_toUpdateRenderObjects drain
    // during cwRhiScene::synchroize, leaving no list of live clouds for
    // setPointCloudGapFudge to iterate.
    QSet<cwRenderPointCloud*> m_pointClouds;
};




#endif // CWSCENE_H

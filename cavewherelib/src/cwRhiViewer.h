/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLRENDERER_H
#define CWGLRENDERER_H

//Qt includes
#include <QOpenGLBuffer>
#include <QQuaternion>
// #include <QStateMachine>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QQuickRhiItem>
#include <QPointer>
#include <memory>

//Our includes
#include "cwCamera.h"
class cwRhiRendererHandle;
// class cwMouseEventTransition;
class cwGLShader;
class cwShaderDebugger;
class cwCavingRegion;
class cwCave;
class cwScene;
// #include "cwStation.h"
// #include "cwRegularTile.h"
// #include "cwEdgeTile.h"
// #include "cwCollisionRectKdTree.h"


class cwRhiViewer : public QQuickRhiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RhiViewer)
    Q_PROPERTY(cwCamera* camera READ camera NOTIFY cameraChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)

public:
    explicit cwRhiViewer(QQuickItem *parent = 0);
    ~cwRhiViewer();

    cwCamera* camera() const;

    cwScene* scene() const;
    void setScene(cwScene* scene);

    //! The rendezvous a cwRhiItemRenderer attaches itself to at its first
    //! synchronize, so this item can reach the render thread while it is hidden
    std::shared_ptr<cwRhiRendererHandle> rendererHandle() const { return m_rendererHandle; }

protected:

signals:
    void glWidgetChanged();
    void cameraChanged();
    void sceneChanged();

protected:
    //The main camera for the viewer
    cwCamera* Camera;

    //For Painting
    QPointer<cwScene> Scene;

protected slots:
    virtual void resizeGL() {}
    void updateRenderer();

private slots:
    void privateResize();


    // QQuickRhiItem interface
protected:
    QQuickRhiItemRenderer *createRenderer() override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    std::shared_ptr<cwRhiRendererHandle> m_rendererHandle;
};

inline cwCamera* cwRhiViewer::camera() const { return Camera; }
inline void cwRhiViewer::updateRenderer() { update(); }



#endif // CWGLRENDERER_H

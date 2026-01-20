/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBJECT_H
#define CWGLOBJECT_H

//Qt includes
#include <QObject>
#include <QOpenGLFunctions>
#include <QPointer>
class QOpenGLShaderProgram;

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwRhiViewer;
class cwScene;
class cwUpdateDataCommand;
class cwRHIObject;
class cwRhiItemRenderer;
class cwGeometryItersector;
#include "cwScene.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwRenderObject : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

    friend class cwRhiScene;

public:
    cwRenderObject(QObject* parent = nullptr);
    ~cwRenderObject();

    void setScene(cwScene *scene);
    cwScene *scene() const;

    cwGeometryItersecter* geometryItersecter() const;

    cwCamera* camera() const;

    void update();

    bool isVisible() const;
    void setVisible(bool newVisible);

signals:
    void sceneChange();

    void visibleChanged();

protected:
    virtual cwRHIObject* createRHIObject() { return nullptr; }
    // virtual cwSceneUpdate::Flag updateOnFlags() const { return cwSceneUpdate::Flag::None; }

private:
    cwScene* m_scene;

    bool m_visible = true;
};



/**
 * @brief cwRenderObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwRenderObject::scene() const
{
    return m_scene;
}

inline bool cwRenderObject::isVisible() const
{
    return m_visible;
}




#endif // CWGLOBJECT_H

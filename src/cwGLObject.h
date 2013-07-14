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

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwGLRenderer;
#include "cwGeometryItersecter.h"

class cwGLObject : public QObject
{
public:
    cwGLObject(QObject* parent = NULL);

    virtual void initialize() = 0;
    virtual void draw() = 0;
    virtual void updateData() {}

    void setScene(cwGLRenderer* renderer);
    cwGLRenderer* scene() const;

    cwGeometryItersecter* geometryItersecter() const;
    cwCamera* camera() const;
    cwShaderDebugger* shaderDebugger() const;

    bool isDirty() const;

protected:
    void setDirty(bool isDirty);

private:
    cwGLRenderer* Scene;
    bool Dirty;
};

inline bool cwGLObject::isDirty() const
{
    return Dirty;
}



/**
 * @brief cwGLObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwGLRenderer *cwGLObject::scene() const
{
    return Scene;
}

#endif // CWGLOBJECT_H

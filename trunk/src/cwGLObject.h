#ifndef CWGLOBJECT_H
#define CWGLOBJECT_H

//Qt includes
#include <QObject>

//Our includes
class cwCamera;
class cwShaderDebugger;

class cwGLObject : public QObject
{
public:
    cwGLObject(QObject* parent = NULL);

    virtual void initalize() = 0;
    virtual void draw() = 0;

    void setCamera(cwCamera* camera);
    cwCamera* camera() const;

    void setShaderDebugger(cwShaderDebugger* debugger);
    cwShaderDebugger* shaderDebugger() const;

private:
    cwCamera* Camera;
    cwShaderDebugger* ShaderDebugger;
};

inline void cwGLObject::setCamera(cwCamera* camera) {
    Camera = camera;
}

inline cwCamera* cwGLObject::camera() const {
    return Camera;
}

inline void cwGLObject::setShaderDebugger(cwShaderDebugger* debugger) {
    ShaderDebugger = debugger;
}

inline cwShaderDebugger* cwGLObject::shaderDebugger() const {
    return ShaderDebugger;
}

#endif // CWGLOBJECT_H

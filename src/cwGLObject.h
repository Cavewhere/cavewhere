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

    virtual void initialize() = 0;
    virtual void draw() = 0;
    virtual void updateData() {}

    void setCamera(cwCamera* camera);
    cwCamera* camera() const;

    void setShaderDebugger(cwShaderDebugger* debugger);
    cwShaderDebugger* shaderDebugger() const;

    bool isDirty() const;

protected:
    void setDirty(bool isDirty);

private:
    cwCamera* Camera;
    cwShaderDebugger* ShaderDebugger;
    bool Dirty;
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

inline bool cwGLObject::isDirty() const
{
    return Dirty;
}

inline void cwGLObject::setDirty(bool isDirty)
{
    Dirty = isDirty;
}


#endif // CWGLOBJECT_H

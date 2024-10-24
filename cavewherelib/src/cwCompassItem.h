/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLCOMPASS_H
#define CWGLCOMPASS_H

//Our includes
#include "cwRenderObject.h"
#include "cwCamera.h"
#include "cwShaderDebugger.h"
class cwCamera;

//Qt includes
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;
#include <QQuickPaintedItem>
#include <QOpenGLBuffer>
#include <QQuaternion>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QQmlEngine>

/**
 * @brief The cwGLCompass class
 *
 * This draws 3d compass on the screen.
 */
class cwCompassItem : public QQuickPaintedItem, private QOpenGLFunctions
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CompassItem)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QQuaternion rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(cwShaderDebugger* shaderDebugger READ shaderDebugger WRITE setShaderDebugger NOTIFY shaderDebuggerChanged)

public:
    explicit cwCompassItem(QQuickItem *parent = 0);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwShaderDebugger* shaderDebugger() const;
    void setShaderDebugger(cwShaderDebugger* shaderDebugger);

    void initialize();
    void draw();

    QQuaternion rotation() const;
    void setRotation(QQuaternion quaternion);
    QQuaternion modelView() const;

    void paint(QPainter *painter);
    void releaseResources();

signals:
    void cameraChanged();
    void rotationChanged();
    void shaderDebuggerChanged();

public slots:
    
private:

    class CompassGeometry {
    public:
        CompassGeometry() {}
        CompassGeometry(QVector3D position, QVector3D color) :
            Position(position),
            Color(color)
        {
        }

        QVector3D Position;
        QVector3D Color;
    };

    enum Direction {
        Top,
        Bottom
    };

    cwCamera* Camera;
    cwShaderDebugger* ShaderDebugger; //!<

    QOpenGLShaderProgram* Program;
    QOpenGLShaderProgram* XShadowProgram;
    QOpenGLShaderProgram* YShadowProgram;
    QOpenGLShaderProgram* ShadowOutputProgram;
    QOpenGLBuffer CompassVertexBuffer;
    QOpenGLBuffer TextureGeometryBuffer;

    QOpenGLFramebufferObject* CompassFramebuffer; //Can be a multi-sample or texture framebuffer
    QOpenGLFramebufferObject* ShadowBufferFramebuffer;
    QOpenGLFramebufferObject* HorizonalShadowBufferFramebuffer;

//    QOpenGLBuffer CompassIndexes;
    bool Initialized;

    //Shader attributes for the normal compass shader
    int vVertex;
    int vColor;
    int ModelViewProjectionMatrixUniform;
    int IgnoreColorUniform;

    //Shader attributes for the shadow compass shader
    int vVertexShadow;
    int ModelViewProjectionMatrixShadowUniform;
    int TextureUnitShadow;

    //Shader attirbutes for the shadow ouptu shader
    int vVertexShadowOutput;
    int ModelViewProjectionMatrixShadowOutputUniform;
    int TextureUnitShadowOutputUniform;

    int NumberOfPoints;

    QQuaternion Rotation;

    void initializeGeometry();
    void initializeShaders();
    void initializeShadowShader();
    void initializeShadowOutputShader();
    void initializeCompassShader();
    void initializeFramebuffer();

    void generateStarGeometry(QVector<CompassGeometry> &triangles, Direction direction);

    void drawShadow();
    void drawCompass(QOpenGLFramebufferObject *framebuffer, bool withColors, QMatrix4x4 modelView = QMatrix4x4());
    void drawFramebuffer(QOpenGLFramebufferObject *framebuffer, QMatrix4x4 modelView = QMatrix4x4());
    void labelCompass(QMatrix4x4 modelView);
    void drawLabel(QVector3D pos, QString label, QFont font, QPainter* painter, cwCamera* camera);

//    void drawStar(Direction top);

private slots:
//    void updateOrientation();



};

/**
Gets camera
*/
inline cwCamera* cwCompassItem::camera() const {
    return Camera;
}

/**
Gets rotation
*/
inline QQuaternion cwCompassItem::rotation() const {
    return Rotation;
}

/**
Gets shaderDebugger
*/
inline cwShaderDebugger* cwCompassItem::shaderDebugger() const {
    return ShaderDebugger;
}

#endif // CWGLCOMPASS_H

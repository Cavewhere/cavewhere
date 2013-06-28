#ifndef CWGLCOMPASS_H
#define CWGLCOMPASS_H

//Our includes
#include <cwGLObject.h>
class cwCamera;

//Qt includes
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;
#include <QOpenGLBuffer>
#include <QQuaternion>


/**
 * @brief The cwGLCompass class
 *
 * This draws 3d compass on the screen.
 */
class cwGLCompass : public cwGLObject
{
    Q_OBJECT

public:
    explicit cwGLCompass(QObject *parent = 0);
    
//    void setCamera(cwCamera* camera);
//    cwCamera* camera() const;

    void initialize();
    void draw();

    void setRotation(QQuaternion quaternion);
    QQuaternion rotation() const;


signals:
    
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

//    cwCamera* Camera;

    QOpenGLShaderProgram* Program;
    QOpenGLShaderProgram* XShadowProgram;
    QOpenGLShaderProgram* YShadowProgram;
    QOpenGLShaderProgram* ShadowOutputProgram;
    QOpenGLBuffer CompassVertexBuffer;
    QOpenGLBuffer TextureGeometryBuffer;

    QOpenGLFramebufferObject* CompassFramebuffer;
    QOpenGLFramebufferObject* ShadowBufferFramebuffer;
    QOpenGLFramebufferObject* HorizonalShadowBufferFramebuffer;

//    QOpenGLBuffer CompassIndexes;

    //Shader attributes for the normal compass shader
    int vVertex;
    int vColor;
    int ModelViewProjectionMatrixUniform;

    //Shader attributes for the shadow compass shader
    int vVertexShadow;
    int ModelViewProjectionMatrixShadowUniform;
    int TextureUnitShadow;

    //Shader attirbutes for the shadow ouptu shader
    int vVertexShadowOutput;
    int ModelViewProjectionMatrixShadowOutputUniform;
    int TextureUnitShadowOutputUniform;

    int NumberOfPoints;

    QQuaternion RotationQuaternion;

    void initializeGeometry();
    void initializeShaders();
    void initializeShadowShader();
    void initializeShadowOutputShader();
    void initializeCompassShader();
    void initializeFramebuffer();

    void generateStarGeometry(QVector<CompassGeometry> &triangles, Direction direction);

    void drawShadow();

//    void drawStar(Direction top);

private slots:
//    void updateOrientation();



};

#endif // CWGLCOMPASS_H

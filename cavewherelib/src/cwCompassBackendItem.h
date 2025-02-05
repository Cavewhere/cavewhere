/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#pragma once

#include <QQuickRhiItem>
#include <QBindable>
#include <QProperty>
#include <QQuaternion>
#include <QPointF>
#include <QPropertyChangeHandler>

class cwCompassBackendItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CompassBackendItem)

    // Properties with READ, WRITE, and BINDABLE accessors
    Q_PROPERTY(QQuaternion compassRotation READ compassRotation WRITE setCompassRotation BINDABLE bindableCompassRotation)
    Q_PROPERTY(QPointF labelNorthPosition READ labelNorthPosition BINDABLE bindableLabelNorthPosition)
    Q_PROPERTY(QPointF labelSouthPosition READ labelSouthPosition BINDABLE bindableLabelSouthPosition)
    Q_PROPERTY(QPointF labelEastPosition READ labelEastPosition BINDABLE bindableLabelEastPosition)
    Q_PROPERTY(QPointF labelWestPosition READ labelWestPosition BINDABLE bindableLabelWestPosition)

public:
    explicit cwCompassBackendItem(QQuickItem* parent = nullptr);

    // compassRotation property
    QQuaternion compassRotation() const { return m_rotation; }
    void setCompassRotation(const QQuaternion& rotation) { qDebug() << "Setting rotation data"; m_rotation = rotation; }
    QBindable<QQuaternion> bindableCompassRotation() { return &m_rotation; }

    // labelNorthPosition property
    QPointF labelNorthPosition() const { return m_labelNorthPosition; }
    QBindable<QPointF> bindableLabelNorthPosition() { return &m_labelNorthPosition; }

    // labelSouthPosition property
    QPointF labelSouthPosition() const { return m_labelSouthPosition; }
    QBindable<QPointF> bindableLabelSouthPosition() { return &m_labelSouthPosition; }

    // labelEastPosition property
    QPointF labelEastPosition() const { return m_labelEastPosition; }
    QBindable<QPointF> bindableLabelEastPosition() { return &m_labelEastPosition; }

    // labelWestPosition property
    QPointF labelWestPosition() const { return m_labelWestPosition; }
    QBindable<QPointF> bindableLabelWestPosition() { return &m_labelWestPosition; }


// signals:
    // void compassRotationChanged();

    // // Individual NOTIFY signals for each label position
    // void labelNorthPositionChanged();
    // void labelSouthPositionChanged();
    // void labelEastPositionChanged();
    // void labelWestPositionChanged();

protected:
    QQuickRhiItemRenderer* createRenderer() override;

    // void componentComplete() override;

private:
    void updateLabelPositions();
    QPointF computeLabelPosition(const QVector3D& pos);

    // Bindable properties using Q_OBJECT_BINDABLE_PROPERTY
    Q_OBJECT_BINDABLE_PROPERTY(cwCompassBackendItem, QQuaternion, m_rotation)
    Q_OBJECT_BINDABLE_PROPERTY(cwCompassBackendItem, QPointF, m_labelNorthPosition)
    Q_OBJECT_BINDABLE_PROPERTY(cwCompassBackendItem, QPointF, m_labelSouthPosition)
    Q_OBJECT_BINDABLE_PROPERTY(cwCompassBackendItem, QPointF, m_labelEastPosition)
    Q_OBJECT_BINDABLE_PROPERTY(cwCompassBackendItem, QPointF, m_labelWestPosition)

    QPropertyNotifier m_updateNotifier;
};


// /**
//  * @brief The cwGLCompass class
//  *
//  * This draws 3d compass on the screen.
//  */
// class cwCompassBackendItem : public QQuickPaintedItem, private QOpenGLFunctions
// {
//     Q_OBJECT
//     QML_NAMED_ELEMENT(CompassItem)

//     Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
//     Q_PROPERTY(QQuaternion rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
//     Q_PROPERTY(cwShaderDebugger* shaderDebugger READ shaderDebugger WRITE setShaderDebugger NOTIFY shaderDebuggerChanged)

// public:
//     explicit cwCompassBackendItem(QQuickItem *parent = 0);

//     cwCamera* camera() const;
//     void setCamera(cwCamera* camera);

//     cwShaderDebugger* shaderDebugger() const;
//     void setShaderDebugger(cwShaderDebugger* shaderDebugger);

//     void initialize();
//     void draw();

//     QQuaternion rotation() const;
//     void setRotation(QQuaternion quaternion);
//     QQuaternion modelView() const;

//     void paint(QPainter *painter);
//     void releaseResources();

// signals:
//     void cameraChanged();
//     void rotationChanged();
//     void shaderDebuggerChanged();

// public slots:
    
// private:

//     class CompassGeometry {
//     public:
//         CompassGeometry() {}
//         CompassGeometry(QVector3D position, QVector3D color) :
//             Position(position),
//             Color(color)
//         {
//         }

//         QVector3D Position;
//         QVector3D Color;
//     };

//     enum Direction {
//         Top,
//         Bottom
//     };

//     cwCamera* Camera;
//     cwShaderDebugger* ShaderDebugger; //!<

//     QOpenGLShaderProgram* Program;
//     QOpenGLShaderProgram* XShadowProgram;
//     QOpenGLShaderProgram* YShadowProgram;
//     QOpenGLShaderProgram* ShadowOutputProgram;
//     QOpenGLBuffer CompassVertexBuffer;
//     QOpenGLBuffer TextureGeometryBuffer;

//     QOpenGLFramebufferObject* CompassFramebuffer; //Can be a multi-sample or texture framebuffer
//     QOpenGLFramebufferObject* ShadowBufferFramebuffer;
//     QOpenGLFramebufferObject* HorizonalShadowBufferFramebuffer;

// //    QOpenGLBuffer CompassIndexes;
//     bool Initialized;

//     //Shader attributes for the normal compass shader
//     int vVertex;
//     int vColor;
//     int ModelViewProjectionMatrixUniform;
//     int IgnoreColorUniform;

//     //Shader attributes for the shadow compass shader
//     int vVertexShadow;
//     int ModelViewProjectionMatrixShadowUniform;
//     int TextureUnitShadow;

//     //Shader attirbutes for the shadow ouptu shader
//     int vVertexShadowOutput;
//     int ModelViewProjectionMatrixShadowOutputUniform;
//     int TextureUnitShadowOutputUniform;

//     int NumberOfPoints;

//     QQuaternion Rotation;

//     void initializeGeometry();
//     void initializeShaders();
//     void initializeShadowShader();
//     void initializeShadowOutputShader();
//     void initializeCompassShader();
//     void initializeFramebuffer();

//     void generateStarGeometry(QVector<CompassGeometry> &triangles, Direction direction);

//     void drawShadow();
//     void drawCompass(QOpenGLFramebufferObject *framebuffer, bool withColors, QMatrix4x4 modelView = QMatrix4x4());
//     void drawFramebuffer(QOpenGLFramebufferObject *framebuffer, QMatrix4x4 modelView = QMatrix4x4());
//     void labelCompass(QMatrix4x4 modelView);
//     void drawLabel(QVector3D pos, QString label, QFont font, QPainter* painter, cwCamera* camera);

// //    void drawStar(Direction top);

// private slots:
// //    void updateOrientation();



// };

// /**
// Gets camera
// */
// inline cwCamera* cwCompassItem::camera() const {
//     return Camera;
// }

// /**
// Gets rotation
// */
// inline QQuaternion cwCompassItem::rotation() const {
//     return Rotation;
// }

// /**
// Gets shaderDebugger
// */
// inline cwShaderDebugger* cwCompassItem::shaderDebugger() const {
//     return ShaderDebugger;
// }

// #endif // CWGLCOMPASS_H

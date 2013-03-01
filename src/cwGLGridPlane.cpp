//Our includes
#include "cwGLGridPlane.h"
#include "cwGLShader.h"
#include "cwGlobals.h"
#include "cwGlobalDirectory.h"
#include "cwShaderDebugger.h"
#include "cwCamera.h"

//Qt includes
#include <QVector3D>

//Std includes
#include <math.h>

cwGLGridPlane::cwGLGridPlane(QObject* parent) :
    cwGLObject(parent),
    Plane(QPlane3D(QVector3D(0.0, 0.0, -50.0), QVector3D(0.0, 0.0, 1.0))),
    Extent(10000.0),
    Program(NULL)
{
    updateModelMatrix();
}

void cwGLGridPlane::initialize()
{
    initializeShaders();
    initializeGeometry();

    vVertex = Program->attributeLocation("vVertex");

}

void cwGLGridPlane::draw() {
    Program->bind();

    Program->setUniformValue(UniformModelViewProjectionMatrix, camera()->viewProjectionMatrix() * ModelMatrix);
    Program->setUniformValue(UniformModelMatrix, ModelMatrix);

    TriangleVertexBuffer.bind();

    Program->setAttributeArray(vVertex, GL_FLOAT, NULL, 3);
    Program->enableAttributeArray(vVertex);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    TriangleVertexBuffer.release();

    Program->disableAttributeArray(vVertex);
    Program->release();
}


void cwGLGridPlane::setPlane(QPlane3D plane) {
    if(plane != Plane) {
        plane = Plane;
        updateModelMatrix();
    }
}


/**
    Sets extent, the max rendering geometry of the plane
*/
void cwGLGridPlane::setExtent(double extent) {
    if(Extent != extent) {
        Extent = extent;
        updateModelMatrix();
        emit extentChanged();
    }
}

/**
 * @brief cwGLGridPlane::initilaizeGeometry
 *
 * Creates a simple geometry quad geometry around 0.0, 0.0, 0.0
 *
 * This should be classed after the opengl context has been initiziled
 * and should be called by the rendering thread
 */
void cwGLGridPlane::initializeGeometry() {
    QVector<QVector3D> points;
    points.resize(4);
    points[0] = QVector3D(-1.0, -1.0, 0.0);
    points[1] = QVector3D(-1.0, 1.0, 0.0);
    points[2] = QVector3D(1.0, -1.0, 0.0);
    points[3] = QVector3D(1.0, 1.0, 0.0);

    Q_ASSERT(points.size() == 4); //If this fails make sure you date draw with the correct number of points

    TriangleVertexBuffer.create();
    TriangleVertexBuffer.bind();
    TriangleVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    TriangleVertexBuffer.allocate(points.data(), points.size() * sizeof(QVector3D));
    TriangleVertexBuffer.release();
}

/**
  This initializes all the shaders for the line plot
  */
void cwGLGridPlane::initializeShaders() {
    cwGLShader* vertexShader = new cwGLShader(QOpenGLShader::Vertex);
    vertexShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/grid.vert");

    cwGLShader* fragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    fragmentShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/grid.frag");

    Program = new QOpenGLShaderProgram();
    Program->addShader(vertexShader);
    Program->addShader(fragmentShader);

    bool success = Program->link();
    if(!success) {
        qDebug() << "Linking errors:" << Program->log();
    }

    shaderDebugger()->addShaderProgram(Program);

    vVertex = Program->attributeLocation("vVertex");
    UniformModelViewProjectionMatrix = Program->uniformLocation("ModelViewProjectionMatrix");
    UniformModelMatrix = Program->uniformLocation("ModelMatrix");

    Program->setUniformValue("colorBG", Qt::gray);
}

/**
 * @brief cwGLGridPlane::updateModelMatrix
 *
 * Updates the model index for the grid plane
 */
void cwGLGridPlane::updateModelMatrix() {

    QMatrix4x4 modelMatrix;

    //Position
    modelMatrix.translate(Plane.origin());

    //Scale
    modelMatrix.scale(Extent);

//    //Rotation
//    QVector3D upVector(0.0, 0.0, 1.0);
//    QVector3D planeNormal = Plane.normal().normalized();

//    //Find the rotation from up vector
//    double rotation = acos(QVector3D::dotProduct(upVector, planeNormal)) * cwGlobals::RadiansToDegrees;

//    //Find the cross product between the vectors
//    QVector3D cross = QVector3D::crossProduct(upVector, planeNormal);

//    modelMatrix.rotate(rotation, cross);

    ModelMatrix = modelMatrix;
}

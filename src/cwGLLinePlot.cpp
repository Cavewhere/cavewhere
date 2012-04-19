//Std includes
#include <limits>

//Our includes
#include "cwGLLinePlot.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwCamera.h"
#include "cwGlobalDirectory.h"

cwGLLinePlot::cwGLLinePlot(QObject *parent) :
    cwGLObject(parent),
    ShaderProgram(NULL)
{
    MaxZValue = 0.0;
    MinZValue = 0.0;
    IndexBufferSize = 0;
}

void cwGLLinePlot::initialize() {
    initializeShaders();
    initializeBuffers();
}

/**
  This initializes all the shaders for the line plot
  */
void cwGLLinePlot::initializeShaders() {
    cwGLShader* linePlotVertexShader = new cwGLShader(QOpenGLShader::Vertex);
    linePlotVertexShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/LinePlot.vert");

    cwGLShader* linePlotFragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    linePlotFragmentShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/LinePlot.frag");

    ShaderProgram = new QOpenGLShaderProgram(this);
    ShaderProgram->addShader(linePlotVertexShader);
    ShaderProgram->addShader(linePlotFragmentShader);

    bool success = ShaderProgram->link();
    if(!success) {
        qDebug() << "Linking errors:" << ShaderProgram->log();
    }

    shaderDebugger()->addShaderProgram(ShaderProgram);

    vVertex = ShaderProgram->attributeLocation("vVertex");
    UniformModelViewProjectionMatrix = ShaderProgram->uniformLocation("ModelViewProjectionMatrix");
    UniformMaxZValue = ShaderProgram->uniformLocation("MaxZValue");
    UniformMinZValue = ShaderProgram->uniformLocation("MinZValue");

}

/**
  This initializes all the opengl buffers for the line plot
  */
void cwGLLinePlot::initializeBuffers() {
    //Setup the buffers
    LinePlotVertexBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    LinePlotIndexBuffer = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);

    LinePlotVertexBuffer.create();
    LinePlotIndexBuffer.create();

    LinePlotVertexBuffer.bind();
    LinePlotVertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    LinePlotVertexBuffer.release();

    LinePlotIndexBuffer.bind();
    LinePlotIndexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    LinePlotIndexBuffer.release();
}

void cwGLLinePlot::draw() {
    ShaderProgram->bind();

    ShaderProgram->setUniformValue(UniformModelViewProjectionMatrix, camera()->viewProjectionMatrix());
    ShaderProgram->enableAttributeArray(vVertex);

    LinePlotVertexBuffer.bind();
    LinePlotIndexBuffer.bind();

    ShaderProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 3);

    glDrawElements(GL_LINES, IndexBufferSize, GL_UNSIGNED_INT, NULL);

    LinePlotVertexBuffer.release();
    LinePlotIndexBuffer.release();

    ShaderProgram->release();
}

/**
  \brief Set the line points for the line plot object
  */
void cwGLLinePlot::setPoints(QVector<QVector3D> pointData) {
    if(ShaderProgram == NULL) { return; }

    //Find the max value and the min value
    MaxZValue = -std::numeric_limits<float>::max();
    MinZValue = std::numeric_limits<float>::max();

    for(int i = 0; i < pointData.size(); i++) {
        MaxZValue = qMax((qreal)MaxZValue, pointData[i].z());
        MinZValue = qMin((qreal)MinZValue, pointData[i].z());
    }

//    qDebug() << "MaxZValue: " << MaxZValue;
//    qDebug() << "MinZValue: " << MinZValue;

    LinePlotVertexBuffer.bind();
    LinePlotVertexBuffer.allocate(pointData.data(), pointData.size() * sizeof(QVector3D));
    LinePlotVertexBuffer.release();

    ShaderProgram->bind();
    ShaderProgram->setUniformValue(UniformMaxZValue, MaxZValue);
    ShaderProgram->setUniformValue(UniformMinZValue, MinZValue);
    ShaderProgram->release();
}

/**
  \brief Set the line indexes for the line plot object
  */
void cwGLLinePlot::setIndexes(QVector<unsigned int> indexData) {
    LinePlotIndexBuffer.bind();
    LinePlotIndexBuffer.allocate(indexData.data(), indexData.size() * sizeof(unsigned int));
    LinePlotIndexBuffer.release();

    IndexBufferSize = indexData.size();
}

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
    ShaderProgram(nullptr)
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

    ShaderProgram = new QOpenGLShaderProgram();
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
    LinePlotVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    LinePlotVertexBuffer.release();

    LinePlotIndexBuffer.bind();
    LinePlotIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    LinePlotIndexBuffer.release();
}

void cwGLLinePlot::draw() {
    if(Points.size() <= 0) { return; }

    glLineWidth(1.0);

    ShaderProgram->bind();

    ShaderProgram->setUniformValue(UniformModelViewProjectionMatrix, camera()->viewProjectionMatrix());
    ShaderProgram->enableAttributeArray(vVertex);

    LinePlotVertexBuffer.bind();
    LinePlotIndexBuffer.bind();

    ShaderProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 3);

    glDrawElements(GL_LINES, IndexBufferSize, GL_UNSIGNED_INT, nullptr);

    LinePlotVertexBuffer.release();
    LinePlotIndexBuffer.release();

    ShaderProgram->disableAttributeArray(vVertex);
    ShaderProgram->release();
}

/**
  \brief Set the line points for the line plot object
  */
void cwGLLinePlot::setPoints(QVector<QVector3D> pointData) {
    //Find the max value and the min value
    MaxZValue = -std::numeric_limits<float>::max();
    MinZValue = std::numeric_limits<float>::max();

    for(int i = 0; i < pointData.size(); i++) {
        MaxZValue = qMax(MaxZValue, (float)pointData[i].z());
        MinZValue = qMin(MinZValue, (float)pointData[i].z());
    }

    Points = pointData;
    markDataAsDirty();
}

/**
  \brief Set the line indexes for the line plot object
  */
void cwGLLinePlot::setIndexes(QVector<unsigned int> indexData) {
    Indexes = indexData;
    markDataAsDirty();
}

/**
 * @brief cwGLLinePlot::updateData
 *
 * This is called by the cwGLRenderer to update all the dataobject's that are dirty.
 *
 * This is called in updateScene and is thread safe
 */
void cwGLLinePlot::updateData() {
    cwGLObject::updateData();

    if(ShaderProgram == nullptr) { return; }

    LinePlotVertexBuffer.bind();
    LinePlotVertexBuffer.allocate(Points.data(), Points.size() * sizeof(QVector3D));
    LinePlotVertexBuffer.release();

    ShaderProgram->bind();
    ShaderProgram->setUniformValue(UniformMaxZValue, MaxZValue);
    ShaderProgram->setUniformValue(UniformMinZValue, MinZValue);
    ShaderProgram->release();

    LinePlotIndexBuffer.bind();
    LinePlotIndexBuffer.allocate(Indexes.data(), Indexes.size() * sizeof(unsigned int));
    LinePlotIndexBuffer.release();

    IndexBufferSize = Indexes.size();

    if(geometryItersecter() != nullptr) {
        geometryItersecter()->clear(this);

        //For geometry intersection
        cwGeometryItersecter::Object geometryObject(
                    this, //This object's pointer
                    0, //Id
                    Points,
                    Indexes,
                    cwGeometryItersecter::Lines);

        //FIXME: This could potentially slow down the rendering.
        geometryItersecter()->addObject(geometryObject);
    }
}

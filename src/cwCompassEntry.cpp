//Our includes
#include "cwCompassEntry.h"
#include "cwCompassBufferGenerator.h"

//Qt includes
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QGraphicsApiFilter>

using namespace Qt3DCore;
using namespace Qt3DRender;

cwCompassEntry::cwCompassEntry(Qt3DCore::QNode *parent) :
    QEntity(parent)
{
    //This buffer contain all the point and color data
    QBuffer* buffer = new QBuffer();
    buffer->setDataGenerator(QBufferDataGeneratorPtr(new cwCompassBufferGenerator()));

    QAttribute* pointAttribute = new QAttribute();
    pointAttribute->setAttributeType(QAttribute::VertexAttribute);
    pointAttribute->setDataSize(3);
    pointAttribute->setDataType(QAttribute::Float);
    pointAttribute->setByteOffset(0);
    pointAttribute->setBuffer(buffer);
    pointAttribute->buffer()->setType(Qt3DRender::QBuffer::VertexBuffer);
    pointAttribute->setName("vertexPosition");

    QAttribute* colorAttribute = new QAttribute();
    colorAttribute->setAttributeType(QAttribute::VertexAttribute);
    colorAttribute->setDataSize(3);
    colorAttribute->setDataType(QAttribute::Float);
    colorAttribute->setByteOffset(sizeof(QVector3D));
    colorAttribute->setBuffer(buffer);
    colorAttribute->buffer()->setType(Qt3DRender::QBuffer::VertexBuffer);
    colorAttribute->setName("color");

    QGeometry* geometry = new QGeometry();
    geometry->addAttribute(pointAttribute);
    geometry->addAttribute(colorAttribute);

    QGeometryRenderer* geometryRenderer = new QGeometryRenderer();
    geometryRenderer->setPrimitiveType(QGeometryRenderer::Triangles);
    geometryRenderer->setGeometry(geometry);

    QEffect* effect = new QEffect();

    // Create technique, render pass and shader
    QTechnique *gl3Technique = new QTechnique();
    QRenderPass *gl3Pass = new QRenderPass();
    QShaderProgram *glShader = new QShaderProgram();

    // Set the shader on the render pass
    gl3Pass->setShaderProgram(glShader);

    // Add the pass to the technique
    gl3Technique->addRenderPass(gl3Pass);

    // Set the targeted GL version for the technique
    gl3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    gl3Technique->graphicsApiFilter()->setMajorVersion(3);
    gl3Technique->graphicsApiFilter()->setMinorVersion(1);
    gl3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    // Add the technique to the effect
    effect->addTechnique(gl3Technique);

    addComponent(geometryRenderer);
}

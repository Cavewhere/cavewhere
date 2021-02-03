//Our includes
#include "cwScrapEntity.h"
#include "cwScrap.h"
#include "cwTextureImage.h"
#include "cwTextureUploadTask.h"
#include "cwTexture.h"

//Qt includes
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <QVector3D>
#include <QVector2D>
using namespace Qt3DCore;
using namespace Qt3DRender;

//Async Future
#include "asyncfuture.h"

cwScrapEntity::cwScrapEntity(Qt3DCore::QNode* parent) :
        QEntity(parent),
        GeometryRenderer(new QGeometryRenderer(this)),
        Material(new QMaterial(this))
{

    QAttribute* pointAttribute = new QAttribute(this);
    pointAttribute->setAttributeType(QAttribute::VertexAttribute);
    pointAttribute->setDataSize(3);
    pointAttribute->setDataType(QAttribute::Float);
    pointAttribute->setByteOffset(0);
    pointAttribute->setBuffer(new Qt3DRender::QBuffer());
    pointAttribute->buffer()->setType(Qt3DRender::QBuffer::VertexBuffer);
    pointAttribute->setName("vertexPosition");

    QAttribute* texCoordAttribute = new QAttribute(this);
    texCoordAttribute->setAttributeType(QAttribute::VertexAttribute);
    texCoordAttribute->setDataSize(2);
    texCoordAttribute->setDataType(QAttribute::Float);
    texCoordAttribute->setByteOffset(0);
    texCoordAttribute->setBuffer(new Qt3DRender::QBuffer());
    texCoordAttribute->buffer()->setType(Qt3DRender::QBuffer::VertexBuffer);
    texCoordAttribute->setName("scrapTexCoord");

    QAttribute* indexAttribute = new QAttribute(this);
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    indexAttribute->setDataType(QAttribute::UnsignedInt);
    indexAttribute->setBuffer(new Qt3DRender::QBuffer());
    indexAttribute->buffer()->setType(Qt3DRender::QBuffer::IndexBuffer);

    //Create geometry
    QGeometry* geometry = new QGeometry(this);
    geometry->addAttribute(pointAttribute);
    geometry->addAttribute(texCoordAttribute);
    geometry->addAttribute(indexAttribute);
    GeometryRenderer->setGeometry(geometry);


    ScrapTexture = new cwTexture(this);

    auto scrapTextureParameter = new QParameter("scrapTexture", nullptr); //ScrapTexture->texture());

    Material->addParameter(scrapTextureParameter);
    Material->addParameter(new QParameter("texCoordsScale", QVector2D(1.0, 1.0)));

    connect(ScrapTexture, &cwTexture::textureChanged, this, [scrapTextureParameter, this]() {
        scrapTextureParameter->setValue(QVariant::fromValue(ScrapTexture->texture()));
    });

    //Setup what type we are drawing
    GeometryRenderer->setPrimitiveType(QGeometryRenderer::Triangles);

    addComponent(GeometryRenderer);
    addComponent(Material);
}

/**
* @brief cwScrapEntity::scrap
* @return
*/
cwScrap* cwScrapEntity::scrap() const {
    return Scrap;
}

/**
* @brief cwScrapEntity::setScrap
* @param scrap
*/
void cwScrapEntity::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {

        if(!Scrap.isNull()) {
            disconnect(Scrap.data(), 0, this, 0);
        }

        Scrap = scrap;

        if(!Scrap.isNull()) {
            connect(Scrap.data(), &cwScrap::triangulationDataChange, this, &cwScrapEntity::updateGeometry);
            updateGeometry();
        }

        emit scrapChanged();
    }
}

/**
* @brief cwScrapEntity::material
* @return
*/
Qt3DRender::QMaterial* cwScrapEntity::material() const {
    return Material;
}

/**
 * @brief cwScrapEntity::updateGeometry
 */
void cwScrapEntity::updateGeometry()
{
    if(!Scrap.isNull()) {

        auto triangleData = Scrap->triangulationData();
        Points = triangleData.points();
        TexCoords = triangleData.texCoords();
        Indices = triangleData.indices();

        if(!Points.isEmpty() && !TexCoords.isEmpty() && !Indices.isEmpty()) {
            auto geometry = GeometryRenderer->geometry();
            auto pointAttribute = geometry->attributes().at(Point);
            auto texCoordAttribute = geometry->attributes().at(TexCoord);
            auto indexAttribute = geometry->attributes().at(Index);

            const char* points = reinterpret_cast<const char*>(&Points[0]);
            int size = Points.size() * sizeof(QVector3D);
            pointAttribute->buffer()->setData(QByteArray(points, size)); //shallow copy

            const char* texCoords = reinterpret_cast<const char*>(&TexCoords[0]);
            size = TexCoords.size() * sizeof(QVector2D);
            texCoordAttribute->buffer()->setData(QByteArray(texCoords, size)); //shallow copy

            const char* indexes = reinterpret_cast<const char*>(&Indices[0]);
            size = Indices.size() * sizeof(unsigned int);
            indexAttribute->buffer()->setData(QByteArray(indexes, size)); //shallow copy
            indexAttribute->setCount(Indices.size());

            updateTexture(triangleData.croppedImage());
        }
    }
}

/**
 * This sets up the texture to load all the mipmaps
 */
void cwScrapEntity::updateTexture(const cwImage &image)
{
    ScrapTexture->setImage(image);
}

/**
* @brief cwScrapEntity::setProject
* @param project
*/
void cwScrapEntity::setProject(QString project) {
    if(this->project() != project) {
        Project = project;
        ScrapTexture->setProject(project);
        emit projectChanged();
    }
}

/**
* @brief cwScrapEntity::project
* @return
*/
QString cwScrapEntity::project() const {
    return Project;
}

/**
* @brief class::effect
* @return
*/
Qt3DRender::QEffect* cwScrapEntity::effect() const {
    return Effect;
}

/**
* @brief class::setEffect
* @param effect
*/
void cwScrapEntity::setEffect(Qt3DRender::QEffect* effect) {
    if(Effect != effect) {
        Effect = effect;
        Material->setEffect(Effect);
        emit effectChanged();
    }
}

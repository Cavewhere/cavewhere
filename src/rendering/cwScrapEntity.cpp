//Our includes
#include "cwScrapEntity.h"
#include "cwScrap.h"

//Qt includes
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <QVector3D>
#include <QVector2D>
using namespace Qt3DCore;
using namespace Qt3DRender;

cwScrapEntity::cwScrapEntity(Qt3DCore::QNode* parent) :
        QEntity(parent),
        GeometryRenderer(new QGeometryRenderer()),
        Material(nullptr)
{

    QAttribute* pointAttribute = new QAttribute();
    pointAttribute->setAttributeType(QAttribute::VertexAttribute);
    pointAttribute->setDataSize(3);
    pointAttribute->setDataType(QAttribute::Float);
    pointAttribute->setByteOffset(0);
    pointAttribute->setBuffer(new Qt3DRender::QBuffer());
    pointAttribute->setName("vertexPosition");

    QAttribute* texCoordAttribute = new QAttribute();
    texCoordAttribute->setAttributeType(QAttribute::VertexAttribute);
    texCoordAttribute->setDataSize(2);
    texCoordAttribute->setDataType(QAttribute::Float);
    texCoordAttribute->setByteOffset(0);
    texCoordAttribute->setBuffer(new Qt3DRender::QBuffer());
    texCoordAttribute->setName("texCoord");

    QAttribute* indexAttribute = new QAttribute();
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    indexAttribute->setDataType(QAttribute::UnsignedInt);
    indexAttribute->setBuffer(new Qt3DRender::QBuffer());

    //Create geometry
    QGeometry* geometry = new QGeometry();
    geometry->addAttribute(pointAttribute);
    geometry->addAttribute(texCoordAttribute);
    geometry->addAttribute(indexAttribute);
    GeometryRenderer->setGeometry(geometry);

    //Setup what type we are drawing
    GeometryRenderer->setPrimitiveType(QGeometryRenderer::Triangles);

    addComponent(GeometryRenderer);
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
* @brief cwScrapEntity::setMaterial
* @param material
*/
void cwScrapEntity::setMaterial(Qt3DRender::QMaterial* material) {
    if(Material != material) {

        if(Material != nullptr) {
            removeComponent(Material);
        }

        Material = material;

        if(Material != nullptr) {
            qDebug() << "Adding material:" << Material;
            addComponent(Material);
        }

        emit materialChanged();
    }
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

        qDebug() << "Update geometry" << Scrap << Points.size() << TexCoords.size() << Indices.size();

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

        }
    }
}

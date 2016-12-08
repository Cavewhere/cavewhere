#ifndef CWSCRAPENTITY_H
#define CWSCRAPENTITY_H

//Qt includes
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <QPointer>
#include <QVector>
#include <QVector3D>
#include <QVector2D>

//Our includes
class cwScrap;

class cwScrapEntity : public Qt3DCore::QEntity
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(Qt3DRender::QMaterial* material READ material WRITE setMaterial NOTIFY materialChanged)

public:
    cwScrapEntity(Qt3DCore::QNode* parent = nullptr);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    Qt3DRender::QMaterial* material() const;
    void setMaterial(Qt3DRender::QMaterial* material);

signals:
    void scrapChanged();
    void materialChanged();

private slots:
    void updateGeometry();

private:
    enum {
        Point,
        TexCoord,
        Index
    };

    QPointer<cwScrap> Scrap;
    Qt3DRender::QGeometryRenderer* GeometryRenderer;
    Qt3DRender::QMaterial* Material; //!<

    QVector<QVector3D> Points;
    QVector<QVector2D> TexCoords;
    QVector<uint> Indices;
};

#endif // CWSCRAPENTITY_H

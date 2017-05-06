#ifndef CWSCRAPENTITY_H
#define CWSCRAPENTITY_H

//Qt includes
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QEffect>
#include <QPointer>
#include <QVector>
#include <QVector3D>
#include <QVector2D>

//Our includes
class cwScrap;
class cwTextureImage;

class cwScrapEntity : public Qt3DCore::QEntity
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(Qt3DRender::QEffect* effect READ effect WRITE setEffect NOTIFY effectChanged)
    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged)


public:
    cwScrapEntity(Qt3DCore::QNode* parent = nullptr);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    QString project() const;
    void setProject(QString project);

//    Qt3DRender::QTechnique* technique() const;
//    void setTechnique(Qt3DRender::QTechnique* technique);

    Qt3DRender::QEffect* effect() const;
    void setEffect(Qt3DRender::QEffect* effect);

    Qt3DRender::QMaterial* material() const;
//    void setMaterial(Qt3DRender::QMaterial* material);

signals:
    void scrapChanged();
    void effectChanged();
    void materialChanged();
    void techniqueChanged();
    void projectChanged();

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

    Qt3DRender::QTechnique* Technique; //!<
    Qt3DRender::QMaterial* Material; //!<
    Qt3DRender::QTexture2D* ScrapTexture;
    cwTextureImage* ScrapTextureImage;
    Qt3DRender::QEffect* Effect; //!<

    QVector<QVector3D> Points;
    QVector<QVector2D> TexCoords;
    QVector<uint> Indices;
};





#endif // CWSCRAPENTITY_H

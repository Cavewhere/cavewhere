#ifndef CWSCRAPESENTITY_H
#define CWSCRAPESENTITY_H

//Qt includes
#include <QEntity>
#include <Qt3DRender/QMaterial>

//Cavewhere includes
class cwScrap;
class cwScrapEntity;

class cwScrapsEntity : public Qt3DCore::QEntity
{
    Q_OBJECT

    Q_PROPERTY(Qt3DRender::QMaterial* material READ material WRITE setMaterial NOTIFY materialChanged)

public:
    cwScrapsEntity(QNode* parent = nullptr);

    Qt3DRender::QMaterial* material() const;
    void setMaterial(Qt3DRender::QMaterial* material);

    void addScrap(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

signals:
    void materialChanged();

private:
    Qt3DRender::QMaterial* Material; //!<
    QHash<cwScrap*, cwScrapEntity*> ScrapToEntity;

};

#endif // CWSCRAPESENTITY_H

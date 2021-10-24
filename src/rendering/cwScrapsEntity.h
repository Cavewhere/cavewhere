#ifndef CWSCRAPESENTITY_H
#define CWSCRAPESENTITY_H

//Qt includes
#include <QEntity>
#include <Qt3DRender/QEffect>

//Cavewhere includes
class cwScrap;
class cwScrapEntity;
class cwKeywordItemModel;
#include "cwFutureManagerToken.h"

class cwScrapsEntity : public Qt3DCore::QEntity
{
    Q_OBJECT

    Q_PROPERTY(Qt3DRender::QEffect* effect READ effect WRITE setEffect NOTIFY effectChanged)
    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)

public:
    cwScrapsEntity(QNode* parent = nullptr);

    Qt3DRender::QEffect* effect() const;
    void setEffect(Qt3DRender::QEffect* effect);

    QString project() const;
    void setProject(QString project);

    void addScrap(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    void setFutureManagerToken(const cwFutureManagerToken& token);

signals:
    void effectChanged();
    void projectChanged();
    void keywordItemModelChanged();

private:
    Qt3DRender::QEffect* Effect; //!<
    QHash<cwScrap*, cwScrapEntity*> ScrapToEntity;
    QString Project; //!<
    cwKeywordItemModel* KeywordItemModel = nullptr; //!<
    cwFutureManagerToken FutureToken;

};

/**
* @brief cwScrapEntity::project
* @return
*/
inline QString cwScrapsEntity::project() const {
    return Project;
}

/**
*
*/
inline cwKeywordItemModel* cwScrapsEntity::keywordItemModel() const {
    return KeywordItemModel;
}

#endif // CWSCRAPESENTITY_H

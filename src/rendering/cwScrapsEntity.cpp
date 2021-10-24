//Our inculdes
#include "cwScrapsEntity.h"
#include "cwScrapEntity.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"
#include "cwScrap.h"

using namespace Qt3DCore;
using namespace Qt3DRender;

cwScrapsEntity::cwScrapsEntity(Qt3DCore::QNode* parent) :
    QEntity(parent),
    Effect(nullptr)
{

}

/**
 * @brief cwScrapsEntity::addScrap
 * @param scrap
 */
void cwScrapsEntity::addScrap(cwScrap* scrap)
{
    if(!ScrapToEntity.contains(scrap)) {
        cwScrapEntity* entity = new cwScrapEntity();
        entity->setScrap(scrap);
        entity->setEffect(Effect);
        entity->setProject(Project);

        if(keywordItemModel()) {
            auto keywordItem = new cwKeywordItem();
            keywordItem->setObject(entity);
            keywordItem->keywordModel()->addExtension(scrap->keywordModel());
            keywordItemModel()->addItem(keywordItem);
        }

        ScrapToEntity.insert(scrap, entity);

        //This is a node traverse work around, Qt3D crashes when cwScrapEntity parent
        //is assigned in the constructor. The scrap entity needs to be completely created
        //and the assigned.
        entity->setParent(this);
    }
}

/**
 * @brief cwScrapsEntity::removeScrap
 * @param scrap
 */
void cwScrapsEntity::removeScrap(cwScrap* scrap)
{
    if(ScrapToEntity.contains(scrap)) {
        auto entity = ScrapToEntity.value(scrap);
        entity->deleteLater();
        ScrapToEntity.remove(scrap);
    }
}

/**
* @brief cwScrapsE::effect
* @return
*/
Qt3DRender::QEffect* cwScrapsEntity::effect() const {
    return Effect;
}

/**
* @brief cwScrapsE::setEffect
* @param effect
*/
void cwScrapsEntity::setEffect(Qt3DRender::QEffect* effect) {
    if(Effect != effect) {
        Effect = effect;

        for(auto iter = ScrapToEntity.begin(); iter != ScrapToEntity.end(); iter++) {
            auto entity = iter.value();
            entity->setEffect(Effect);
        }

        emit effectChanged();
    }
}

/**
* @brief cwScrapEntity::setProject
* @param project
*/
void cwScrapsEntity::setProject(QString project) {
    if(Project != project) {
        Project = project;

        for(auto iter = ScrapToEntity.begin(); iter != ScrapToEntity.end(); iter++) {
            auto entity = iter.value();
            entity->setProject(Project);
        }

        emit projectChanged();
    }
}

/**
*
*/
void cwScrapsEntity::setKeywordItemModel(cwKeywordItemModel* keywordItemModel) {
    if(KeywordItemModel != keywordItemModel) {
        KeywordItemModel = keywordItemModel;
        emit keywordItemModelChanged();
    }
}


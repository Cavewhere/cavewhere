//Our inculdes
#include "cwScrapsEntity.h"
#include "cwScrapEntity.h"

using namespace Qt3DCore;
using namespace Qt3DRender;

cwScrapsEntity::cwScrapsEntity(Qt3DCore::QNode* parent) :
    QEntity(parent),
    Material(nullptr)
{

}

/**
 * @brief cwScrapsEntity::addScrap
 * @param scrap
 */
void cwScrapsEntity::addScrap(cwScrap* scrap)
{
    if(!ScrapToEntity.contains(scrap)) {
        cwScrapEntity* entity = new cwScrapEntity(this);
        entity->setScrap(scrap);
        entity->setMaterial(Material);
        ScrapToEntity.insert(scrap, entity);
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
* @brief cwScrapsE::material
* @return
*/
Qt3DRender::QMaterial* cwScrapsEntity::material() const {
    return Material;
}

/**
* @brief cwScrapsE::setMaterial
* @param material
*/
void cwScrapsEntity::setMaterial(Qt3DRender::QMaterial* material) {
    if(Material != material) {
        Material = material;

        for(auto iter = ScrapToEntity.begin(); iter != ScrapToEntity.end(); iter++) {
            auto entity = iter.value();
            entity->setMaterial(Material);
        }

        emit materialChanged();
    }
}

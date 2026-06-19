/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLeadView.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwScrap.h"
#include "cwSelectionManager.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwRayHit.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLeadVisibility.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>
#include <QMatrix4x4>
#include <QRay3D>

namespace {
// Pull the lead marker toward the eye along the line of sight (screen position
// and size unaffected) so local scrap/centerline geometry near the lead doesn't
// draw over it. Same rationale and value as the station-label bias. The click
// occlusion test (isOccluded) reuses this as its slack so a click is rejected
// only when geometry is more than this far in front of the lead — i.e. exactly
// when the drawn marker is hidden too. A lead sits on its scrap surface, so a
// generous bias keeps that surface from reporting the lead as self-occluded.
constexpr float kLeadDepthBias = 1.0f;
}

cwLeadView::cwLeadView(QQuickItem *parent) :
    cwBillboardOverlayItem(parent),
    SelectionMananger(new cwSelectionManager(this))
{
}

/**
* @brief cwLeadView::regionModel
* @return The regionModel that this leadView looks for leads in
*/
cwRegionTreeModel* cwLeadView::regionModel() const {
    return RegionModel;
}

/**
* @brief cwLeadView::setRegionModel
* @param regionModel
*
* Sets the region model that is used to listen to scraps being added or removed
*/
void cwLeadView::setRegionModel(cwRegionTreeModel* regionModel) {
    if(RegionModel != regionModel) {
        if(!RegionModel.isNull()) {
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadView::scrapsAdded);
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadView::scrapsRemoved);
        }

        RegionModel = regionModel;

        if(!RegionModel.isNull()) {
            m_currentScrapId = 0; //Reset the scrap id, this is useful for the qml testcase to work correctly

            connect(RegionModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadView::scrapsAdded);
            connect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadView::scrapsRemoved);

            QList<cwScrap*> scraps = RegionModel->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);
            for(auto scrap : std::as_const(scraps)) {
                addScrap(scrap);
            }
        }

        emit regionModelChanged();
    }
}

/**
 * @brief cwLeadView::addScrap
 * @param scrap - Adds the scrap to the LeadView
 */
void cwLeadView::addScrap(cwScrap *scrap)
{
    Q_ASSERT(scrap != nullptr);

    // A region-model reset (project load/checkout) emits beginResetModel/
    // endResetModel rather than per-row removals, so a scrap can still be tracked
    // here when its slot is reused or the scrap is re-added. Rebuild cleanly
    // instead of double-inserting (which previously hit an assert).
    if(m_leadItems.contains(scrap)) {
        removeScrap(scrap);
    }

    auto itemAt = [scrap](const auto& items, int i) {
        Q_ASSERT(dynamic_cast<QQuickItem*>(items[i]));
        return static_cast<QQuickItem*>(items[i]);
    };

    auto updatePosition = [scrap](QQuickItem* item, int i) {
        auto position = scrap->leadData(cwScrap::LeadPosition, i).value<QVector3D>();
        item->setProperty("position3D", position);
    };

    auto updatePositions = [scrap, this, updatePosition, itemAt](int begin, int end) {
        // find() (not value()) — ScrapEntry is move-only now, so a copy won't compile.
        const auto entryIt = m_leadItems.find(scrap);
        if(entryIt == m_leadItems.end()) { return; }
        const ScrapEntry& entry = entryIt->second;
        for(int i = begin; i <= end; i++) {
            auto item = itemAt(entry.items, i);
            updatePosition(item, i);
        }
    };

    auto updateIndexesToEnd = [scrap, itemAt, this](int begin) {
        auto& entry = m_leadItems[scrap];
        for(int i = begin; i < entry.items.size(); i++) {
            auto item = itemAt(entry.items, i);
            item->setProperty("pointIndex", i);
        }
    };

    auto beginInsert = [this, scrap, updateIndexesToEnd, updatePosition](int begin, int end) {

        if(begin <= end) {
            auto& entry = m_leadItems[scrap];

            for(int i = begin; i <= end; i++) {
                //Create the item
                auto item = createItem();
                if(item == nullptr) {
                    //createItem already logged the compile failure; skip rather
                    //than dereference null below.
                    continue;
                }
                entry.items.insert(i, item);
                entry.billboardHandles.emplace(entry.billboardHandles.begin() + i);

                item->setProperty("scrap", QVariant::fromValue(scrap));
                item->setProperty("scrapId", entry.scrapLeadId);

                // A lead created while a filter already hides this scrap's leads
                // must start hidden, or it flashes in before the next filter pass.
                item->setVisible(entry.keywordVisible);
            }

            updateIndexesToEnd(begin);
        }
    };

    auto removeItems = [this, scrap, updateIndexesToEnd](int begin, int end) {
        auto& entry = m_leadItems[scrap];
        if(entry.items.isEmpty()) { return; }
        if(begin > end) { return; }
        if(begin < 0) { return; }
        if(end >= entry.items.size()) { return; }

        for(int index = end; index >= begin; index--) {
            SelectionMananger->clear();
            entry.items.at(index)->deleteLater();
            entry.items.removeAt(index);
            // Erasing the handle removes its billboard from the shared layer.
            entry.billboardHandles.erase(entry.billboardHandles.begin() + index);
        }

        updateIndexesToEnd(begin);
    };

    auto insert = [this, scrap, updatePosition, itemAt](int begin, int end) {
        auto& entry = m_leadItems[scrap];

        for(int i = begin; i <= end; i++) {
            auto item = itemAt(entry.items, i);
            updatePosition(item, i);
        }
    };

    auto resetScrap = [this, scrap, itemAt, updatePosition, beginInsert, insert, removeItems]() {
        auto& entry = m_leadItems[scrap];
        int currentItemCount = entry.items.size();
        int expectedLeadCount = scrap->numberOfLeads();

        if(currentItemCount < expectedLeadCount) {
            beginInsert(currentItemCount, expectedLeadCount - 1);
            insert(currentItemCount, expectedLeadCount - 1);
        } else if(currentItemCount > expectedLeadCount) {
            removeItems(expectedLeadCount, currentItemCount - 1);
        }
        Q_ASSERT(entry.items.size() == expectedLeadCount);

        for(int i = 0; i < expectedLeadCount; i++) {
            auto item = itemAt(entry.items, i);
            item->setProperty("pointIndex", i);
            updatePosition(item, i);
        }
    };

    connect(scrap, &cwScrap::leadsBeginInserted, this, beginInsert);
    connect(scrap, &cwScrap::leadsInserted, this, insert);
    connect(scrap, &cwScrap::leadsRemoved, this, removeItems);

    connect(scrap, &cwScrap::leadsDataChanged, this, [this, scrap, updatePositions](int begin, int end, const QList<int>& roles) {
        if(roles.contains(cwScrap::LeadPosition)) {
            updatePositions(begin, end);
            updateItemPositions();
        }
    });

    connect(scrap, &cwScrap::leadsReset, this, resetScrap);

    // Reproject + (re)register billboards after the model mutates the item list.
    // These run after the lambdas above (connection order), so position3D is set.
    connect(scrap, &cwScrap::leadsInserted, this, &cwLeadView::updateItemPositions);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwLeadView::updateItemPositions);
    connect(scrap, &cwScrap::leadsReset, this, &cwLeadView::updateItemPositions);

    // Register/unregister the scrap's keyword item as its lead count crosses 0.
    // These run last (after the item list reflects the change), so updateKeywordItem
    // sees the correct count.
    connect(scrap, &cwScrap::leadsInserted, this, [this, scrap]() { updateKeywordItem(scrap); });
    connect(scrap, &cwScrap::leadsRemoved, this, [this, scrap]() { updateKeywordItem(scrap); });
    connect(scrap, &cwScrap::leadsReset, this, [this, scrap]() { updateKeywordItem(scrap); });

    //Setup the leads. operator[] default-constructs in place (no copy), since the
    //entry is move-only now; insert() would try to copy the braced temporary.
    m_leadItems[scrap].scrapLeadId = ++m_currentScrapId;

    auto lastIndex = scrap->numberOfLeads() - 1;
    beginInsert(0, lastIndex);
    insert(0, lastIndex);
    updateItemPositions();
    updateKeywordItem(scrap);
}

/**
 * @brief cwLeadView::removeScrap
 * @param scrap - Removes the scrap from the lead view
 */
void cwLeadView::removeScrap(cwScrap *scrap)
{
    Q_ASSERT(scrap != nullptr);

    // Drop the per-scrap lambda connections set up in addScrap; otherwise a
    // re-add of the same live scrap would stack a second set of handlers.
    disconnect(scrap, nullptr, this, nullptr);

    auto entryIt = m_leadItems.find(scrap);
    if(entryIt != m_leadItems.end()) {
        ScrapEntry& entry = entryIt->second;
        // Drop the keyword item before the scrap's lead items go away; its proxy
        // is a child of the item, so the registry owns its teardown.
        m_keywordRegistry.drop(scrap);
        // The entry's handles remove their billboards from the shared layer when
        // the entry is erased below.
        for(auto item : std::as_const(entry.items)) {
            item->deleteLater();
        }
        m_leadItems.erase(entryIt);
    }

    if(m_leadItems.size() == 0) {
        m_currentScrapId = 0; //Reset the scrap id, this is useful for the qml testcase to work correctly
    }
}

/**
 * @brief cwLeadView::keywordItemModel
 * @return The keyword model this view publishes its per-scrap lead items into.
 */
cwKeywordItemModel* cwLeadView::keywordItemModel() const {
    return m_keywordRegistry.model();
}

void cwLeadView::setKeywordItemModel(cwKeywordItemModel* keywordItemModel) {
    if(m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    // Tears down items in the old model; re-register every scrap that still has
    // leads against the new one.
    m_keywordRegistry.setModel(keywordItemModel);

    for(const auto& [scrap, entry] : m_leadItems) {
        updateKeywordItem(scrap);
    }

    emit keywordItemModelChanged();
}

/**
 * @brief cwLeadView::updateKeywordItem
 *
 * Lazily registers/unregisters the scrap's lead keyword item so the filter only
 * carries a Type=Lead row for scraps that actually hold leads: create it when the
 * count crosses 0->1, drop it when it returns to 0. The item references the
 * scrap-owned cwScrap::leadKeywordModel() (Type="Lead" plus the scrap's inherited
 * keywords: Type=Plan/Profile, Cave, Year, ...), so leads filter both as "Lead"
 * and alongside their scrap. The cwLeadVisibility proxy (a child of the item)
 * receives the filter's setVisible and calls back into setScrapKeywordVisible.
 * Mirrors cwLinePlotManager / cwScrapManager.
 */
void cwLeadView::updateKeywordItem(cwScrap* scrap) {
    auto it = m_leadItems.find(scrap);
    if(it == m_leadItems.end()) {
        return;
    }

    if(it->second.items.isEmpty()) {
        m_keywordRegistry.drop(scrap);
    } else {
        // ensure() calls addItem, which fires resolveVisibility -> proxy
        // setVisible; if the current filter excludes leads, that calls back into
        // setScrapKeywordVisible(scrap, false), hiding the items.
        m_keywordRegistry.ensure(scrap, [this, scrap]() {
            auto item = new cwKeywordItem();
            item->keywordModel()->addExtension(scrap->leadKeywordModel());
            auto visibility = new cwLeadVisibility(this, scrap, item);
            item->setObject(visibility);
            return item;
        });
    }
}

void cwLeadView::setScrapKeywordVisible(cwScrap* scrap, bool visible) {
    auto it = m_leadItems.find(scrap);
    if(it == m_leadItems.end()) {
        return;
    }

    ScrapEntry& entry = it->second;
    entry.keywordVisible = visible;

    for(auto item : std::as_const(entry.items)) {
        if(item != nullptr) {
            item->setVisible(visible);
        }
    }

    // The QuoteBox popup is a sibling overlay, not pulled into the billboard, so
    // it would linger pointing at a now-hidden marker. Close it.
    if(!visible) {
        QQuickItem* selected = SelectionMananger->selectedItem();
        if(selected != nullptr && entry.items.contains(selected)) {
            SelectionMananger->setSelectedItem(nullptr);
        }
    }
}

QQuickItem* cwLeadView::createItem()
{
    if(m_itemComponent == nullptr) {
        createComponent();
        // qDebug() << "ItemComponent hasn't been created, call createComponent(). THIS IS A BUG" << LOCATION;
        // return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    //Supply the required injected properties at creation: selectionManager routes
    //selection through the shared manager (single open popup, click-away clears),
    //and leadView lets the tap handler gate clicks with the occlusion test.
    QQuickItem* item = qobject_cast<QQuickItem*>(
        m_itemComponent->createWithInitialProperties(
            {{QStringLiteral("selectionManager"), QVariant::fromValue(SelectionMananger)},
             {QStringLiteral("leadView"), QVariant::fromValue(this)}},
            context));
    if(item == nullptr) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        qDebug() << "Compiling errors:" << m_itemComponent->errorString();
        return nullptr;
    }

    //Reparent the item so the reverse transform works correctly
    //This will keep the item's size and rotation consistent
    item->setParentItem(this);
    item->setParent(this);
    // Q_ASSERT(item->parent() == this);

    // The marker draws in the on-demand 3D pass, so its fade/visibility changes
    // must request a frame. Mirror the label pattern: re-render the billboards
    // when the content item's opacity or visibility changes.
    QQuickItem* content = item->property("billboardContent").value<QQuickItem*>();
    if(content != nullptr) {
        connect(content, &QQuickItem::opacityChanged, this, &cwLeadView::requestBillboardRender);
        connect(content, &QQuickItem::visibleChanged, this, &cwLeadView::requestBillboardRender);
    }

    return item;
}

void cwLeadView::createComponent()
{
    //Make sure we have a note component so we can create it
    if(m_itemComponent == nullptr) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        if(context == nullptr) {
            qDebug() << "Context is nullptr, did you forget to set the context? THIS IS A BUG" << LOCATION;
            return;
        }

        m_itemComponent = new QQmlComponent(context->engine(), qmlSource(), this);
        if(m_itemComponent->isError()) {
            qDebug() << "Point errors:" << m_itemComponent->errorString();
        }
    }
}

QString cwLeadView::qmlSource() const
{
    return QStringLiteral("qrc:/qt/qml/cavewherelib/qml/LeadPoint.qml");
}

/**
 * @brief cwLeadView::scrapsAdded
 * @param parent
 * @param begin
 * @param end
 *
 * This should be called when the model adds scraps
 */
void cwLeadView::scrapsAdded(QModelIndex parent, int begin, int end)
{
    if(RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            //Get the scrap out of the model
            QModelIndex index = RegionModel->index(i, 0, parent);
            cwScrap* scrap = RegionModel->data(index, cwRegionTreeModel::ObjectRole).value<cwScrap*>();

            //Add the scrap
            addScrap(scrap);
        }
    }
}

/**
 * @brief cwLeadView::scrapsRemoved
 * @param parent
 * @param begin
 * @param end
 *
 * This should be called when the model removes scraps
 */
void cwLeadView::scrapsRemoved(QModelIndex parent, int begin, int end)
{
    if(RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            //Get the scrap out of the model
            QModelIndex index = RegionModel->index(i, 0, parent);
            cwScrap* scrap = RegionModel->data(index, cwRegionTreeModel::ObjectRole).value<cwScrap*>();

            //Add the scrap
            removeScrap(scrap);
        }
    }
}


/**
* @brief cwLeadView::repositionBillboards
*
* The base calls this on every scene/camera/window/visibility change. Leads have
* a 2D hit item per lead in addition to the billboard, so we reproject through
* updateItemPositions().
*/
void cwLeadView::repositionBillboards() {
    updateItemPositions();
}

/**
 * @brief cwLeadView::updateItemPositions
 *
 * Projects every lead's world position to screen (placing the invisible 2D hit
 * item) and registers/updates its billboard for the occluded draw. The hit item
 * and the billboard share the camera, so the tap target sits under the marker.
 */
void cwLeadView::updateItemPositions() {
    if(camera() == nullptr || !isVisible()) {
        return;
    }

    const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();

    for(auto it = m_leadItems.begin(); it != m_leadItems.end(); ++it) {
        ScrapEntry& entry = it->second;
        for(int i = 0; i < entry.items.size(); i++) {
            QQuickItem* item = entry.items.at(i);
            if(item == nullptr) {
                continue;
            }

            const QVector3D world = item->property("position3D").value<QVector3D>();
            const QVector3D screen = matrix.map(world);
            item->setPosition(QPointF(screen.x(), screen.y()));

            addOrUpdateBillboard(entry, i, item, world);
        }
    }
}

/**
 * @brief cwLeadView::addOrUpdateBillboard
 *
 * Registers (or refreshes) a lead's question-mark marker as a depth-occluded
 * billboard. The content is the LeadPoint's billboardContent sub-item, so the
 * 2D QuoteBox popup (a sibling) stays an un-occluded overlay.
 */
void cwLeadView::addOrUpdateBillboard(ScrapEntry& entry, int index, QQuickItem* item, const QVector3D& worldPosition) {
    if(item == nullptr) {
        return;
    }
    if(index < 0 || index >= int(entry.billboardHandles.size())) {
        return;
    }

    QQuickItem* content = item->property("billboardContent").value<QQuickItem*>();
    setBillboard(entry.billboardHandles.at(index), content, worldPosition, kLeadDepthBias);
}

/**
 * @brief cwLeadView::isOccluded
 *
 * Casts one CPU ray through the tapped viewport pixel and reports whether cave
 * geometry sits in front of the lead's billboard at that pixel. The marker is a
 * camera-facing quad, so a single eye->center ray cannot match the per-pixel
 * silhouette the depth buffer clips against (a click on a visible rim pixel would
 * be rejected because the center happens to sit behind a fold). Comparing the
 * cave hit to the billboard plane's depth *at the tapped pixel* keeps the click
 * consistent with what is drawn. Main-thread / works offscreen (the BVH is
 * CPU-side). Returns false when there is no camera, scene, or geometry to test.
 */
bool cwLeadView::isOccluded(const QVector3D& worldPosition,
                            const QPointF& tapViewportPosition) const {
    if(camera() == nullptr || scene() == nullptr) {
        return false;
    }

    cwGeometryItersecter* intersecter = scene()->geometryItersecter();
    if(intersecter == nullptr) {
        return false;
    }

    const QRay3D ray = camera()->rayFromQtViewport(tapViewportPosition);

    // The marker is a camera-facing quad centred on worldPosition; its plane
    // normal is the camera's view direction (third row of the view matrix). The
    // tap usually lands off-centre, so the point under the cursor is where the
    // ray crosses that plane — NOT worldPosition itself. Comparing the cave hit
    // to the centre's depth would mis-gate off-centre taps under perspective
    // (the ray through a rim pixel does not pass through the centre). Solve the
    // ray/plane crossing so the depth is taken at the tapped quad point.
    const QMatrix4x4 view = camera()->viewMatrix();
    const QVector3D planeNormal(view(2, 0), view(2, 1), view(2, 2));
    const double denom = QVector3D::dotProduct(ray.direction(), planeNormal);
    if(qFuzzyIsNull(denom)) {
        return false;
    }
    // ray.direction() is unit length (cwCamera::frustrumRay normalizes it), so
    // this parameter is a world distance comparable to cwRayHit::tWorld(). The
    // - kLeadDepthBias slack mirrors the eye-ward shift the billboard renders
    // with (cwRenderBillboards::buildModelMatrix).
    const double quadDistance =
        QVector3D::dotProduct(worldPosition - ray.origin(), planeNormal) / denom;
    if(quadDistance <= 0.0) {
        return false;
    }

    const cwRayHit hit = intersecter->intersectsDetailed(ray);
    if(!hit.hit()) {
        return false;
    }

    return hit.tWorld() < quadDistance - kLeadDepthBias;
}

/**
 * @brief cwLeadView::select
 * @param scarp - The scrap that the lead is located in
 * @param index - The index of the lead in the scrap
 *
 * This finds the scrapView for the scrap and selects the lead at index. If the scrap
 * isn't in the view, this does nothing.
 */
void cwLeadView::select(cwScrap *scrap, int index)
{
    const auto entryIt = m_leadItems.find(scrap);
    if(entryIt != m_leadItems.end()) {
        const ScrapEntry& entry = entryIt->second;
        if(index < entry.items.size()) {
            auto item = entry.items.at(index);
            SelectionMananger->setSelectedItem(item);
        }
    }
}

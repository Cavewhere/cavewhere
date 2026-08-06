/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADVIEW_H
#define CWLEADVIEW_H

//Qt includes
#include <QPointer>
#include <QModelIndex>
#include <QQmlEngine>
#include <QVector>
#include <QVector3D>
#include <QPointF>

//Our includes
#include "cwAbstractPointManager.h"
#include "cwBillboardHandle.h"
#include "cwBillboardOverlayItem.h"
#include "cwKeywordItemRegistry.h"

//Std includes
#include <vector>
#include <unordered_map>
class cwRegionTreeModel;
class cwScrap;
class cwScrapLeadView;
class cwKeywordItemModel;

/**
 * @brief The cwLeadView class
 *
 * Shows every lead from all caves and scraps. For each scrap it creates one
 * LeadPoint item per lead, positioned from the lead's global cwScrap::LeadPosition
 * (computed by the carpeting algorithm). Following cwLabel3dView, the view
 * projects those world positions itself on each camera change and renders each
 * lead's marker as a depth-occluded billboard (cwRenderBillboards); the LeadPoint
 * item stays as the invisible 2D layer that owns the tap, selection, and popup.
 */
class cwLeadView : public cwBillboardOverlayItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LeadView)

    Q_PROPERTY(cwRegionTreeModel* regionModel READ regionModel WRITE setRegionModel NOTIFY regionModelChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager CONSTANT)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)


public:
    cwLeadView(QQuickItem* parent = 0);

    cwRegionTreeModel* regionModel() const;
    void setRegionModel(cwRegionTreeModel* regionModel);

    cwSelectionManager* selectionManager() const;

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    // Called by a scrap's cwLeadVisibility proxy when keyword filters change.
    // Shows or hides every LeadPoint in the scrap (effective isVisible() then
    // gates the billboard draw and the tap target), and closes the popup if the
    // selected lead belongs to a scrap being hidden.
    void setScrapKeywordVisible(cwScrap* scrap, bool visible);

    Q_INVOKABLE void select(cwScrap* scarp, int index);

    // True when cave geometry hides the billboard pixel the user actually tapped.
    // tapViewportPosition is a Qt-viewport pixel (top-left origin, the lead item's
    // own coordinate space). A ray is cast through that pixel and compared against
    // the lead's camera-facing billboard plane, so the test matches the per-pixel
    // silhouette the depth buffer draws — what you see is what you can click. One
    // CPU BVH ray per call (main thread). Used by LeadPoint to ignore taps that
    // land on a part of the marker hidden behind a wall.
    Q_INVOKABLE bool isOccluded(const QVector3D& worldPosition,
                                const QPointF& tapViewportPosition) const;

signals:
    void regionModelChanged();
    void keywordItemModelChanged();

protected:
    void repositionBillboards() override;

private:
    QPointer<cwRegionTreeModel> RegionModel; //!<
    cwSelectionManager* SelectionMananger;

    struct ScrapEntry {
        int scrapLeadId = -1;
        QVector<QQuickItem*> items;
        // Index-aligned with items. Each handle owns its lead's billboard in the
        // shared layer and removes it when erased or when the entry is destroyed,
        // so there are no manual removeBillboard calls. Move-only, which makes the
        // entry move-only (QHash holds it fine; .value() copies are now find()s).
        std::vector<cwBillboardHandle> billboardHandles;
        bool keywordVisible = true; //!< last value pushed by the keyword filter
    };

    // std::unordered_map, not QHash: ScrapEntry is move-only (it owns billboard
    // handles), and QHash's copy-on-write detach() requires a copyable value.
    std::unordered_map<cwScrap*, ScrapEntry> m_leadItems;
    QQmlComponent* m_itemComponent = nullptr;
    int m_currentScrapId = 0;

    // The per-scrap lead keyword item is owned by the registry, registered only
    // while the scrap has >=1 lead (see updateKeywordItem).
    cwKeywordItemRegistry<cwScrap*> m_keywordRegistry;

    void addScrap(cwScrap* scrap);
    void removeScrap(cwScrap* scrap);

    // A model reset (project load / git checkout) replaces every row without per-row
    // remove/insert signals, so the view must clear its tracked scraps before the
    // reset and repopulate after it.
    void clearAllScraps();
    void addAllScraps();

    void updateKeywordItem(cwScrap* scrap);

    QQuickItem *createItem();
    void createComponent();

    QString qmlSource() const;

    void addOrUpdateBillboard(ScrapEntry& entry, int index, QQuickItem* item, const QVector3D& worldPosition);

private slots:
    void scrapsAdded(QModelIndex parent, int begin, int end);
    void scrapsRemoved(QModelIndex parent, int begin, int end);
    void updateItemPositions();

};

/**
 * @brief cwLeadView::selectionManager
 * @return The selection manager that tracks the single selected lead, so lead
 * taps route through it (single selection) and a tap on empty space can clear it.
 */
inline cwSelectionManager* cwLeadView::selectionManager() const {
    return SelectionMananger;
}

#endif // CWLEADVIEW_H

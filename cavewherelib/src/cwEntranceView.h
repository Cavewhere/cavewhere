/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCEVIEW_H
#define CWENTRANCEVIEW_H

//Qt includes
#include <QColor>
#include <QFont>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QUuid>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwBillboardOverlayItem.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItemRegistry.h"
#include "cwSelectionManager.h"
#include "cwSpriteBillboardHandle.h"
#include "cwTextBillboardHandle.h"

//Std includes
#include <map>

class cwSurveyNode;
class cwKeywordItem;
class QQmlComponent;

/**
 * @brief Draws a pin and one line of text at every placed, solved fix station.
 *
 * Walks every node of the region and, for each of a node's own fixes whose
 * station has a solved position below that node and whose coordinate places it,
 * draws a tip-down pin in this view's keyed sprite layer (slot = the fix's
 * cwFixStation::Kind) and the cwEntranceLabel text above it in the keyed text
 * layer. Each drawn fix also owns an invisible EntrancePoint item, the tap
 * target, and one cwKeywordItem (Type = kind name, Name = display name,
 * extending the node's keywords) whose cwEntranceVisibility hides that fix.
 * Model changes coalesce into one queued rebuild; camera changes only
 * reproject.
 */
class CAVEWHERE_LIB_EXPORT cwEntranceView : public cwBillboardOverlayItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EntranceView)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager CONSTANT)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)
    Q_PROPERTY(qreal textGap READ textGap WRITE setTextGap NOTIFY textGapChanged)
    Q_PROPERTY(int entranceCount READ entranceCount NOTIFY entrancesChanged)

public:
    explicit cwEntranceView(QQuickItem* parent = nullptr);
    ~cwEntranceView() override;

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    cwSelectionManager* selectionManager() const;

    QFont font() const;
    void setFont(const QFont& font);

    QColor color() const;
    void setColor(const QColor& color);

    QColor outlineColor() const;
    void setOutlineColor(const QColor& outlineColor);

    //! Logical pixels between the top of the pin and the bottom of the text.
    qreal textGap() const;
    void setTextGap(qreal textGap);

    //! The number of fixes drawn.
    int entranceCount() const;

    //! Called by a fix's cwEntranceVisibility when keyword filters change. Shows
    //! or hides that fix's pin, text and tap target, and clears the selection
    //! when it hides the selected fix.
    void setFixKeywordVisible(const QUuid& fixId, bool visible);

signals:
    void regionChanged();
    void keywordItemModelChanged();
    void fontChanged();
    void colorChanged();
    void outlineColorChanged();
    void textGapChanged();
    //! Emitted once per rebuild pass that changed the drawn set, any kind, or any text.
    void entrancesChanged();

protected:
    void repositionBillboards() override;

private:
    // One drawn fix. Move-only: the handles own the fix's pin and text in the
    // keyed layers and remove them when the entry is erased.
    struct Entry {
        QPointer<cwSurveyNode> node;
        QPointer<QQuickItem> item; //!< the EntrancePoint tap target
        QPointer<cwKeywordItem> keywordItem;
        cwSpriteBillboardHandle pinBillboard;
        cwTextBillboardHandle textBillboard;
        QVector3D position;
        QString displayName;
        QString text;
        cwFixStation::Kind kind = cwFixStation::Entrance;
        bool keywordVisible = true;
    };

    // What one rebuild pass wants drawn for a fix.
    struct Desired {
        cwSurveyNode* node = nullptr;
        QString stationName;
        QVector3D position;
        QString displayName;
        QString text;
        cwFixStation::Kind kind = cwFixStation::Entrance;
    };

    QPointer<cwCavingRegion> m_region;
    cwSelectionManager* m_selectionManager;
    QFont m_font;
    QColor m_color = Qt::white;
    QColor m_outlineColor = Qt::black;
    qreal m_textGap = 0.0;

    // std::map, not QHash: Entry is move-only and QHash's detach() copies values.
    std::map<QUuid, Entry> m_entries;
    QSet<cwSurveyNode*> m_connectedNodes;
    cwKeywordItemRegistry<QUuid> m_keywordRegistry;
    QQmlComponent* m_itemComponent = nullptr;
    bool m_rebuildPending = false;

    void scheduleRebuild();
    void connectRegion();
    void disconnectRegion();
    void syncNodeConnections();
    void disconnectNode(cwSurveyNode* node);
    QList<std::pair<QUuid, Desired>> desiredEntrances() const;

    void ensureKeywordItems(const QList<QUuid>& ids);
    void replaceKeyword(Entry& entry, const QString& key, const QString& value);

    QQuickItem* createItem(const QUuid& fixId, const Desired& desired);
    void createComponent();

    bool applyStyle();
    void projectItems();
    void updateBillboards(Entry& entry);
    void updateAllBillboards();
    void clearSelectionIfIn(const Entry& entry);

private slots:
    void rebuild();
    void restyle();
    void nodeDestroyed(QObject* node);
    // The sending marker's effective visibility changed; its parent is the
    // EntrancePoint.
    void markerVisibleChanged();
};

#endif // CWENTRANCEVIEW_H

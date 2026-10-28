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
#include <vector>

class cwSurveyNode;
class cwKeywordItem;
class QQmlComponent;

/**
 * @brief Draws a pin and up to two lines of text at every placed, solved fix station.
 *
 * Walks every node of the region and, for each of a node's own fixes whose
 * station has a solved position below that node and whose coordinate places it,
 * draws a tip-down pin in this view's keyed sprite layer (slot = the fix's
 * cwFixStation::Kind) with the fix's display name above it in the keyed text
 * layer. An Entrance also draws its cwEntranceLabel stats line between the pin
 * and the name, in the statsLayerKey() text layer with its own style. Each
 * drawn fix also owns an invisible EntrancePoint item, the tap target, and one
 * cwKeywordItem (Type = kind name, Name = display name, extending the node's
 * keywords) whose cwEntranceVisibility hides that fix.
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
    Q_PROPERTY(QFont statsFont READ statsFont WRITE setStatsFont NOTIFY statsFontChanged)
    Q_PROPERTY(QColor statsColor READ statsColor WRITE setStatsColor NOTIFY statsColorChanged)
    Q_PROPERTY(QColor statsOutlineColor READ statsOutlineColor WRITE setStatsOutlineColor NOTIFY statsOutlineColorChanged)
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

    //! The style of the length and depth line under an entrance's name.
    QFont statsFont() const;
    void setStatsFont(const QFont& statsFont);

    QColor statsColor() const;
    void setStatsColor(const QColor& statsColor);

    QColor statsOutlineColor() const;
    void setStatsOutlineColor(const QColor& statsOutlineColor);

    //! The key of the scene text layer that draws the stats lines; layerKey()
    //! draws the names.
    QString statsLayerKey() const;

    //! Logical pixels between the top of the pin and the bottom of the text.
    qreal textGap() const;
    void setTextGap(qreal textGap);

    //! The number of fixes drawn.
    int entranceCount() const;

    //! Called by a fix's cwEntranceVisibility when keyword filters change. Shows
    //! or hides that fix's pin, text and tap target, and clears the selection
    //! when it hides the selected fix.
    void setFixKeywordVisible(const QUuid& fixId, bool visible);

    //! Selects the EntrancePoint of @a node's fix @a fixId, opening its popup.
    //! Does nothing when that fix of that node isn't drawn.
    Q_INVOKABLE void select(cwSurveyNode* node, const QUuid& fixId);

    //! True when the pin pixel under @a tapViewportPosition (a Qt-viewport pixel
    //! in this view's coordinates) isn't drawn: the tap misses the tip-down pin,
    //! or cave geometry hides that pixel. EntrancePoint ignores such taps.
    Q_INVOKABLE bool isOccluded(const QVector3D& worldPosition,
                                const QPointF& tapViewportPosition) const;

signals:
    void regionChanged();
    void keywordItemModelChanged();
    void fontChanged();
    void colorChanged();
    void outlineColorChanged();
    void statsFontChanged();
    void statsColorChanged();
    void statsOutlineColorChanged();
    void textGapChanged();
    //! Emitted once per rebuild pass that changed the drawn set, any kind, any
    //! name, or any stats line.
    void entrancesChanged();

protected:
    void repositionBillboards() override;
    void claimDeclutterSpace(cwCollisionRectGrid& grid) override;

private:
    // One drawn fix. Move-only: the handles own the fix's pin and text lines in
    // the keyed layers and remove them when the entry is erased.
    struct Entry {
        QPointer<cwSurveyNode> node;
        QPointer<QQuickItem> item; //!< the EntrancePoint tap target
        QPointer<cwKeywordItem> keywordItem;
        cwSpriteBillboardHandle pinBillboard;
        cwTextBillboardHandle nameBillboard;
        cwTextBillboardHandle statsBillboard;
        QVector3D position;
        QString displayName;
        QString statsText; //!< set for an Entrance, empty otherwise
        QSizeF nameSize; //!< laid-out size of displayName, valid while textSizesValid
        QSizeF statsSize; //!< laid-out size of statsText; an empty size for empty statsText
        cwFixStation::Kind kind = cwFixStation::Entrance;
        bool keywordVisible = true;
        bool textSizesValid = false;
    };

    // An entry whose text block claims space in a declutter pass, and where.
    struct TextCandidate {
        Entry* entry = nullptr;
        QUuid id;
        QVector3D qtViewportPosition;
        bool shown = false; //!< drew its text after the last pass
    };

    // What one rebuild pass wants drawn for a fix.
    struct Desired {
        cwSurveyNode* node = nullptr;
        QString stationName;
        QVector3D position;
        QString displayName;
        QString statsText;
        cwFixStation::Kind kind = cwFixStation::Entrance;
    };

    QPointer<cwCavingRegion> m_region;
    cwSelectionManager* m_selectionManager;
    QFont m_font;
    QColor m_color = Qt::white;
    QColor m_outlineColor = Qt::black;
    QFont m_statsFont;
    QColor m_statsColor = Qt::gray;
    QColor m_statsOutlineColor = Qt::white;
    qreal m_textGap = 0.0;

    // std::map, not QHash: Entry is move-only and QHash's detach() copies values.
    std::map<QUuid, Entry> m_entries;
    QSet<cwSurveyNode*> m_connectedNodes;
    cwKeywordItemRegistry<QUuid> m_keywordRegistry;
    QQmlComponent* m_itemComponent = nullptr;
    bool m_rebuildPending = false;
    std::vector<TextCandidate> m_textCandidates; //!< reused by each declutter pass

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
    static void measureText(Entry& entry, cwRenderTextBillboards* names, cwRenderTextBillboards* stats);
    void projectItems();
    bool isEntryVisible(const Entry& entry) const;
    void updatePin(Entry& entry, bool visible);
    void updateText(Entry& entry);
    static void releaseText(Entry& entry);
    void updateBillboards(Entry& entry);
    void updateAllBillboards();
    void prepareLayers();
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

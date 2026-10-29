/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMARKERVIEW_H
#define CWMARKERVIEW_H

//Qt includes
#include <QHash>
#include <QList>
#include <QMatrix4x4>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QString>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwBillboardDepthMode.h"
#include "cwBillboardOverlayItem.h"
#include "cwKeywordItemModel.h"
#include "cwMarkerSource.h"
#include "cwMarkerTextStyle.h"
#include "cwKeywordItemRegistry.h"
#include "cwSelectionManager.h"
#include "cwSpriteBillboardHandle.h"
#include "cwTextBillboardHandle.h"

//Std includes
#include <memory>
#include <vector>

/**
 * @brief Draws the markers of a cwMarkerSource as sprites and text lines in the
 * scene's layers of layerKey, with one delegate item per marker as its tap target.
 *
 * Each marker draws its sprite slot at its world position and its text lines
 * above the sprite, each line styled and layered by the textStyles entry of its
 * style key. The delegate (LeadPoint, EntrancePoint) is created with the
 * marker's delegateProperties plus markerView and selectionManager, placed at
 * the marker's projected position, and owns the tap and the popup. Its
 * "marker" item's visibility, opacity and optional spriteIndex drive the
 * marker's sprite instance.
 *
 * The view keeps one keyword item per marker visibility group in
 * keywordItemModel, reprojects on every camera change, and claims its pins and
 * text blocks in the scene's declutter pass when it has a declutterTier.
 */
class CAVEWHERE_LIB_EXPORT cwMarkerView : public cwBillboardOverlayItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MarkerView)

    Q_PROPERTY(cwMarkerSource* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager WRITE setSelectionManager NOTIFY selectionManagerChanged)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)
    Q_PROPERTY(QQmlListProperty<cwMarkerTextStyle> textStyles READ textStyles NOTIFY textStylesChanged)
    Q_PROPERTY(qreal textGap READ textGap WRITE setTextGap NOTIFY textGapChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit cwMarkerView(QQuickItem* parent = nullptr);
    ~cwMarkerView() override;

    cwMarkerSource* source() const;
    void setSource(cwMarkerSource* source);

    QQmlComponent* delegate() const;
    void setDelegate(QQmlComponent* delegate);

    //! The selection manager shared by every marker view, so one tap selects one
    //! marker across views. Null leaves the markers unselectable.
    cwSelectionManager* selectionManager() const;
    void setSelectionManager(cwSelectionManager* selectionManager);

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    QQmlListProperty<cwMarkerTextStyle> textStyles();

    //! Logical pixels between the top of the sprite and the bottom of the text.
    qreal textGap() const;
    void setTextGap(qreal textGap);

    //! The number of markers drawn, hidden ones included.
    int count() const;

    //! Selects the delegate of the source's marker @a row, opening its popup.
    //! Does nothing for a row out of range or without a selection manager.
    Q_INVOKABLE void select(int row);

    //! True when the tap at @a tapViewportPosition (a Qt-viewport pixel in this
    //! view's coordinates) lands outside the sprite the source's spriteAnchor
    //! places, or when cave geometry hides the tapped pixel.
    Q_INVOKABLE bool isOccluded(const QVector3D& worldPosition,
                                const QPointF& tapViewportPosition) const;

    //! True when a tap at @a tapViewportPosition selects the marker at
    //! @a worldPosition: the tap lands on the sprite, and the marker draws
    //! there. A marker hidden behind geometry draws while x-ray is on or the
    //! depth test is off, and only the depth test on with x-ray off hides it.
    Q_INVOKABLE bool isTappable(const QVector3D& worldPosition,
                                const QPointF& tapViewportPosition) const;

    //! Called by a group's cwMarkerVisibility when keyword filters change. Shows
    //! or hides every marker of @a group, and clears the selection when it hides
    //! the selected marker.
    void setGroupKeywordVisible(const QString& group, bool visible);

signals:
    void sourceChanged();
    void delegateChanged();
    void selectionManagerChanged();
    void keywordItemModelChanged();
    void textStylesChanged();
    void textGapChanged();
    void countChanged();

protected:
    void componentComplete() override;
    void repositionBillboards() override;
    void claimDeclutterSpace(cwCollisionRectGrid& grid) override;

private:
    // One drawn marker. The handles own its sprite and text lines in the
    // scene's layers and remove them when the entry goes.
    struct Entry {
        cwMarker marker;
        QPointer<QQuickItem> item; //!< the delegate, the tap target
        QPointer<QQuickItem> markerItem; //!< the delegate's "marker" item
        cwSpriteBillboardHandle sprite;
        std::vector<cwTextBillboardHandle> lines; //!< index-aligned with marker.textLines
        QList<QSizeF> lineSizes; //!< laid-out size of each line, valid while lineSizesValid
        bool lineSizesValid = false;
    };

    // The markers sharing one keyword item.
    struct Group {
        QList<Entry*> entries;
        QPointer<cwKeywordItem> keywordItem;
        bool keywordVisible = true;
    };

    // An entry whose text block claims space in a declutter pass, and where.
    struct TextCandidate {
        Entry* entry = nullptr;
        QVector3D qtViewportPosition;
        bool shown = false; //!< drew its text after the last pass
    };

    QPointer<cwMarkerSource> m_source;
    QPointer<QQmlComponent> m_delegate;
    QPointer<cwSelectionManager> m_selectionManager;
    QList<cwMarkerTextStyle*> m_textStyles;
    qreal m_textGap = 0.0;

    // Row-aligned with the source. Entries are heap-held so groups and the
    // marker lookup keep stable pointers while rows shift.
    std::vector<std::unique_ptr<Entry>> m_entries;
    QHash<QString, Group> m_groups;
    QHash<const QObject*, Entry*> m_entryByMarkerItem;
    cwKeywordItemRegistry<QString> m_keywordRegistry;
    std::vector<TextCandidate> m_textCandidates; //!< reused by each declutter pass

    //! How this view's layers meet the scene's depth: the marker knobs.
    cwBillboardDepthMode m_depthMode;

    void removeAllEntries();
    void insertEntries(int first, int last);
    void removeEntries(int first, int last);
    void changeEntries(int first, int last);
    void refreshEntries(int first, int last);
    void emitCountChangedFrom(int oldCount);

    std::unique_ptr<Entry> createEntry(const cwMarker& marker);
    void destroyEntry(Entry& entry);
    void createDelegate(Entry& entry);
    void updateDelegate(Entry& entry, const cwMarker& oldMarker);

    bool joinGroup(Entry* entry);
    void leaveGroup(Entry* entry, const QString& group);
    bool isGroupVisible(const QString& group) const;
    void refreshDeclutter();
    void ensureKeywordItems(const QStringList& groups);
    void rebuildKeywordItem(const QString& group);
    void replaceKeywords(const QString& group, const QList<cwKeyword>& oldKeywords,
                         const QList<cwKeyword>& keywords);

    QPointF spriteAnchor() const;
    float depthBias() const;
    const cwMarkerTextStyle* style(const QString& styleKey) const;
    cwRenderTextBillboards* textLayer(const cwMarkerTextStyle* style);
    cwRenderTextBillboards* lineLayer(const cwMarkerTextLine& line);
    bool applyStyles();
    void measureLines(Entry& entry);
    void projectItem(const Entry& entry, const QMatrix4x4& matrix);
    void projectItems();
    void prepareLayers();
    void pushDepthMode();
    bool isEntryVisible(const Entry& entry) const;
    void updateSprite(Entry& entry, bool visible);
    void updateText(Entry& entry);
    static void releaseText(Entry& entry);
    void updateBillboards(Entry& entry);
    void updateAllBillboards();
    void clearSelectionIfIn(const Entry& entry);
    void disconnectSource();

    static void appendTextStyle(QQmlListProperty<cwMarkerTextStyle>* list, cwMarkerTextStyle* style);
    static qsizetype textStyleCount(QQmlListProperty<cwMarkerTextStyle>* list);
    static cwMarkerTextStyle* textStyleAt(QQmlListProperty<cwMarkerTextStyle>* list, qsizetype index);
    static void clearTextStyles(QQmlListProperty<cwMarkerTextStyle>* list);

private slots:
    void rebuildFromSource();
    void sourceMarkersInserted(int first, int last);
    void sourceMarkersRemoved(int first, int last);
    void sourceMarkersChanged(int first, int last);
    void restyle();
    // The marker knobs in cwRenderingSettings moved.
    void applyDepthMode();
    // The sending marker item's visibility, opacity or sprite slot changed.
    void markerItemChanged();
};

#endif // CWMARKERVIEW_H

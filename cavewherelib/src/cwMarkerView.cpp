/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMarkerView.h"
#include "cwBillboardGeometry.h"
#include "cwCamera.h"
#include "cwDebug.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwLabelDeclutter.h"
#include "cwMarkerVisibility.h"
#include "cwPositionItem.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"

//Qt includes
#include <QQmlContext>
#include <QRectF>

//Std includes
#include <algorithm>
#include <iterator>

namespace {
constexpr qreal kHalf = 0.5;
constexpr float kFullOpacity = 1.0f;

// Returns true when @a layer's style changed.
bool pushStyle(cwRenderTextBillboards* layer, const cwMarkerTextStyle* textStyle)
{
    if(layer == nullptr) {
        return false;
    }

    cwRenderTextBillboards::Style style;
    style.font = textStyle->font();
    style.color = textStyle->color();
    style.outlineColor = textStyle->outlineColor();
    if(layer->style() == style) {
        return false;
    }
    layer->setStyle(style);
    return true;
}
}

cwMarkerView::cwMarkerView(QQuickItem* parent) :
    cwBillboardOverlayItem(parent)
{
}

cwMarkerView::~cwMarkerView()
{
    disconnectSource();

    // The delegates are QObject children and outlive m_entries during
    // teardown; a marker item change from them must not reach this view then.
    for(const auto& entry : m_entries) {
        if(!entry->markerItem.isNull()) {
            disconnect(entry->markerItem, nullptr, this, nullptr);
        }
    }
    m_entryByMarkerItem.clear();

    // The keyword items go while their visibility targets still point at a live view.
    m_keywordRegistry.clear();
}

cwMarkerSource* cwMarkerView::source() const
{
    return m_source;
}

void cwMarkerView::setSource(cwMarkerSource* source)
{
    if(m_source == source) {
        return;
    }

    disconnectSource();
    m_source = source;

    if(!m_source.isNull()) {
        connect(m_source, &cwMarkerSource::markersReset, this, &cwMarkerView::rebuildFromSource);
        connect(m_source, &cwMarkerSource::markersInserted, this, &cwMarkerView::sourceMarkersInserted);
        connect(m_source, &cwMarkerSource::markersRemoved, this, &cwMarkerView::sourceMarkersRemoved);
        connect(m_source, &cwMarkerSource::markersChanged, this, &cwMarkerView::sourceMarkersChanged);
        connect(m_source, &QObject::destroyed, this, &cwMarkerView::rebuildFromSource);
    }

    rebuildFromSource();
    emit sourceChanged();
}

void cwMarkerView::disconnectSource()
{
    if(!m_source.isNull()) {
        disconnect(m_source, nullptr, this, nullptr);
    }
}

QQmlComponent* cwMarkerView::delegate() const
{
    return m_delegate;
}

void cwMarkerView::setDelegate(QQmlComponent* delegate)
{
    if(m_delegate == delegate) {
        return;
    }
    m_delegate = delegate;
    rebuildFromSource();
    emit delegateChanged();
}

cwSelectionManager* cwMarkerView::selectionManager() const
{
    return m_selectionManager;
}

void cwMarkerView::setSelectionManager(cwSelectionManager* selectionManager)
{
    if(m_selectionManager == selectionManager) {
        return;
    }

    // Another view's marker may hold the old manager's selection; only this
    // view's own marker is cleared.
    if(!m_selectionManager.isNull()) {
        QQuickItem* selected = m_selectionManager->selectedItem();
        if(selected != nullptr && selected->parentItem() == this) {
            m_selectionManager->clear();
        }
    }

    m_selectionManager = selectionManager;

    for(const auto& entry : m_entries) {
        if(!entry->item.isNull()) {
            entry->item->setProperty("selectionManager", QVariant::fromValue(selectionManager));
        }
    }

    emit selectionManagerChanged();
}

cwKeywordItemModel* cwMarkerView::keywordItemModel() const
{
    return m_keywordRegistry.model();
}

void cwMarkerView::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if(m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    // Tears down the items in the old model; every group registers again
    // against the new one, starting shown so the new filter decides.
    m_keywordRegistry.setModel(keywordItemModel);

    const QStringList groups = m_groups.keys();
    for(const QString& group : groups) {
        m_groups[group].keywordItem = nullptr;
        if(!isGroupVisible(group)) {
            setGroupKeywordVisible(group, true);
        }
    }
    ensureKeywordItems(groups);

    emit keywordItemModelChanged();
}

QQmlListProperty<cwMarkerTextStyle> cwMarkerView::textStyles()
{
    return QQmlListProperty<cwMarkerTextStyle>(this, nullptr,
                                               &cwMarkerView::appendTextStyle,
                                               &cwMarkerView::textStyleCount,
                                               &cwMarkerView::textStyleAt,
                                               &cwMarkerView::clearTextStyles);
}

void cwMarkerView::appendTextStyle(QQmlListProperty<cwMarkerTextStyle>* list, cwMarkerTextStyle* style)
{
    auto view = static_cast<cwMarkerView*>(list->object);
    view->m_textStyles.append(style);
    if(style != nullptr) {
        connect(style, &cwMarkerTextStyle::styleKeyChanged, view, &cwMarkerView::restyle);
        connect(style, &cwMarkerTextStyle::layerKeyChanged, view, &cwMarkerView::restyle);
        connect(style, &cwMarkerTextStyle::fontChanged, view, &cwMarkerView::restyle);
        connect(style, &cwMarkerTextStyle::colorChanged, view, &cwMarkerView::restyle);
        connect(style, &cwMarkerTextStyle::outlineColorChanged, view, &cwMarkerView::restyle);
    }
    view->restyle();
    emit view->textStylesChanged();
}

qsizetype cwMarkerView::textStyleCount(QQmlListProperty<cwMarkerTextStyle>* list)
{
    return static_cast<cwMarkerView*>(list->object)->m_textStyles.size();
}

cwMarkerTextStyle* cwMarkerView::textStyleAt(QQmlListProperty<cwMarkerTextStyle>* list, qsizetype index)
{
    return static_cast<cwMarkerView*>(list->object)->m_textStyles.at(index);
}

void cwMarkerView::clearTextStyles(QQmlListProperty<cwMarkerTextStyle>* list)
{
    auto view = static_cast<cwMarkerView*>(list->object);
    for(cwMarkerTextStyle* style : std::as_const(view->m_textStyles)) {
        if(style != nullptr) {
            disconnect(style, nullptr, view, nullptr);
        }
    }
    view->m_textStyles.clear();
    view->restyle();
    emit view->textStylesChanged();
}

qreal cwMarkerView::textGap() const
{
    return m_textGap;
}

void cwMarkerView::setTextGap(qreal textGap)
{
    if(m_textGap == textGap) {
        return;
    }
    m_textGap = textGap;
    emit textGapChanged();
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->schedulePass();
    } else {
        updateAllBillboards();
    }
}

int cwMarkerView::count() const
{
    return int(m_entries.size());
}

void cwMarkerView::select(int row)
{
    if(row < 0 || row >= count() || m_selectionManager.isNull()) {
        return;
    }

    QQuickItem* item = m_entries.at(row)->item;
    if(item != nullptr) {
        m_selectionManager->setSelectedItem(item);
    }
}

bool cwMarkerView::isOccluded(const QVector3D& worldPosition,
                              const QPointF& tapViewportPosition) const
{
    return cw::billboard::isOccluded(scene() ? scene()->geometryItersecter() : nullptr,
                                     camera(), worldPosition, tapViewportPosition,
                                     spriteSize(), spriteAnchor(), depthBias());
}

void cwMarkerView::setGroupKeywordVisible(const QString& group, bool visible)
{
    const auto it = m_groups.find(group);
    if(it == m_groups.end()) {
        return;
    }

    it->keywordVisible = visible;
    const QList<Entry*> entries = it->entries;
    for(Entry* entry : entries) {
        if(!entry->item.isNull()) {
            // A hidden item also takes its tap target out of reach.
            entry->item->setVisible(visible);
        }
        if(!visible) {
            // The popup is a 2D overlay, so it would linger at a hidden marker.
            clearSelectionIfIn(*entry);
        }
        updateBillboards(*entry);
    }
}

void cwMarkerView::componentComplete()
{
    cwBillboardOverlayItem::componentComplete();
    rebuildFromSource();
}

/**
 * Projects every delegate onto the current camera, then draws every marker, or
 * leaves the drawing to the scene's declutter pass when this view takes part in it.
 */
void cwMarkerView::repositionBillboards()
{
    projectItems();
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->ensurePass(camera());
    } else {
        updateAllBillboards();
    }
}

/**
 * Every visible sprite claims its rect unconditionally, so sprites never hide.
 * Then each visible text block claims the union of its lines: blocks that drew
 * after the last pass first, the rest after, each group ordered by declutter
 * priority, viewport depth, y, x and marker id. A block that collides hides
 * all its lines and leaves its sprite drawn.
 */
void cwMarkerView::claimDeclutterSpace(cwCollisionRectGrid& grid)
{
    if(camera() == nullptr) {
        return;
    }

    prepareLayers();

    const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();
    const QSizeF size = spriteSize();
    const QPointF anchor = spriteAnchor();
    const QPointF anchorOffset(size.width() * anchor.x(), size.height() * anchor.y());

    m_textCandidates.clear();
    for(const auto& entryPtr : m_entries) {
        Entry& entry = *entryPtr;
        const bool visible = isEntryVisible(entry);
        updateSprite(entry, visible);

        const QVector3D qtViewportPosition = matrix.map(entry.marker.position);
        if(!visible || camera()->isQtViewportCoordinateClipped(qtViewportPosition)) {
            releaseText(entry);
            continue;
        }

        const QPointF tip(qtViewportPosition.x(), qtViewportPosition.y());
        grid.insertRect(QRectF(tip - anchorOffset, size).toAlignedRect());

        const bool shown = std::any_of(entry.lines.cbegin(), entry.lines.cend(),
                                       [](const cwTextBillboardHandle& line) { return line.isValid(); });
        m_textCandidates.push_back(TextCandidate{&entry, qtViewportPosition, shown});
    }

    std::sort(m_textCandidates.begin(), m_textCandidates.end(),
              [](const TextCandidate& lhs, const TextCandidate& rhs) {
                  if(lhs.shown != rhs.shown) {
                      return lhs.shown;
                  }
                  const int lhsPriority = lhs.entry->marker.declutterPriority;
                  const int rhsPriority = rhs.entry->marker.declutterPriority;
                  if(lhsPriority != rhsPriority) {
                      return lhsPriority < rhsPriority;
                  }
                  const QVector3D& a = lhs.qtViewportPosition;
                  const QVector3D& b = rhs.qtViewportPosition;
                  if(a.z() != b.z()) {
                      return a.z() < b.z();
                  }
                  if(a.y() != b.y()) {
                      return a.y() < b.y();
                  }
                  if(a.x() != b.x()) {
                      return a.x() < b.x();
                  }
                  return lhs.entry->marker.id < rhs.entry->marker.id;
              });

    for(const TextCandidate& candidate : m_textCandidates) {
        Entry& entry = *candidate.entry;
        measureLines(entry);
        qreal width = 0.0;
        qreal height = 0.0;
        for(const QSizeF& lineSize : std::as_const(entry.lineSizes)) {
            width = std::max(width, lineSize.width());
            height += lineSize.height();
        }
        if(height <= 0.0) {
            releaseText(entry);
            continue;
        }

        // Qt viewport y grows downward: the block ends m_textGap above the sprite.
        const QPointF tip(candidate.qtViewportPosition.x(), candidate.qtViewportPosition.y());
        const QRectF spriteRect(tip - anchorOffset, size);
        const qreal blockBottom = spriteRect.top() - m_textGap;
        QRect block = QRectF(tip.x() - width * kHalf, blockBottom - height, width, height).toAlignedRect();
        // Rounding outward keeps the block at least one pixel clear of its own sprite.
        block.setBottom(std::min(block.bottom(), spriteRect.toAlignedRect().top() - 1));

        if(grid.addRect(block)) {
            updateText(entry);
        } else {
            releaseText(entry);
        }
    }
}

void cwMarkerView::sourceMarkersInserted(int first, int last)
{
    if(!isComponentComplete() || m_source.isNull()) {
        return;
    }
    const int oldCount = count();
    insertEntries(first, last);
    emitCountChangedFrom(oldCount);
}

void cwMarkerView::sourceMarkersRemoved(int first, int last)
{
    if(!isComponentComplete()) {
        return;
    }
    const int oldCount = count();
    removeEntries(first, last);
    refreshDeclutter();
    emitCountChangedFrom(oldCount);
}

void cwMarkerView::sourceMarkersChanged(int first, int last)
{
    if(!isComponentComplete() || m_source.isNull()) {
        return;
    }
    changeEntries(first, last);
}

void cwMarkerView::restyle()
{
    if(!isComponentComplete()) {
        return;
    }

    // A style's layer, font or key moved, and with them each line's layer, size
    // and offset above its sprite.
    for(const auto& entry : m_entries) {
        entry->lineSizesValid = false;
    }
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        prepareLayers();
        declutter->schedulePass();
    } else {
        updateAllBillboards();
    }
}

void cwMarkerView::markerItemChanged()
{
    const auto it = m_entryByMarkerItem.constFind(sender());
    if(it != m_entryByMarkerItem.constEnd()) {
        updateBillboards(*it.value());
    }
}

void cwMarkerView::rebuildFromSource()
{
    if(!isComponentComplete()) {
        return;
    }

    const int oldCount = count();
    removeAllEntries();
    if(!m_source.isNull() && m_source->count() > 0) {
        insertEntries(0, m_source->count() - 1);
    }
    refreshDeclutter();
    emitCountChangedFrom(oldCount);
}

void cwMarkerView::removeAllEntries()
{
    for(const auto& entry : m_entries) {
        destroyEntry(*entry);
    }
    m_entries.clear();

    for(auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        m_keywordRegistry.drop(it.key());
    }
    m_groups.clear();
}

/**
 * Adds the entries of the source's rows @a first to @a last, each with its
 * delegate, then registers the keyword items of the groups those rows start in
 * one model insert, and draws the new rows.
 */
void cwMarkerView::insertEntries(int first, int last)
{
    if(first > last) {
        return;
    }

    std::vector<std::unique_ptr<Entry>> created;
    created.reserve(size_t(last - first + 1));
    for(int row = first; row <= last; ++row) {
        created.push_back(createEntry(m_source->marker(row)));
    }
    m_entries.insert(m_entries.begin() + first,
                     std::make_move_iterator(created.begin()),
                     std::make_move_iterator(created.end()));

    QStringList newGroups;
    for(int row = first; row <= last; ++row) {
        Entry* entry = m_entries.at(size_t(row)).get();
        if(joinGroup(entry)) {
            newGroups.append(entry->marker.visibilityGroup);
        }
        createDelegate(*entry);
    }

    // After every delegate exists: adding the items resolves their visibility at
    // once, which calls back into setGroupKeywordVisible.
    ensureKeywordItems(newGroups);
    refreshEntries(first, last);
}

void cwMarkerView::removeEntries(int first, int last)
{
    if(first > last || first < 0 || last >= count()) {
        return;
    }

    for(int row = last; row >= first; --row) {
        Entry* entry = m_entries.at(size_t(row)).get();
        destroyEntry(*entry);
        leaveGroup(entry, entry->marker.visibilityGroup);
    }
    // Erasing the entries' handles removes their sprites and text lines from the layers.
    m_entries.erase(m_entries.begin() + first, m_entries.begin() + last + 1);
}

/**
 * Updates the entries of the source's rows @a first to @a last in place: their
 * delegates keep their identity (and an open popup), and their keyword items
 * follow a group, extension or keyword change.
 */
void cwMarkerView::changeEntries(int first, int last)
{
    if(first > last || first < 0 || last >= count()) {
        return;
    }

    QStringList needsKeywordItem;
    for(int row = first; row <= last; ++row) {
        Entry& entry = *m_entries.at(size_t(row));
        const cwMarker old = entry.marker;
        entry.marker = m_source->marker(row);
        const cwMarker& now = entry.marker;
        const QString& group = now.visibilityGroup;

        if(old.visibilityGroup != group) {
            leaveGroup(&entry, old.visibilityGroup);
            if(joinGroup(&entry)) {
                needsKeywordItem.append(group);
            }
            if(!entry.item.isNull()) {
                entry.item->setVisible(isGroupVisible(group));
            }
        } else if(old.keywordExtension != now.keywordExtension) {
            // The keyword item extends the old model's keywords; build a new one.
            if(!needsKeywordItem.contains(group)) {
                rebuildKeywordItem(group);
                needsKeywordItem.append(group);
            }
        } else if(old.keywords != now.keywords) {
            replaceKeywords(group, old.keywords, now.keywords);
        }

        if(old.textLines != now.textLines) {
            entry.lineSizesValid = false;
            entry.lines.resize(size_t(now.textLines.size()));
        }

        updateDelegate(entry, old);
    }

    ensureKeywordItems(needsKeywordItem);
    refreshEntries(first, last);
}

/**
 * Projects and draws the rows @a first to @a last, or reruns the scene's
 * declutter pass when this view takes part in it.
 */
void cwMarkerView::refreshEntries(int first, int last)
{
    if(camera() != nullptr && isVisible()) {
        const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();
        for(int row = first; row <= last; ++row) {
            projectItem(*m_entries.at(size_t(row)), matrix);
        }
    }

    if(labelDeclutter() != nullptr) {
        refreshDeclutter();
        return;
    }

    prepareLayers();
    for(int row = first; row <= last; ++row) {
        updateBillboards(*m_entries.at(size_t(row)));
    }
}

void cwMarkerView::refreshDeclutter()
{
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->invalidate();
        declutter->ensurePass(camera());
    }
}

void cwMarkerView::emitCountChangedFrom(int oldCount)
{
    if(count() != oldCount) {
        emit countChanged();
    }
}

std::unique_ptr<cwMarkerView::Entry> cwMarkerView::createEntry(const cwMarker& marker)
{
    auto entry = std::make_unique<Entry>();
    entry->marker = marker;
    entry->lines.resize(size_t(marker.textLines.size()));
    return entry;
}

void cwMarkerView::destroyEntry(Entry& entry)
{
    clearSelectionIfIn(entry);
    if(!entry.markerItem.isNull()) {
        disconnect(entry.markerItem, nullptr, this, nullptr);
        m_entryByMarkerItem.remove(entry.markerItem);
    }
    if(!entry.item.isNull()) {
        // The tap target leaves the view at once; deletion waits for the event
        // loop, since the removal may run inside the delegate's own handler.
        entry.item->setParentItem(nullptr);
        entry.item->setParent(nullptr);
        entry.item->deleteLater();
    }
}

/**
 * Creates the entry's delegate with the marker's delegateProperties, plus this
 * view as markerView and the selection manager. The delegate's "marker" item
 * drives the sprite instance: its effective visibility, its opacity and its
 * optional spriteIndex property.
 */
void cwMarkerView::createDelegate(Entry& entry)
{
    if(m_delegate.isNull()) {
        return;
    }

    QVariantMap properties = entry.marker.delegateProperties;
    properties.insert(QStringLiteral("markerView"), QVariant::fromValue(this));
    properties.insert(QStringLiteral("selectionManager"), QVariant::fromValue(m_selectionManager.data()));

    QQmlContext* context = m_delegate->creationContext();
    if(context == nullptr) {
        context = QQmlEngine::contextForObject(this);
    }

    QObject* object = m_delegate->createWithInitialProperties(properties, context);
    auto item = qobject_cast<QQuickItem*>(object);
    if(item == nullptr) {
        qWarning() << "MarkerView's delegate didn't create an item. THIS IS A BUG!"
                   << m_delegate->errorString() << LOCATION;
        delete object;
        return;
    }

    // Parented to this view so the delegate shares its viewport coordinates.
    item->setParentItem(this);
    item->setParent(this);
    if(auto positionItem = qobject_cast<cwPositionItem*>(item)) {
        positionItem->setPosition3D(entry.marker.position);
    }
    // A marker added to a group a keyword filter already hides starts hidden.
    item->setVisible(isGroupVisible(entry.marker.visibilityGroup));
    entry.item = item;

    QQuickItem* markerItem = item->property("marker").value<QQuickItem*>();
    if(markerItem == nullptr) {
        return;
    }
    entry.markerItem = markerItem;
    m_entryByMarkerItem.insert(markerItem, &entry);
    connect(markerItem, &QQuickItem::visibleChanged, this, &cwMarkerView::markerItemChanged);
    connect(markerItem, &QQuickItem::opacityChanged, this, &cwMarkerView::markerItemChanged);

    // spriteIndex is declared in QML, so its notifier is only reachable
    // through the meta-object.
    const QMetaObject* markerMeta = markerItem->metaObject();
    const int signalIndex = markerMeta->indexOfSignal("spriteIndexChanged()");
    if(signalIndex >= 0) {
        const int slotIndex = staticMetaObject.indexOfSlot("markerItemChanged()");
        connect(markerItem, markerMeta->method(signalIndex), this, staticMetaObject.method(slotIndex));
    }
}

void cwMarkerView::updateDelegate(Entry& entry, const cwMarker& oldMarker)
{
    if(entry.item.isNull()) {
        return;
    }

    const QVariantMap& properties = entry.marker.delegateProperties;
    for(auto it = properties.cbegin(); it != properties.cend(); ++it) {
        if(oldMarker.delegateProperties.value(it.key()) != it.value()) {
            entry.item->setProperty(it.key().toUtf8().constData(), it.value());
        }
    }
    if(auto positionItem = qobject_cast<cwPositionItem*>(entry.item.data())) {
        positionItem->setPosition3D(entry.marker.position);
    }
}

// Returns true when @a entry starts its group.
bool cwMarkerView::joinGroup(Entry* entry)
{
    Group& group = m_groups[entry->marker.visibilityGroup];
    group.entries.append(entry);
    return group.entries.size() == 1;
}

// Drops @a group's keyword item with its last marker.
void cwMarkerView::leaveGroup(Entry* entry, const QString& group)
{
    const auto it = m_groups.find(group);
    if(it == m_groups.end()) {
        return;
    }
    it->entries.removeOne(entry);
    if(it->entries.isEmpty()) {
        m_keywordRegistry.drop(group);
        m_groups.erase(it);
    }
}

bool cwMarkerView::isGroupVisible(const QString& group) const
{
    const auto it = m_groups.constFind(group);
    return it == m_groups.constEnd() || it->keywordVisible;
}

/**
 * Registers one keyword item per group in @a groups, built from the group's
 * first marker, in one model insert.
 */
void cwMarkerView::ensureKeywordItems(const QStringList& groups)
{
    if(groups.isEmpty() || m_keywordRegistry.model() == nullptr) {
        return;
    }

    const QList<cwKeywordItem*> items = m_keywordRegistry.ensureAll(groups, [this](const QString& group) {
        const cwMarker& marker = m_groups.constFind(group)->entries.constFirst()->marker;
        auto item = new cwKeywordItem();
        if(!marker.keywordExtension.isNull()) {
            item->keywordModel()->addExtension(marker.keywordExtension);
        }
        for(const cwKeyword& keyword : marker.keywords) {
            item->keywordModel()->add(keyword);
        }
        item->setObject(new cwMarkerVisibility(this, group, item));
        return item;
    });

    for(int i = 0; i < items.size(); ++i) {
        const auto it = m_groups.find(groups.at(i));
        if(it != m_groups.end()) {
            it->keywordItem = items.at(i);
        }
    }
}

void cwMarkerView::rebuildKeywordItem(const QString& group)
{
    m_keywordRegistry.drop(group);
    const auto it = m_groups.find(group);
    if(it == m_groups.end()) {
        return;
    }
    it->keywordItem = nullptr;
    // The new item's filter decides from a shown group.
    if(!it->keywordVisible) {
        setGroupKeywordVisible(group, true);
    }
}

void cwMarkerView::replaceKeywords(const QString& group, const QList<cwKeyword>& oldKeywords,
                                   const QList<cwKeyword>& keywords)
{
    const auto it = m_groups.constFind(group);
    if(it == m_groups.constEnd() || it->keywordItem.isNull()) {
        return;
    }
    for(const cwKeyword& keyword : keywords) {
        if(!oldKeywords.contains(keyword)) {
            it->keywordItem->keywordModel()->replace(keyword);
        }
    }
}

QPointF cwMarkerView::spriteAnchor() const
{
    return m_source.isNull() ? cwRenderSpriteBillboards::kDefaultAnchor : m_source->spriteAnchor();
}

float cwMarkerView::depthBias() const
{
    return m_source.isNull() ? 0.0f : m_source->depthBias();
}

const cwMarkerTextStyle* cwMarkerView::style(const QString& styleKey) const
{
    for(const cwMarkerTextStyle* style : m_textStyles) {
        if(style != nullptr && style->styleKey() == styleKey) {
            return style;
        }
    }
    return nullptr;
}

cwRenderTextBillboards* cwMarkerView::textLayer(const cwMarkerTextStyle* style)
{
    return ensureRenderTextBillboards(style->layerKey().isEmpty() ? layerKey() : style->layerKey());
}

// The layer that draws @a line, or null when the line draws nothing: it is
// empty, its style is missing, or the layers aren't available yet.
cwRenderTextBillboards* cwMarkerView::lineLayer(const cwMarkerTextLine& line)
{
    if(line.text.isEmpty()) {
        return nullptr;
    }
    const cwMarkerTextStyle* lineStyle = style(line.styleKey);
    return lineStyle != nullptr ? textLayer(lineStyle) : nullptr;
}

/**
 * Pushes each text style to its layer, fetching the layers in list order, which
 * is their draw order. Returns true when any layer's style changed.
 */
bool cwMarkerView::applyStyles()
{
    bool changed = false;
    for(const cwMarkerTextStyle* textStyle : std::as_const(m_textStyles)) {
        if(textStyle != nullptr) {
            changed |= pushStyle(textLayer(textStyle), textStyle);
        }
    }
    if(changed) {
        for(const auto& entry : m_entries) {
            entry->lineSizesValid = false;
        }
    }
    return changed;
}

/**
 * Lays out the entry's lines once per text or style change, so a declutter
 * pass reads sizes without laying out hidden text every frame. A line that
 * draws nothing has an empty size.
 */
void cwMarkerView::measureLines(Entry& entry)
{
    if(entry.lineSizesValid) {
        return;
    }

    const QList<cwMarkerTextLine>& lines = entry.marker.textLines;
    entry.lineSizes.resize(lines.size());
    bool measured = true;
    for(int i = 0; i < lines.size(); ++i) {
        const cwMarkerTextLine& line = lines.at(i);
        cwRenderTextBillboards* layer = lineLayer(line);
        if(layer == nullptr && !line.text.isEmpty() && style(line.styleKey) != nullptr) {
            // Its layer arrives with the window and scene; measure again then.
            measured = false;
        }
        entry.lineSizes[i] = layer != nullptr ? layer->textSize(line.text) : QSizeF();
    }
    entry.lineSizesValid = measured;
}

void cwMarkerView::projectItem(const Entry& entry, const QMatrix4x4& matrix)
{
    if(entry.item.isNull()) {
        return;
    }
    const QVector3D screen = matrix.map(entry.marker.position);
    entry.item->setPosition(QPointF(screen.x(), screen.y()));
}

void cwMarkerView::projectItems()
{
    if(camera() == nullptr || !isVisible()) {
        return;
    }

    const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();
    for(const auto& entry : m_entries) {
        projectItem(*entry, matrix);
    }
}

void cwMarkerView::prepareLayers()
{
    if(cwRenderSpriteBillboards* sprites = ensureRenderSpriteBillboards()) {
        const QPointF anchor = spriteAnchor();
        if(sprites->anchor() != anchor) {
            sprites->setAnchor(anchor);
        }
    }
    applyStyles();
}

bool cwMarkerView::isEntryVisible(const Entry& entry) const
{
    if(!entry.markerItem.isNull()) {
        return entry.markerItem->isVisible();
    }
    return isGroupVisible(entry.marker.visibilityGroup) && isVisible();
}

void cwMarkerView::updateSprite(Entry& entry, bool visible)
{
    const QQuickItem* markerItem = entry.markerItem;
    const float opacity = markerItem != nullptr ? float(markerItem->opacity()) : kFullOpacity;
    const QVariant spriteIndex = markerItem != nullptr ? markerItem->property("spriteIndex") : QVariant();
    const int slot = spriteIndex.isValid() ? spriteIndex.toInt() : entry.marker.spriteIndex;
    setSpriteBillboard(entry.sprite, entry.marker.position, depthBias(), opacity, visible, slot);
}

/**
 * Stacks the entry's lines m_textGap above its sprite, the last line nearest
 * the sprite. Each line centers on its offset point, y-up.
 */
void cwMarkerView::updateText(Entry& entry)
{
    measureLines(entry);

    const QList<cwMarkerTextLine>& lines = entry.marker.textLines;
    qreal blockBottom = spriteSize().height() * spriteAnchor().y() + m_textGap;
    for(int i = int(lines.size()) - 1; i >= 0; --i) {
        const cwMarkerTextLine& line = lines.at(i);
        cwTextBillboardHandle& handle = entry.lines.at(size_t(i));
        cwRenderTextBillboards* layer = lineLayer(line);
        if(layer == nullptr) {
            handle.reset();
            continue;
        }

        const qreal lineHeight = entry.lineSizes.at(i).height();
        setTextBillboard(layer, handle, line.text, entry.marker.position, depthBias(),
                         QPointF(0.0, blockBottom + lineHeight * kHalf));
        blockBottom += lineHeight;
    }
}

// A text billboard has no visible flag, so a hidden line leaves its layer.
void cwMarkerView::releaseText(Entry& entry)
{
    for(cwTextBillboardHandle& line : entry.lines) {
        line.reset();
    }
}

/**
 * Outside the scene's declutter pass, draws the entry's sprite and text lines
 * as its visibility says. In the pass, only hides them at once; showing waits
 * for the one queued pass that decides where text has room.
 */
void cwMarkerView::updateBillboards(Entry& entry)
{
    const bool visible = isEntryVisible(entry);
    updateSprite(entry, visible);

    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        if(!visible) {
            releaseText(entry);
        }
        declutter->schedulePass();
    } else if(visible) {
        updateText(entry);
    } else {
        releaseText(entry);
    }
}

void cwMarkerView::updateAllBillboards()
{
    prepareLayers();
    for(const auto& entry : m_entries) {
        updateBillboards(*entry);
    }
}

void cwMarkerView::clearSelectionIfIn(const Entry& entry)
{
    if(!entry.item.isNull() && !m_selectionManager.isNull()
        && m_selectionManager->selectedItem() == entry.item) {
        m_selectionManager->setSelectedItem(nullptr);
    }
}

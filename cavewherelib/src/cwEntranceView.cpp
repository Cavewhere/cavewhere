/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEntranceView.h"
#include "cwBillboardGeometry.h"
#include "cwCamera.h"
#include "cwDebug.h"
#include "cwEntranceLabel.h"
#include "cwEntranceVisibility.h"
#include "cwFixStationModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwLength.h"
#include "cwPositionItem.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNode.h"

//Qt includes
#include <QMatrix4x4>
#include <QQmlComponent>
#include <QQmlContext>

namespace {
const QString kEntranceLayerKey = QStringLiteral("entrances");
const QString kEntranceStatsLayerKey = QStringLiteral("entranceStats");

// The pin's tip is the bottom center of its sprite, so the tip sits on the station.
const QPointF kPinAnchor(0.5, 1.0);

// Pulls pin and text toward the viewer, further than leads (1.0), so the marker
// at a surface station clears the terrain around it.
constexpr float kEntranceDepthBias = 2.0f;
constexpr float kPinOpacity = 1.0f;
constexpr qreal kHalf = 0.5;

// Returns true when @a layer's style changed.
bool pushStyle(cwRenderTextBillboards* layer, const QFont& font, const QColor& color,
               const QColor& outlineColor)
{
    if(layer == nullptr) {
        return false;
    }

    cwRenderTextBillboards::Style style;
    style.font = font;
    style.color = color;
    style.outlineColor = outlineColor;
    if(layer->style() == style) {
        return false;
    }
    layer->setStyle(style);
    return true;
}

double toMeters(const cwLength* length)
{
    return cwUnits::convert(length->value(),
                            static_cast<cwUnits::LengthUnit>(length->unit()),
                            cwUnits::Meters);
}
}

cwEntranceView::cwEntranceView(QQuickItem* parent) :
    cwBillboardOverlayItem(kEntranceLayerKey, parent),
    m_selectionManager(new cwSelectionManager(this))
{
}

cwEntranceView::~cwEntranceView()
{
    // The EntrancePoints outlive m_entries during teardown; a marker visibility
    // change from them must not reach markerVisibleChanged then.
    for(auto& [id, entry] : m_entries) {
        if(entry.item.isNull()) {
            continue;
        }
        if(auto marker = entry.item->property("marker").value<QQuickItem*>()) {
            disconnect(marker, nullptr, this, nullptr);
        }
    }
}

cwCavingRegion* cwEntranceView::region() const
{
    return m_region;
}

void cwEntranceView::setRegion(cwCavingRegion* region)
{
    if(m_region == region) {
        return;
    }
    disconnectRegion();
    m_region = region;
    connectRegion();
    scheduleRebuild();
    emit regionChanged();
}

cwKeywordItemModel* cwEntranceView::keywordItemModel() const
{
    return m_keywordRegistry.model();
}

void cwEntranceView::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if(m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    // Tears down the items in the old model; every drawn fix registers again
    // against the new one.
    m_keywordRegistry.setModel(keywordItemModel);

    QList<QUuid> ids;
    ids.reserve(int(m_entries.size()));
    for(auto& [id, entry] : m_entries) {
        entry.keywordItem = nullptr;
        ids.append(id);
    }
    ensureKeywordItems(ids);

    emit keywordItemModelChanged();
}

cwSelectionManager* cwEntranceView::selectionManager() const
{
    return m_selectionManager;
}

QFont cwEntranceView::font() const
{
    return m_font;
}

void cwEntranceView::setFont(const QFont& font)
{
    if(m_font == font) {
        return;
    }
    m_font = font;
    emit fontChanged();
    restyle();
}

QColor cwEntranceView::color() const
{
    return m_color;
}

void cwEntranceView::setColor(const QColor& color)
{
    if(m_color == color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    restyle();
}

QColor cwEntranceView::outlineColor() const
{
    return m_outlineColor;
}

void cwEntranceView::setOutlineColor(const QColor& outlineColor)
{
    if(m_outlineColor == outlineColor) {
        return;
    }
    m_outlineColor = outlineColor;
    emit outlineColorChanged();
    restyle();
}

QFont cwEntranceView::statsFont() const
{
    return m_statsFont;
}

void cwEntranceView::setStatsFont(const QFont& statsFont)
{
    if(m_statsFont == statsFont) {
        return;
    }
    m_statsFont = statsFont;
    emit statsFontChanged();
    restyle();
}

QColor cwEntranceView::statsColor() const
{
    return m_statsColor;
}

void cwEntranceView::setStatsColor(const QColor& statsColor)
{
    if(m_statsColor == statsColor) {
        return;
    }
    m_statsColor = statsColor;
    emit statsColorChanged();
    restyle();
}

QColor cwEntranceView::statsOutlineColor() const
{
    return m_statsOutlineColor;
}

void cwEntranceView::setStatsOutlineColor(const QColor& statsOutlineColor)
{
    if(m_statsOutlineColor == statsOutlineColor) {
        return;
    }
    m_statsOutlineColor = statsOutlineColor;
    emit statsOutlineColorChanged();
    restyle();
}

QString cwEntranceView::statsLayerKey() const
{
    return kEntranceStatsLayerKey;
}

qreal cwEntranceView::textGap() const
{
    return m_textGap;
}

void cwEntranceView::setTextGap(qreal textGap)
{
    if(m_textGap == textGap) {
        return;
    }
    m_textGap = textGap;
    emit textGapChanged();
    updateAllBillboards();
}

int cwEntranceView::entranceCount() const
{
    return int(m_entries.size());
}

void cwEntranceView::setFixKeywordVisible(const QUuid& fixId, bool visible)
{
    auto it = m_entries.find(fixId);
    if(it == m_entries.end()) {
        return;
    }

    Entry& entry = it->second;
    entry.keywordVisible = visible;
    if(!entry.item.isNull()) {
        // A hidden item also takes its tap target out of reach.
        entry.item->setVisible(visible);
    }
    if(!visible) {
        clearSelectionIfIn(entry);
    }
    updateBillboards(entry);
}

void cwEntranceView::scheduleRebuild()
{
    if(m_rebuildPending) {
        return;
    }
    m_rebuildPending = true;
    QMetaObject::invokeMethod(this, &cwEntranceView::rebuild, Qt::QueuedConnection);
}

void cwEntranceView::connectRegion()
{
    if(m_region.isNull()) {
        return;
    }

    // The root relays both signals for every node below it, so one connection
    // each covers node inserts, removals and every node's solve.
    cwSurveyNode* root = m_region->rootNode();
    connect(root, &cwSurveyNode::subtreeChanged, this, &cwEntranceView::scheduleRebuild);
    connect(root, &cwSurveyNode::scopedPositionsChanged, this, &cwEntranceView::scheduleRebuild);
    connect(m_region, &cwCavingRegion::unitSystemChanged, this, &cwEntranceView::scheduleRebuild);
    connect(m_region, &QObject::destroyed, this, &cwEntranceView::scheduleRebuild);
}

void cwEntranceView::disconnectRegion()
{
    if(!m_region.isNull()) {
        disconnect(m_region->rootNode(), nullptr, this, nullptr);
        disconnect(m_region, nullptr, this, nullptr);
    }

    for(cwSurveyNode* node : std::as_const(m_connectedNodes)) {
        disconnectNode(node);
    }
    m_connectedNodes.clear();
}

void cwEntranceView::syncNodeConnections()
{
    const QList<cwSurveyNode*> current = m_region.isNull()
                                             ? QList<cwSurveyNode*>()
                                             : m_region->rootNode()->allNodes();
    const QSet<cwSurveyNode*> currentSet(current.begin(), current.end());

    // Every node still in m_connectedNodes is alive: nodeDestroyed drops the others.
    for(auto it = m_connectedNodes.begin(); it != m_connectedNodes.end();) {
        if(currentSet.contains(*it)) {
            ++it;
        } else {
            disconnectNode(*it);
            it = m_connectedNodes.erase(it);
        }
    }

    for(cwSurveyNode* node : current) {
        if(m_connectedNodes.contains(node)) {
            continue;
        }
        m_connectedNodes.insert(node);

        connect(node, &QObject::destroyed, this, &cwEntranceView::nodeDestroyed);
        connect(node, &cwSurveyNode::nameChanged, this, &cwEntranceView::scheduleRebuild);
        connect(node->length(), &cwLength::valueChanged, this, &cwEntranceView::scheduleRebuild);
        connect(node->length(), &cwLength::unitChanged, this, &cwEntranceView::scheduleRebuild);
        connect(node->depth(), &cwLength::valueChanged, this, &cwEntranceView::scheduleRebuild);
        connect(node->depth(), &cwLength::unitChanged, this, &cwEntranceView::scheduleRebuild);

        cwFixStationModel* fixes = node->fixStations();
        connect(fixes, &QAbstractItemModel::rowsInserted, this, &cwEntranceView::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::rowsRemoved, this, &cwEntranceView::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::dataChanged, this, &cwEntranceView::scheduleRebuild);
        connect(fixes, &QAbstractItemModel::modelReset, this, &cwEntranceView::scheduleRebuild);
    }
}

void cwEntranceView::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, nullptr, this, nullptr);
    disconnect(node->length(), nullptr, this, nullptr);
    disconnect(node->depth(), nullptr, this, nullptr);
    disconnect(node->fixStations(), nullptr, this, nullptr);
}

void cwEntranceView::nodeDestroyed(QObject* node)
{
    // Only the address is used; the node is mid-destruction.
    m_connectedNodes.remove(static_cast<cwSurveyNode*>(node));
    scheduleRebuild();
}

/**
 * The fixes to draw: a node's own fixes whose coordinate places them and whose
 * station the solve placed below that node.
 */
QList<std::pair<QUuid, cwEntranceView::Desired>> cwEntranceView::desiredEntrances() const
{
    QList<std::pair<QUuid, Desired>> desired;
    if(m_region.isNull()) {
        return desired;
    }

    const cwUnits::UnitSystem unitSystem = m_region->unitSystem();
    const QList<cwSurveyNode*> nodes = m_region->rootNode()->allNodes();
    for(cwSurveyNode* node : nodes) {
        const QList<cwFixStation>& fixes = node->fixStations()->fixStations();
        if(fixes.isEmpty()) {
            continue;
        }

        const cwStationPositionLookup positions = node->scopedPositions();
        const double lengthMeters = toMeters(node->length());
        const double depthMeters = toMeters(node->depth());
        for(const cwFixStation& fix : fixes) {
            if(!fix.hasPlacedCoordinate() || !positions.hasPosition(fix.stationName())) {
                continue;
            }

            Desired entry;
            entry.node = node;
            entry.stationName = fix.stationName();
            entry.position = positions.position(fix.stationName());
            entry.kind = fix.kind();
            entry.displayName = fix.label().isEmpty() ? node->name() : fix.label();
            entry.statsText = cwEntranceLabel::statsText(entry.kind, lengthMeters, depthMeters,
                                                         unitSystem);
            desired.append({fix.id(), entry});
        }
    }
    return desired;
}

/**
 * The one pass every model, solve, name, size and unit change funnels into. It
 * diffs the drawn fixes against desiredEntrances(): removed fixes lose their pin,
 * text lines, tap target and keyword item, new ones gain them, and changed ones update
 * in place.
 */
void cwEntranceView::rebuild()
{
    m_rebuildPending = false;
    syncNodeConnections();

    const QList<std::pair<QUuid, Desired>> desired = desiredEntrances();
    QSet<QUuid> desiredIds;
    desiredIds.reserve(desired.size());
    for(const auto& [id, entry] : desired) {
        desiredIds.insert(id);
    }

    bool changed = false;

    for(auto it = m_entries.begin(); it != m_entries.end();) {
        if(desiredIds.contains(it->first)) {
            ++it;
            continue;
        }
        clearSelectionIfIn(it->second);
        if(!it->second.item.isNull()) {
            it->second.item->deleteLater();
        }
        m_keywordRegistry.drop(it->first);
        // Erasing the entry's handles removes its pin and both text lines from the layers.
        it = m_entries.erase(it);
        changed = true;
    }

    QList<QUuid> needsKeywordItem;
    for(const auto& [id, want] : desired) {
        auto it = m_entries.find(id);
        if(it == m_entries.end()) {
            Entry entry;
            entry.node = want.node;
            entry.position = want.position;
            entry.displayName = want.displayName;
            entry.statsText = want.statsText;
            entry.kind = want.kind;
            entry.item = createItem(id, want);
            m_entries.emplace(id, std::move(entry));
            needsKeywordItem.append(id);
            changed = true;
            continue;
        }

        Entry& entry = it->second;
        if(entry.node != want.node) {
            // The keyword item extends the old node's keywords; build a new one.
            m_keywordRegistry.drop(id);
            entry.keywordItem = nullptr;
            entry.node = want.node;
            if(!entry.item.isNull()) {
                entry.item->setProperty("node", QVariant::fromValue(want.node));
            }
            needsKeywordItem.append(id);
        }
        if(entry.kind != want.kind) {
            entry.kind = want.kind;
            replaceKeyword(entry, cwKeywordModel::TypeKey, cwFixStation::kindName(want.kind));
            if(!entry.item.isNull()) {
                entry.item->setProperty("kind", static_cast<int>(want.kind));
                entry.item->setProperty("kindName", cwFixStation::kindName(want.kind));
            }
            changed = true;
        }
        if(entry.displayName != want.displayName) {
            entry.displayName = want.displayName;
            replaceKeyword(entry, cwKeywordModel::NameKey, want.displayName);
            if(!entry.item.isNull()) {
                entry.item->setProperty("displayName", want.displayName);
            }
            changed = true;
        }
        if(entry.statsText != want.statsText) {
            entry.statsText = want.statsText;
            changed = true;
        }
        entry.position = want.position;
        if(auto positionItem = qobject_cast<cwPositionItem*>(entry.item.data())) {
            positionItem->setPosition3D(want.position);
            positionItem->setProperty("stationName", want.stationName);
        }
    }

    // After every entry and item exists: adding the items resolves their
    // visibility at once, which calls back into setFixKeywordVisible.
    ensureKeywordItems(needsKeywordItem);

    projectItems();
    updateAllBillboards();

    if(changed) {
        emit entrancesChanged();
    }
}

void cwEntranceView::ensureKeywordItems(const QList<QUuid>& ids)
{
    if(ids.isEmpty() || m_keywordRegistry.model() == nullptr) {
        return;
    }

    // Each item gets its keywords before it joins the model, so the model sees
    // one insert per pass rather than one per keyword.
    const QList<cwKeywordItem*> items = m_keywordRegistry.ensureAll(ids, [this](const QUuid& id) {
        const Entry& entry = m_entries.at(id);
        auto item = new cwKeywordItem();
        if(!entry.node.isNull()) {
            item->keywordModel()->addExtension(entry.node->keywordModel());
        }
        item->keywordModel()->add(cwKeyword(cwKeywordModel::TypeKey, cwFixStation::kindName(entry.kind)));
        item->keywordModel()->add(cwKeyword(cwKeywordModel::NameKey, entry.displayName));
        item->setObject(new cwEntranceVisibility(this, id, item));
        return item;
    });

    for(int i = 0; i < items.size(); ++i) {
        auto it = m_entries.find(ids.at(i));
        if(it != m_entries.end()) {
            it->second.keywordItem = items.at(i);
        }
    }
}

void cwEntranceView::replaceKeyword(Entry& entry, const QString& key, const QString& value)
{
    if(!entry.keywordItem.isNull()) {
        entry.keywordItem->keywordModel()->replace(cwKeyword(key, value));
    }
}

QQuickItem* cwEntranceView::createItem(const QUuid& fixId, const Desired& desired)
{
    createComponent();
    if(m_itemComponent == nullptr) {
        return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(
        m_itemComponent->createWithInitialProperties(
            {{QStringLiteral("selectionManager"), QVariant::fromValue(m_selectionManager)},
             {QStringLiteral("entranceView"), QVariant::fromValue(this)},
             {QStringLiteral("node"), QVariant::fromValue(desired.node)},
             {QStringLiteral("stationName"), desired.stationName},
             {QStringLiteral("fixId"), fixId.toString(QUuid::WithoutBraces)},
             {QStringLiteral("displayName"), desired.displayName},
             {QStringLiteral("kind"), static_cast<int>(desired.kind)},
             {QStringLiteral("kindName"), cwFixStation::kindName(desired.kind)}},
            context));
    if(item == nullptr) {
        qDebug() << "EntrancePoint.qml didn't compile. THIS IS A BUG!" << m_itemComponent->errorString() << LOCATION;
        return nullptr;
    }

    item->setParentItem(this);
    item->setParent(this);
    if(auto positionItem = qobject_cast<cwPositionItem*>(item)) {
        positionItem->setPosition3D(desired.position);
    }

    // The pin and text follow the marker's effective visibility: a keyword
    // filter or this view hiding takes them out of the draw.
    if(auto marker = item->property("marker").value<QQuickItem*>()) {
        connect(marker, &QQuickItem::visibleChanged, this, &cwEntranceView::markerVisibleChanged);
    }
    return item;
}

void cwEntranceView::createComponent()
{
    if(m_itemComponent != nullptr) {
        return;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    if(context == nullptr) {
        qDebug() << "EntranceView has no QML context, so it cannot create EntrancePoints. THIS IS A BUG" << LOCATION;
        return;
    }

    m_itemComponent = new QQmlComponent(context->engine(),
                                        QStringLiteral("qrc:/qt/qml/cavewherelib/qml/EntrancePoint.qml"),
                                        this);
    if(m_itemComponent->isError()) {
        qDebug() << "EntrancePoint errors:" << m_itemComponent->errorString();
    }
}

/**
 * Pushes this view's name style to its keyed text layer and its stats style to
 * the stats layer. Returns true when either layer's style changed.
 */
bool cwEntranceView::applyStyle()
{
    // The stats layer is fetched only after the name layer, which keeps the
    // layers' registration (draw) order: pins, names, stats.
    cwRenderTextBillboards* names = renderTextBillboards();
    if(names == nullptr) {
        return false;
    }
    const bool nameChanged = pushStyle(names, m_font, m_color, m_outlineColor);
    const bool statsChanged = pushStyle(ensureRenderTextBillboards(kEntranceStatsLayerKey),
                                        m_statsFont, m_statsColor, m_statsOutlineColor);
    return nameChanged || statsChanged;
}

void cwEntranceView::restyle()
{
    if(applyStyle()) {
        // A line height moved, and with it each line's offset above its pin.
        updateAllBillboards();
    }
}

void cwEntranceView::repositionBillboards()
{
    projectItems();
    updateAllBillboards();
}

void cwEntranceView::projectItems()
{
    if(camera() == nullptr || !isVisible()) {
        return;
    }

    const QMatrix4x4 matrix = camera()->qtViewportMatrix() * camera()->viewProjectionMatrix();
    for(auto& [id, entry] : m_entries) {
        if(entry.item.isNull()) {
            continue;
        }
        const QVector3D screen = matrix.map(entry.position);
        entry.item->setPosition(QPointF(screen.x(), screen.y()));
    }
}

void cwEntranceView::updateAllBillboards()
{
    if(cwRenderSpriteBillboards* pins = ensureRenderSpriteBillboards()) {
        pins->setAnchor(kPinAnchor);
    }
    if(ensureRenderTextBillboards() != nullptr) {
        applyStyle();
    }

    for(auto& [id, entry] : m_entries) {
        updateBillboards(entry);
    }
}

void cwEntranceView::updateBillboards(Entry& entry)
{
    const QQuickItem* marker = entry.item.isNull()
                                   ? nullptr
                                   : entry.item->property("marker").value<QQuickItem*>();
    const bool visible = marker != nullptr ? marker->isVisible()
                                           : entry.keywordVisible && isVisible();

    setSpriteBillboard(entry.pinBillboard, entry.position, kEntranceDepthBias, kPinOpacity,
                       visible, int(entry.kind));

    // A text billboard has no visible flag, so a hidden fix's lines leave their layers.
    cwRenderTextBillboards* names = renderTextBillboards();
    cwRenderTextBillboards* stats = names != nullptr
                                        ? ensureRenderTextBillboards(kEntranceStatsLayerKey)
                                        : nullptr;
    if(!visible || stats == nullptr) {
        entry.nameBillboard.reset();
        entry.statsBillboard.reset();
        return;
    }

    // Each line centers on its offset point. The block of lines sits m_textGap
    // above the pin: the stats line at its bottom, the name directly above it.
    qreal blockBottom = spriteSize().height() + m_textGap;
    if(entry.statsText.isEmpty()) {
        entry.statsBillboard.reset();
    } else {
        const qreal statsHeight = stats->textSize(entry.statsText).height();
        setTextBillboard(stats, entry.statsBillboard, entry.statsText, entry.position,
                         kEntranceDepthBias, QPointF(0.0, blockBottom + statsHeight * kHalf));
        blockBottom += statsHeight;
    }

    const qreal nameHeight = names->textSize(entry.displayName).height();
    setTextBillboard(names, entry.nameBillboard, entry.displayName, entry.position,
                     kEntranceDepthBias, QPointF(0.0, blockBottom + nameHeight * kHalf));
}

void cwEntranceView::select(cwSurveyNode* node, const QUuid& fixId)
{
    const auto it = m_entries.find(fixId);
    if(it == m_entries.end()) {
        return;
    }

    const Entry& entry = it->second;
    if(entry.node == node && !entry.item.isNull()) {
        m_selectionManager->setSelectedItem(entry.item);
    }
}

bool cwEntranceView::isOccluded(const QVector3D& worldPosition,
                                const QPointF& tapViewportPosition) const
{
    return cw::billboard::isOccluded(scene() ? scene()->geometryItersecter() : nullptr,
                                     camera(), worldPosition, tapViewportPosition,
                                     spriteSize(), kPinAnchor, kEntranceDepthBias);
}

void cwEntranceView::clearSelectionIfIn(const Entry& entry)
{
    if(!entry.item.isNull() && m_selectionManager->selectedItem() == entry.item) {
        m_selectionManager->setSelectedItem(nullptr);
    }
}

void cwEntranceView::markerVisibleChanged()
{
    auto marker = qobject_cast<QQuickItem*>(sender());
    QQuickItem* item = marker != nullptr ? marker->parentItem() : nullptr;
    if(item == nullptr) {
        return;
    }

    auto it = m_entries.find(QUuid::fromString(item->property("fixId").toString()));
    if(it != m_entries.end() && it->second.item == item) {
        updateBillboards(it->second);
    }
}

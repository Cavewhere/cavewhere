/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBrushStructureModel.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"

//Std includes
#include <algorithm>
#include <limits>

namespace {

// A QModelIndex's internalId packs which kind of node it is and the coordinates
// parent()/data() need to resolve it without walking the tree:
//   bits [0, 1]   node kind (layer / category / rule)
//   bits [2, 32]  owning layer index
//   bits [33, 63] owning category index (rule nodes only)
// A node's own row lives in QModelIndex::row(); the id only carries what its row
// can't (the ancestry above it).
constexpr quintptr kKindBits = 2;
constexpr quintptr kKindMask = (quintptr(1) << kKindBits) - 1;
constexpr quintptr kIndexBits = 31;
constexpr quintptr kIndexMask = (quintptr(1) << kIndexBits) - 1;

enum NodeKind : quintptr {
    LayerKind = 0,
    CategoryKind = 1,
    RuleKind = 2,
};

quintptr encodeId(NodeKind kind, int layerIndex, int categoryIndex)
{
    return static_cast<quintptr>(kind)
           | (static_cast<quintptr>(layerIndex) << kKindBits)
           | (static_cast<quintptr>(categoryIndex) << (kKindBits + kIndexBits));
}

NodeKind kindOf(const QModelIndex &index)
{
    return static_cast<NodeKind>(index.internalId() & kKindMask);
}

int encodedLayer(const QModelIndex &index)
{
    return static_cast<int>((index.internalId() >> kKindBits) & kIndexMask);
}

int encodedCategory(const QModelIndex &index)
{
    return static_cast<int>((index.internalId() >> (kKindBits + kIndexBits)) & kIndexMask);
}

// A rule's pipeline stage, or a sentinel past every real stage for an unknown
// rule, so unknowns group into a trailing block the layout drops.
int stageOf(const QString &name)
{
    const cwPlacementRule *rule = cwPlacementRuleRegistry::instance().rule(name);
    return rule != nullptr ? static_cast<int>(rule->stage())
                           : std::numeric_limits<int>::max();
}

} // namespace

cwBrushStructureModel::cwBrushStructureModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

QModelIndex cwBrushStructureModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    if (!parent.isValid()) {
        return createIndex(row, column, encodeId(LayerKind, 0, 0));   // a layer row
    }
    if (kindOf(parent) == LayerKind) {
        return createIndex(row, column, encodeId(CategoryKind, parent.row(), 0)); // a category row
    }
    if (kindOf(parent) == CategoryKind) {
        return createIndex(row, column, encodeId(RuleKind, encodedLayer(parent), parent.row())); // a rule row
    }
    return {};   // rule rows are leaves
}

QModelIndex cwBrushStructureModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return {};
    }
    switch (kindOf(index)) {
    case CategoryKind:
        return createIndex(encodedLayer(index), 0, encodeId(LayerKind, 0, 0));
    case RuleKind:
        return createIndex(encodedCategory(index), 0,
                           encodeId(CategoryKind, encodedLayer(index), 0));
    case LayerKind:
        break;
    }
    return {};
}

int cwBrushStructureModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return layerCount();
    }
    if (kindOf(parent) == LayerKind) {
        return static_cast<int>(categoryBlocks(parent.row()).size());
    }
    if (kindOf(parent) == CategoryKind) {
        const QVector<CategoryBlock> blocks = categoryBlocks(encodedLayer(parent));
        const int c = parent.row();
        return c >= 0 && c < blocks.size() ? blocks.at(c).count : 0;
    }
    return 0;   // rule rows are leaves
}

int cwBrushStructureModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant cwBrushStructureModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    if (kindOf(index) == LayerKind) {
        const int layerIndex = index.row();
        switch (role) {
        case Qt::DisplayRole:
        case LabelRole:        return layerLabel(layerIndex);
        case RuleEnabledRole:  return true;
        case IsLayerRole:      return true;
        case IsCategoryRole:   return false;
        case LayerIndexRole:   return layerIndex;
        case RuleIndexRole:    return -1;
        case CategoryRole:     return QString();
        case CategorySizeRole: return 0;
        default:               return {};
        }
    }

    if (kindOf(index) == CategoryKind) {
        const int layerIndex = encodedLayer(index);
        const QVector<CategoryBlock> blocks = categoryBlocks(layerIndex);
        const int c = index.row();
        if (c < 0 || c >= blocks.size()) {
            return {};
        }
        const CategoryBlock &block = blocks.at(c);
        const QString label =
            cwPlacementRule::categoryLabel(static_cast<cwPlacementRule::Stage>(block.stage));
        switch (role) {
        case Qt::DisplayRole:
        case LabelRole:
        case CategoryRole:     return label;
        case RuleEnabledRole:  return true;
        case IsLayerRole:      return false;
        case IsCategoryRole:   return true;
        case LayerIndexRole:   return layerIndex;
        case RuleIndexRole:    return -1;
        case CategorySizeRole: return block.count;
        case CategoryLastRole: return false;
        default:               return {};
        }
    }

    // A rule row: map (category, localRow) back to the flat rule index.
    const int layerIndex = encodedLayer(index);
    const int categoryIndex = encodedCategory(index);
    const QVector<CategoryBlock> blocks = categoryBlocks(layerIndex);
    if (categoryIndex < 0 || categoryIndex >= blocks.size()) {
        return {};
    }
    const CategoryBlock &block = blocks.at(categoryIndex);
    const int ruleIndex = block.firstRule + index.row();
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:        return ruleName(layerIndex, ruleIndex);
    case RuleEnabledRole:  return ruleEnabled(layerIndex, ruleIndex);
    case IsLayerRole:      return false;
    case IsCategoryRole:   return false;
    case LayerIndexRole:   return layerIndex;
    case RuleIndexRole:    return ruleIndex;
    case CategoryRole:     return ruleCategory(layerIndex, ruleIndex);
    case CategorySizeRole: return block.count;
    case CategoryLastRole: return index.row() == block.count - 1;
    default:               return {};
    }
}

QHash<int, QByteArray> cwBrushStructureModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {LabelRole, "label"},
        {RuleEnabledRole, "ruleEnabled"},
        {IsLayerRole, "isLayer"},
        {IsCategoryRole, "isCategory"},
        {LayerIndexRole, "layerIndex"},
        {RuleIndexRole, "ruleIndex"},
        {CategoryRole, "category"},
        {CategorySizeRole, "categorySize"},
        {CategoryLastRole, "categoryLast"},
    };
}

void cwBrushStructureModel::setBrush(const cwLineBrush &brush)
{
    beginResetModel();
    m_brush = brush;
    endResetModel();
}

int cwBrushStructureModel::layerCount() const
{
    return static_cast<int>(m_brush.decorations.size());
}

QString cwBrushStructureModel::layerLabel(int layerIndex) const
{
    if (!layerInRange(layerIndex)) {
        return QString();
    }
    return m_brush.decorations.at(layerIndex).glyphName;   // empty for a line layer
}

int cwBrushStructureModel::ruleCount(int layerIndex) const
{
    if (!layerInRange(layerIndex)) {
        return 0;
    }
    return static_cast<int>(m_brush.decorations.at(layerIndex).rules.size());
}

QString cwBrushStructureModel::ruleName(int layerIndex, int ruleIndex) const
{
    if (!layerInRange(layerIndex)) {
        return QString();
    }
    const cwDecorationLayer &layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return QString();
    }
    return layer.rules.at(ruleIndex).name;
}

bool cwBrushStructureModel::ruleEnabled(int layerIndex, int ruleIndex) const
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    const cwDecorationLayer &layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }
    return layer.rules.at(ruleIndex).enabled;
}

QString cwBrushStructureModel::ruleCategory(int layerIndex, int ruleIndex) const
{
    const QString name = ruleName(layerIndex, ruleIndex);
    if (name.isEmpty()) {
        return QString();
    }
    const cwPlacementRule *rule = cwPlacementRuleRegistry::instance().rule(name);
    return rule != nullptr ? cwPlacementRule::categoryLabel(rule->stage()) : QString();
}

int cwBrushStructureModel::layerIndexOf(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return -1;
    }
    switch (kindOf(index)) {
    case LayerKind:    return index.row();
    case CategoryKind:
    case RuleKind:     return encodedLayer(index);
    }
    return -1;
}

int cwBrushStructureModel::ruleIndexOf(const QModelIndex &index) const
{
    if (!index.isValid() || kindOf(index) != RuleKind) {
        return -1;
    }
    const QVector<CategoryBlock> blocks = categoryBlocks(encodedLayer(index));
    const int categoryIndex = encodedCategory(index);
    if (categoryIndex < 0 || categoryIndex >= blocks.size()) {
        return -1;
    }
    return blocks.at(categoryIndex).firstRule + index.row();
}

QVector<cwBrushStructureModel::CategoryBlock>
cwBrushStructureModel::categoryBlocks(int layerIndex) const
{
    QVector<CategoryBlock> blocks;
    if (!layerInRange(layerIndex)) {
        return blocks;
    }
    const cwDecorationLayer &layer = m_brush.decorations.at(layerIndex);
    for (int i = 0; i < layer.rules.size(); ++i) {
        const int s = stageOf(layer.rules.at(i).name);
        if (!blocks.isEmpty() && blocks.last().stage == s) {
            blocks.last().count += 1;
        } else {
            blocks.append(CategoryBlock{s, i, 1});
        }
    }
    return blocks;
}

bool cwBrushStructureModel::ruleLocation(int layerIndex, int ruleIndex,
                                         int *categoryIndex, int *localRow) const
{
    const QVector<CategoryBlock> blocks = categoryBlocks(layerIndex);
    for (int c = 0; c < blocks.size(); ++c) {
        const CategoryBlock &block = blocks.at(c);
        if (ruleIndex >= block.firstRule && ruleIndex < block.firstRule + block.count) {
            if (categoryIndex != nullptr) {
                *categoryIndex = c;
            }
            if (localRow != nullptr) {
                *localRow = ruleIndex - block.firstRule;
            }
            return true;
        }
    }
    return false;
}

void cwBrushStructureModel::emitLayerRuleRolesChanged(int layerIndex)
{
    const QModelIndex layerRow = index(layerIndex, 0, QModelIndex());
    const int categoryRows = rowCount(layerRow);
    for (int c = 0; c < categoryRows; ++c) {
        const QModelIndex categoryRow = index(c, 0, layerRow);
        const int n = rowCount(categoryRow);
        if (n > 0) {
            emit dataChanged(index(0, 0, categoryRow), index(n - 1, 0, categoryRow));
        }
    }
}

bool cwBrushStructureModel::setRuleEnabled(int layerIndex, int ruleIndex, bool enabled)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }
    if (layer.rules.at(ruleIndex).enabled == enabled) {
        return false;
    }

    cwPlacementRuleData rule = layer.rules.at(ruleIndex);
    rule.enabled = enabled;
    layer.rules.replace(ruleIndex, rule);
    m_brush.decorations.replace(layerIndex, layer);

    int categoryIndex = -1;
    int localRow = -1;
    if (ruleLocation(layerIndex, ruleIndex, &categoryIndex, &localRow)) {
        const QModelIndex layerRow = index(layerIndex, 0, QModelIndex());
        const QModelIndex ruleRow = index(localRow, 0, index(categoryIndex, 0, layerRow));
        emit dataChanged(ruleRow, ruleRow, {RuleEnabledRole, Qt::DisplayRole, LabelRole});
    }
    return true;
}

bool cwBrushStructureModel::insertRule(int layerIndex, int ruleIndex, const cwPlacementRuleData &rule)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex > layer.rules.size()) {   // == size appends
        return false;
    }

    const int newStage = stageOf(rule.name);
    const QVector<CategoryBlock> blocks = categoryBlocks(layerIndex);
    const QModelIndex layerRow = index(layerIndex, 0, QModelIndex());

    int existingCategory = -1;
    for (int c = 0; c < blocks.size(); ++c) {
        if (blocks.at(c).stage == newStage) {
            existingCategory = c;
            break;
        }
    }

    if (existingCategory >= 0) {
        // The rule joins an existing stage block: one inserted rule row.
        const CategoryBlock &block = blocks.at(existingCategory);
        const int local = std::clamp(ruleIndex - block.firstRule, 0, block.count);
        beginInsertRows(index(existingCategory, 0, layerRow), local, local);
        layer.rules.insert(ruleIndex, rule);
        m_brush.decorations.replace(layerIndex, layer);
        endInsertRows();
    } else {
        // A brand-new stage block: a category row appears (its single rule rides
        // along under it), inserted in stage order among the existing blocks.
        int newCategory = 0;
        while (newCategory < blocks.size() && blocks.at(newCategory).stage < newStage) {
            newCategory += 1;
        }
        beginInsertRows(layerRow, newCategory, newCategory);
        layer.rules.insert(ruleIndex, rule);
        m_brush.decorations.replace(layerIndex, layer);
        endInsertRows();
    }
    emitLayerRuleRolesChanged(layerIndex);
    return true;
}

bool cwBrushStructureModel::removeRule(int layerIndex, int ruleIndex)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }

    int categoryIndex = -1;
    int localRow = -1;
    if (!ruleLocation(layerIndex, ruleIndex, &categoryIndex, &localRow)) {
        return false;
    }
    const QVector<CategoryBlock> blocks = categoryBlocks(layerIndex);
    const QModelIndex layerRow = index(layerIndex, 0, QModelIndex());

    if (blocks.at(categoryIndex).count == 1) {
        // The last rule in its stage block: the category row goes away with it.
        beginRemoveRows(layerRow, categoryIndex, categoryIndex);
        layer.rules.removeAt(ruleIndex);
        m_brush.decorations.replace(layerIndex, layer);
        endRemoveRows();
    } else {
        beginRemoveRows(index(categoryIndex, 0, layerRow), localRow, localRow);
        layer.rules.removeAt(ruleIndex);
        m_brush.decorations.replace(layerIndex, layer);
        endRemoveRows();
    }
    emitLayerRuleRolesChanged(layerIndex);
    return true;
}

bool cwBrushStructureModel::moveRule(int layerIndex, int fromRuleIndex, int toRuleIndex)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    const int count = static_cast<int>(layer.rules.size());
    if (fromRuleIndex < 0 || fromRuleIndex >= count
        || toRuleIndex < 0 || toRuleIndex >= count
        || fromRuleIndex == toRuleIndex) {
        return false;
    }

    int fromCategory = -1;
    int fromLocal = -1;
    int toCategory = -1;
    int toLocal = -1;
    if (!ruleLocation(layerIndex, fromRuleIndex, &fromCategory, &fromLocal)
        || !ruleLocation(layerIndex, toRuleIndex, &toCategory, &toLocal)
        || fromCategory != toCategory) {
        return false;   // a move only means anything within a single category node
    }

    // Both rules share one category node, so the move is a row reorder under that
    // parent. beginMoveRows' destination is Qt's "insert before" position in the
    // pre-move indexing: moving down lands after the target (toLocal + 1), moving
    // up lands at the target (toLocal).
    const QModelIndex categoryNode = index(fromCategory, 0, index(layerIndex, 0, QModelIndex()));
    const int destinationChild = toLocal > fromLocal ? toLocal + 1 : toLocal;

    beginMoveRows(categoryNode, fromLocal, fromLocal, categoryNode, destinationChild);
    layer.rules.move(fromRuleIndex, toRuleIndex);
    m_brush.decorations.replace(layerIndex, layer);
    endMoveRows();

    // The rows the moved rule shifted past keep stale flat RuleIndexRole values
    // until refreshed.
    emitLayerRuleRolesChanged(layerIndex);
    return true;
}

bool cwBrushStructureModel::layerInRange(int layerIndex) const
{
    return layerIndex >= 0 && layerIndex < m_brush.decorations.size();
}

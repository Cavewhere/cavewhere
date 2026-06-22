/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBRUSHSTRUCTUREMODEL_H
#define CWBRUSHSTRUCTUREMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDecorationLayerValidator.h"
#include "cwLineBrush.h"

// A 3-level tree projection (layers -> categories -> rules) of a brush's
// structure, driving the brush editor's TreeView. Category nodes are real tree
// items derived from each layer's contiguous pipeline-stage blocks (the rules
// are kept stage-normalized, so each stage forms one contiguous block), giving
// the view fixed-height rule delegates instead of rule rows that smuggle a
// variable-height header.
//
// It owns the editor's working cwLineBrush and is the structure-mutation
// mechanism: setBrush() resets it wholesale (load/discard), while
// setRuleEnabled()/insertRule()/removeRule()/moveRule() mutate in place and
// bracket the matching QAbstractItemModel begin/end calls internally (inserting
// or removing the category node when a stage block appears or empties), so the
// tree stays in sync without a collapsing reset. The editor wraps these as its
// public API and owns the dirty/preview side effects (it is not the model's job
// to know about them), so the mutators here are deliberately not Q_INVOKABLE.
//
// The editor and the mutators address rules by a flat (layerIndex, ruleIndex)
// pair — the index into the layer's stored rule list. The category nesting is a
// view concern only; data() maps a rule node's (category, localRow) back to that
// flat index, and RuleIndexRole always reports it.
class CAVEWHERE_LIB_EXPORT cwBrushStructureModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ANONYMOUS

    // Root row count == layer count. A read-only computed property (the standard
    // getter + NOTIFY pattern, not a QBindable) so QML grip bindings react when a
    // layer is added/removed without the model reset that used to force a rebuild.
    Q_PROPERTY(int layerCount READ layerCount NOTIFY layerCountChanged)

public:
    enum Roles : int {
        LabelRole = Qt::UserRole + 1,   // layer glyph name, category label, or rule name
        RuleEnabledRole,                // rule's enabled flag (true for layer/category rows)
        IsLayerRole,                    // true for a layer row
        IsCategoryRole,                 // true for a category row
        LayerIndexRole,                 // owning layer index (all row kinds)
        RuleIndexRole,                  // flat rule index within the layer (-1 for layer/category rows)
        CategoryRole,                   // pipeline category label (category and rule rows); "" for layers
        CategorySizeRole,               // number of rules in this row's category (0 for layers)
        CategoryLastRole,               // true if this rule is the last in its category (drop-line gate); false otherwise
        RuleErrorSeverityRole,          // worst cwError::ErrorType for this row (NoError = clean); category rows are always NoError
        RuleErrorMessagesRole,          // QStringList of this row's validation messages (empty when clean)
        RuleErrorCodesRole,             // QList<int> of SymbologyError codes, one per message (parallel to RuleErrorMessagesRole)
    };
    Q_ENUM(Roles)

    explicit cwBrushStructureModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The working brush this model owns and projects. The editor reads it back for
    // snapshot()/dirty/apply().
    const cwLineBrush &brush() const { return m_brush; }
    void setBrush(const cwLineBrush &brush);   // wholesale change (load/discard) -> model reset

    // The glyph names the resolved palette offers, so the validator can flag a
    // layer that references a glyph the palette doesn't have. Setting it
    // re-validates and refreshes the per-row error roles.
    void setAvailableGlyphNames(const QSet<QString> &names);

    // Structure accessors the editor's invokables delegate to. All rule indices
    // here are flat indices into the layer's stored rule list.
    int layerCount() const;
    QString layerLabel(int layerIndex) const;
    int ruleCount(int layerIndex) const;
    QString ruleName(int layerIndex, int ruleIndex) const;
    bool ruleEnabled(int layerIndex, int ruleIndex) const;
    // The rule's pipeline category, derived from its registered stage (the same
    // labels the add-rule picker groups by); empty for an unknown rule.
    Q_INVOKABLE QString ruleCategory(int layerIndex, int ruleIndex) const;
    // Map a view QModelIndex (e.g. from TableView::modelIndex/cellAtPosition)
    // back to the layer/rule coordinates the editor's mutators use. layerIndexOf
    // returns -1 for an invalid index; ruleIndexOf returns -1 for a non-rule row
    // (a flat rule index otherwise).
    Q_INVOKABLE int layerIndexOf(const QModelIndex &index) const;
    Q_INVOKABLE int ruleIndexOf(const QModelIndex &index) const;

    // Structure mutators. Each brackets the matching QAIM begin/end internally;
    // returns false (no change, no signal) on an out-of-range request.
    bool setRuleEnabled(int layerIndex, int ruleIndex, bool enabled);            // narrow dataChanged
    bool insertRule(int layerIndex, int ruleIndex, const cwPlacementRuleData &rule);
    bool removeRule(int layerIndex, int ruleIndex);
    // Move a rule within its category so it ends up at toRuleIndex; brackets
    // beginMoveRows/endMoveRows. Returns false on an out-of-range, no-op, or
    // cross-category request (the layout re-sorts across stages, so a move only
    // means anything within a single category node).
    bool moveRule(int layerIndex, int fromRuleIndex, int toRuleIndex);

    // Layer (top-level row) mutators. Like the rule mutators these bracket the
    // matching QAIM begin/end (beginInsertRows / beginRemoveRows / beginMoveRows),
    // not a reset: a category/rule QModelIndex names its owning layer by a STABLE
    // id (m_layerIds), not a position, so shifting later layers' rows leaves every
    // descendant's internalId valid (it resolves via layerRowForId). Qt leaves
    // grandchildren untouched on a root-level edit, which is correct here, so
    // QPersistentModelIndex survives and scroll/expansion/selection are preserved.
    // layerIndex == layerCount() appends. Returns false (no change, no signal) on
    // an out-of-range or no-op request.
    bool insertLayer(int layerIndex, const cwDecorationLayer &layer);
    bool removeLayer(int layerIndex);
    // Reorder a layer so it ends up at toLayerIndex. Paint order == layer order
    // (Decision 12), so a layer move is meaningful across the whole brush (unlike
    // a rule move, which only matters within its stage).
    bool moveLayer(int fromLayerIndex, int toLayerIndex);

signals:
    void layerCountChanged();

private:
    // One contiguous pipeline-stage block within a layer's rule list: the rules
    // sharing a stage, which the view renders as one category node.
    struct CategoryBlock {
        int stage = 0;       // cwPlacementRule::Stage value (or a past-the-end sentinel for unknowns)
        int firstRule = 0;   // flat index of the block's first rule
        int count = 0;       // number of rules in the block
    };
    // The layer's stage blocks in display (stage) order. Cheap to recompute (a
    // single pass over a handful of rules), so it isn't cached — a cache would be
    // mutable state that must invalidate on every edit.
    QVector<CategoryBlock> categoryBlocks(int layerIndex) const;
    // Locate a flat rule index within its category block. Returns false if the
    // index isn't a real rule; otherwise fills the category index and the rule's
    // row within that category.
    bool ruleLocation(int layerIndex, int ruleIndex, int *categoryIndex, int *localRow) const;

    bool layerInRange(int layerIndex) const;
    // Map a layer's stable id (carried in a category/rule internalId) back to its
    // current row. n is a handful, so a linear scan beats a hash member to sync.
    int layerRowForId(quintptr id) const;
    // After an insert/remove, the flat positional roles (RuleIndexRole) of rules
    // in LATER category blocks shift, but the view only re-reads the rows it was
    // told changed; refresh every rule row in the layer so those stay correct.
    void emitLayerRuleRolesChanged(int layerIndex);

    // Recompute m_layerErrors for the whole brush. Cheap (a handful of layers,
    // each a handful of rules); kept as an explicit member, not a static, so its
    // lifetime is the model's.
    void revalidate();
    // After revalidate(), refresh the error roles on a layer's rows (the layer row
    // and every rule row under it), since a single edit can change errors anywhere
    // in the layer's stack.
    void emitLayerErrorRolesChanged(int layerIndex);
    // The validation errors attached to one row: rule row by its flat index, or
    // the layer row when flatRuleIndex == -1.
    QList<cwError> rowErrors(int layerIndex, int flatRuleIndex) const;

    cwLineBrush m_brush;
    // A stable id per decoration layer, aligned 1:1 with m_brush.decorations.
    // Category/rule internalIds carry the id instead of a position, so structural
    // layer edits don't strand descendants. Maintained by every layer mutator and
    // setBrush().
    QList<quintptr> m_layerIds;
    quintptr m_nextLayerId = 0;   // monotonic; 31-bit id field = ~2B edits, ample
    QSet<QString> m_availableGlyphNames;
    QVector<QList<cwDecorationLayerError>> m_layerErrors;   // one entry per decoration layer
};

#endif // CWBRUSHSTRUCTUREMODEL_H

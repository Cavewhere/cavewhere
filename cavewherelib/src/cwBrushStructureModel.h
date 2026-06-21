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
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class cwBrushEditor;

// A 2-level tree projection (layers -> rules) of a cwBrushEditor's working brush,
// driving the brush editor's TreeView. It owns no data: every data() call reads
// the editor's structure accessors, and the editor drives change notifications
// directly -- reset() on a wholesale load/discard, ruleChanged() on a single
// toggle -- so a checkbox toggle emits a narrow dataChanged instead of resetting
// (and collapsing) the tree. Replaces the phase-1 _structureRevision /
// comma-operator stopgap; the incremental row signals add/remove/reorder needs
// land in phase 2.2.
class CAVEWHERE_LIB_EXPORT cwBrushStructureModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum Roles : int {
        LabelRole = Qt::UserRole + 1,   // layer glyph name, or rule name
        RuleEnabledRole,                // rule's enabled flag (true for layer rows)
        IsLayerRole,                    // true for a layer row, false for a rule row
        LayerIndexRole,                 // owning layer index (both row kinds)
        RuleIndexRole,                  // rule index within the layer (-1 for layer rows)
    };
    Q_ENUM(Roles)

    explicit cwBrushStructureModel(cwBrushEditor *editor);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Editor-driven change notifications (the editor owns this model).
    void reset();                                    // wholesale change (load/discard)
    void ruleChanged(int layerIndex, int ruleIndex); // one rule's data changed

private:
    // Layer rows carry this sentinel as their QModelIndex internalId; rule rows
    // carry their owning layer's row instead (always small, never colliding).
    static constexpr quintptr kLayerId = ~quintptr(0);

    QPointer<cwBrushEditor> m_editor;
};

#endif // CWBRUSHSTRUCTUREMODEL_H

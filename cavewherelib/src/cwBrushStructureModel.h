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

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLineBrush.h"

// A 2-level tree projection (layers -> rules) of a brush's structure, driving the
// brush editor's TreeView. It owns the editor's working cwLineBrush and is the
// structure-mutation mechanism: setBrush() resets it wholesale (load/discard),
// while setRuleEnabled()/insertRule()/removeRule() mutate in place and bracket the
// matching QAbstractItemModel begin/end calls internally, so the tree stays in
// sync without a collapsing reset. The editor wraps these as its public API and
// owns the dirty/preview side effects (it is not the model's job to know about
// them), so the mutators here are deliberately not Q_INVOKABLE.
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

    // Structure accessors the editor's invokables delegate to.
    int layerCount() const;
    QString layerLabel(int layerIndex) const;
    int ruleCount(int layerIndex) const;
    QString ruleName(int layerIndex, int ruleIndex) const;
    bool ruleEnabled(int layerIndex, int ruleIndex) const;

    // Structure mutators. Each brackets the matching QAIM begin/end internally;
    // returns false (no change, no signal) on an out-of-range request.
    bool setRuleEnabled(int layerIndex, int ruleIndex, bool enabled);            // narrow dataChanged
    bool insertRule(int layerIndex, int ruleIndex, const cwPlacementRuleData &rule);
    bool removeRule(int layerIndex, int ruleIndex);

private:
    // Layer rows carry this sentinel as their QModelIndex internalId; rule rows
    // carry their owning layer's row instead (always small, never colliding).
    static constexpr quintptr kLayerId = ~quintptr(0);

    bool layerInRange(int layerIndex) const;

    cwLineBrush m_brush;
};

#endif // CWBRUSHSTRUCTUREMODEL_H

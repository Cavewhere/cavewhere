/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULEREGISTRY_H
#define CWPLACEMENTRULEREGISTRY_H

//Qt includes
#include <QHash>
#include <QString>
#include <QVector>

//Std includes
#include <memory>
#include <vector>

//Our includes
#include "CaveWhereLibExport.h"

class cwPlacementRule;

// The single registry of executable placement rules. Decoration layers reference
// rules by name (cwPlacementRuleData::name); this maps each name to the one
// const rule instance cwSketchDecorationLayout runs. Modeled on
// cwSyncMergeRegistry: a process-wide singleton built once, rules owned for the
// program lifetime. Iter 1 registers the six rules a layer's stack composes
// from (Decision 11); a layer with no rules draws nothing — there is no mode and
// no per-mode default stack.
class CAVEWHERE_LIB_EXPORT cwPlacementRuleRegistry {
public:
    static const cwPlacementRuleRegistry &instance();

    // Looked-up by displayName (the stable palette-schema key). nullptr for an
    // unknown rule, so callers can skip-and-warn rather than crash.
    const cwPlacementRule *rule(const QString &name) const;

    // Every registered rule, in registration order. Drives the brush editor's
    // add-rule picker; the names are the stable palette-schema keys.
    QVector<const cwPlacementRule *> allRules() const;

private:
    cwPlacementRuleRegistry();

    void add(std::unique_ptr<cwPlacementRule> rule);

    std::vector<std::unique_ptr<cwPlacementRule>> m_rules;
    QHash<QString, const cwPlacementRule *> m_byName;
};

#endif // CWPLACEMENTRULEREGISTRY_H

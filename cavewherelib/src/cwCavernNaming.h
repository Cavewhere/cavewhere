#ifndef CWCAVERNNAMING_H
#define CWCAVERNNAMING_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QString>
#include <QHash>
#include <QList>
#include <QUuid>

/**
 * One home for the survey-scope labels CaveWhere writes into Survex.
 *
 * A cave becomes "*begin <caveLabel>" and an externally-attached trip becomes a
 * nested "*begin <tripLabel>", so cavern echoes a fully-qualified station as
 * <caveLabel>.<tripLabel>.<tail> (external) or <caveLabel>.<tail> (native).
 * The labels are the cave's and trip's own names, folded to a legal cavern
 * identifier - a survey named "Fisher Ridge" writes fisher_ridge, so the driver
 * text on the CavernOutputPage and any .svx a user exports read the way
 * hand-written Survex does.
 *
 * Labels are assigned per sibling set: cave labels are unique within a region,
 * trip labels within their cave. Two names that sanitize to the same identifier
 * are disambiguated by iteration order, so encode and decode reach the same
 * assignment from the same ordered snapshot without carrying a map between
 * them - which is what lets the worker thread decode what the exporter wrote.
 *
 * Every producer (the survex exporter, the line-plot driver) and every consumer
 * (splitLookupByCave, the cave-network mirror, the cwTrip solved-station
 * accessors, the geometry/label enumeration) shares these functions so the
 * encode and decode sides can never drift.
 */
namespace cwCavernNaming {

/**
 * One nameable scope: a cave among its region's caves, or a trip among the
 * trips of its cave.
 */
struct ScopeEntry {
    QUuid id;
    QString name;
};

//! Folds a user-facing name into a legal, lowercase cavern survey identifier.
CAVEWHERE_LIB_EXPORT QString sanitizeToCavernIdentifier(const QString& name);

//! A unique label per sibling, assigned in iteration order.
CAVEWHERE_LIB_EXPORT QHash<QUuid, QString> scopeLabels(const QList<ScopeEntry>& siblings);

//! "<label>." — the prefix a name nested in \a label's scope carries, or empty
//! when \a label is. The one place the separator is spelled, so a caller that
//! holds a label never has to.
CAVEWHERE_LIB_EXPORT QString scopePrefix(const QString& label);

//! The leading "<label>" of a scoped name, or empty when the name carries no
//! scope at all. Only the first segment is returned: nested scopes and dotted
//! tails stay in the remainder.
CAVEWHERE_LIB_EXPORT QString scopeHeadOf(const QString& scopedName);

//! \a scopedName with its leading scope removed, or \a scopedName unchanged
//! when it carries none.
CAVEWHERE_LIB_EXPORT QString removeScopeHead(const QString& scopedName);

}

#endif // CWCAVERNNAMING_H

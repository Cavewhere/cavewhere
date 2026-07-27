#ifndef CWSCOPELABELS_H
#define CWSCOPELABELS_H

//Our includes
#include "cwGlobals.h"
struct cwCaveData;
struct cwCavingRegionData;

//Qt includes
#include <QHash>
#include <QString>
#include <QUuid>

/**
 * The survey labels one region snapshot's caves and trips take, assigned once.
 *
 * cwCavernNaming assigns a label per sibling set — a cave among its region's
 * caves, a trip among its cave's trips — which makes every label a pure
 * function of an ordered snapshot. That is what lets the exporter, the
 * line-plot worker and the geometry pass agree on a name without carrying a map
 * across the thread boundary between them. It also means each of them could
 * derive the labels itself, and each of them did. This is that derivation done
 * once and passed along instead: the same answer, from one place, with no way
 * for two readers to drift apart.
 *
 * Holds no live model object and copies cheaply (implicitly-shared hashes), so
 * it travels with the cwCavingRegionData it was built from.
 */
class CAVEWHERE_LIB_EXPORT cwScopeLabels
{
public:
    cwScopeLabels() = default;
    explicit cwScopeLabels(const cwCavingRegionData& region);

    //! Labels for one cave standing alone, as the single-cave exporter sees it.
    //! Its trips get labels — a trip label is unique within its cave, so no
    //! region is needed to make it right — but the cave itself gets none, since
    //! that is the one question only a region can answer.
    static cwScopeLabels forCave(const cwCaveData& cave);

    //! The label \a caveId's "*begin" block carries, or empty when this pool
    //! assigned none.
    QString caveLabel(const QUuid& caveId) const;

    //! "<caveLabel>." — the prefix every station inside that cave carries, or
    //! empty when this pool assigned the cave no label.
    QString cavePrefix(const QUuid& caveId) const;

    //! The cave wearing \a caveLabel, or a null QUuid when none does. The
    //! inverse of caveLabel(), and how a name cavern echoes back is routed to
    //! the cave that produced it.
    QUuid caveId(const QString& caveLabel) const;

    //! The labels that cave's trips take, keyed by trip id. Empty for a cave
    //! this pool never saw.
    const QHash<QUuid, QString>& tripLabels(const QUuid& caveId) const;

private:
    QHash<QUuid, QString> m_caveLabels;
    QHash<QString, QUuid> m_caveIdsByLabel;
    QHash<QUuid, QHash<QUuid, QString>> m_tripLabelsByCave;
};

#endif // CWSCOPELABELS_H

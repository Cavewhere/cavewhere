/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRECENTERCANDIDATEMODEL_H
#define CWRECENTERCANDIDATEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QString>
#include <QUuid>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

class cwCavingRegion;
class cwLocalProjectionManager;

//! One station the project's projection could be recentered on: which station it
//! is, how to name it to the user, and whether centering there would keep the
//! project's data inside the frame's reach.
struct cwRecenterCandidate {
    QUuid stationId;
    QString stationName;
    QString caveName;
    bool eligible = false;
};

/**
 * The fix stations the user may recenter the local projection on, as rows.
 *
 * Owns the question "which stations could the project be centered on, and which
 * of those would be a sane place to center it" — the picker reads it as a model,
 * and cwLocalProjectionManager::recenterOnStation() enforces the same rule
 * through cwLocalProjectionManager::isWithinReach() before it moves anything.
 *
 * Created by the manager on first access and refreshed whenever the picker
 * opens. It is deliberately not kept current between opens: eligibility
 * reprojects every station in the project, and nothing is showing the rows.
 */
class CAVEWHERE_LIB_EXPORT cwRecenterCandidateModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RecenterCandidateModel)
    QML_UNCREATABLE("Owned by LocalProjectionManager; access via region.localProjection.recenterCandidates")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles {
        StationIdRole = Qt::UserRole + 1,
        StationNameRole,
        CaveNameRole,
        //! Whether recentering on this station is allowed. An ineligible row
        //! stays in the list rather than being dropped: a station the project's
        //! own data sits 200 km from is exactly the one a user goes looking for,
        //! and a row that is present but disabled answers them.
        EligibleRole
    };
    Q_ENUM(Roles)

    cwRecenterCandidateModel(cwLocalProjectionManager* manager, cwCavingRegion* region);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_candidates.size()); }

    //! Rebuild the rows against the project as it stands. Called as the picker
    //! opens, and cheap enough to be: it reprojects the project's inputs.
    Q_INVOKABLE void refresh();

signals:
    void countChanged();

private:
    cwLocalProjectionManager* m_manager = nullptr;
    cwCavingRegion* m_region = nullptr;

    QList<cwRecenterCandidate> m_candidates;

    void setCandidates(const QList<cwRecenterCandidate>& candidates);
};

#endif // CWRECENTERCANDIDATEMODEL_H

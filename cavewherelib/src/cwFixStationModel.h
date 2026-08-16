/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONMODEL_H
#define CWFIXSTATIONMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QVector3D>

//Our includes
#include "cwFixStation.h"
#include "cwGlobals.h"
#include "cwUnits.h"

/**
 * QAbstractListModel of cwFixStation rows owned by a cwCave.
 *
 * QML edits cells in place via setData() using the role names exposed in
 * roleNames(). Add and remove rows with addFixStation() / removeAt(index).
 *
 * Every role here is persisted data the user typed, so dataChanged() means
 * exactly "save, re-solve and re-validate this". The read-only warnings derived
 * from the solve live in cwFixStationDiagnosticsModel, a proxy over this model —
 * keeping them out is what lets cwLinePlotManager, cwFixStationValidator,
 * cwTripCalibration and cwSaveLoad connect here without a role filter.
 */
class CAVEWHERE_LIB_EXPORT cwFixStationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1,
        InputCSRole,
        EastingRole,
        NorthingRole,
        ElevationRole,
        HorizontalVarianceRole,
        VerticalVarianceRole,
        IdRole,
        //! Read-only through setData(): the coordinate as it was written, which
        //! is the whole of what the row stores (the three component roles above
        //! are read out of it). An editor opens on it; the *cell* renders the
        //! components in the project's units, so display and edit deliberately
        //! differ. Written through setCoordinateText(), the one path that
        //! validates a string before it becomes the coordinate.
        CoordinateTextRole,
        //! cwFixStation::ElevationReference — what the row's elevation is
        //! measured from. Metadata: nothing derives from it, and writing it
        //! moves no other role.
        ElevationReferenceRole
    };
    Q_ENUM(Roles)

    explicit cwFixStationModel(QObject* parent = nullptr);
    ~cwFixStationModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
    Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addFixStation();
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE cwFixStation fixStationAt(int index) const;

    //! Row of the fix anchoring \a stationName, or -1 if no row does. Matching
    //! follows the same rule as every other place a fix is resolved to a survey
    //! station (cwFixStationDiagnostics::classifyStationReference via
    //! cwSurveyNetwork::hasStation): trim, then case-insensitive. An empty name
    //! matches nothing, so a blank scaffold row is never "the fix for no
    //! station".
    Q_INVOKABLE int indexOf(const QString& stationName) const;

    //! Row of the fix carrying \a fixId, or -1. The identity that survives an
    //! edit somewhere else — a rename, a row inserted above, a re-sort — which
    //! is what anything holding onto a row across such an edit has to address it
    //! by (setPickedPoint(), cwFixStationValidator).
    int indexOf(const QUuid& fixId) const;

    Q_INVOKABLE bool isFixed(const QString& stationName) const;

    //! Row of the fix for \a stationName, appending one if the station isn't
    //! fixed yet. Idempotent by name, so marking the same station Fixed twice
    //! can't leave a cave with two anchors fighting over it. -1 when the name is
    //! empty — there is nothing to anchor.
    Q_INVOKABLE int addFixStation(const QString& stationName);
    Q_INVOKABLE void removeFixStation(const QString& stationName);

    //! Write the whole coordinate of \a row from one free-form string (#621),
    //! e.g. "46.12113, -115.59902, 304ft". Returns an empty string on success,
    //! or the reason the text couldn't be read — setData() can only report
    //! failure as a bool, and a free-form field has to say what was wrong with
    //! what the user typed.
    //!
    //! Everything the string moves lands in a single dataChanged(), so the line
    //! plot re-solves once for a pasted coordinate rather than three times.
    //! \a units is the project's unit system, and resolves an elevation written
    //! with no unit suffix — the only thing it decides. Which axis the text
    //! leads with is not a parameter: it comes from the row's own
    //! coordinate system, so the reading can't disagree with the row.
    //!
    //! \a text becomes the row's coordinate, normalized only by
    //! cwCoordinateText::textToStore(). Committing an unchanged field is
    //! therefore a no-op, which it has to be: the field is opened and left far
    //! more often than it is edited.
    Q_INVOKABLE QString setCoordinateText(int row,
                                          const QString& text,
                                          cwUnits::UnitSystem units);

    //! Write \a scenePoint — a point in the project's local frame, straight off
    //! the 3D view's ray-cast — as the coordinate of the fix carrying \a fixId.
    //! Returns whether the fix was written.
    //!
    //! \a frameCS is the system \a scenePoint is expressed in
    //! (cwGeoReference::localCoordinateSystem) and \a datum the geographic
    //! system the coordinate is written on, which becomes the row's inputCS —
    //! a pick over a lidar tile lands on the same datum as the tile rather than
    //! 1.5 m from it. \a pickedSourceCS is the system of what the pick landed
    //! on: when it says what its heights are measured from, so can the fix
    //! (cwFixStation::MeanSeaLevel), and otherwise the row stays Unknown. The
    //! caller decides which three those are — the model reads no region.
    //!
    //! False leaves the row untouched: there is no such fix, or the point
    //! can't be put on \a datum, which the 3D view answers by keeping the pick
    //! pending so the user can orbit and click again.
    //!
    //! \a fixId is spelled as a string because that is what QML can hold onto
    //! across the trip to the view and back; QUuid is not a QML value type.
    //!
    //! Everything this moves lands in a single dataChanged(): three setData()
    //! calls would re-solve the line plot three times, and the first two would
    //! leave the row momentarily saying something untrue — a coordinate read
    //! under the system it is about to replace.
    Q_INVOKABLE bool setPickedPoint(const QString& fixId,
                                    const QVector3D& scenePoint,
                                    const QString& frameCS,
                                    const QString& datum,
                                    const QString& pickedSourceCS);

    void appendFixStation(const cwFixStation& fix);
    void setFixStations(const QList<cwFixStation>& fixes);
    const QList<cwFixStation>& fixStations() const { return m_fixStations; }
    int count() const { return m_fixStations.size(); }

signals:
    void countChanged();

    //! The set of station names these rows anchor changed — a row was added,
    //! removed, replaced or renamed. Separate from countChanged(), which a
    //! rename doesn't move, so a view showing "is this station fixed?" has one
    //! signal that covers every way the answer can change. Derived in the
    //! constructor from the model's own row/reset/dataChanged signals, so no
    //! mutator has to remember to emit it.
    void fixedStationsChanged();

private:
    QList<cwFixStation> m_fixStations;
};

#endif // CWFIXSTATIONMODEL_H

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

//Our includes
#include "cwFixStation.h"
#include "cwCoordinateText.h"
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
        //! Read-only through setData(): the string the user typed this
        //! coordinate as, or empty when nobody typed it here. An editor opens
        //! on it and falls back to cwCoordinateText::format() when it's empty;
        //! the *cell* renders from the numbers either way, so display and edit
        //! deliberately differ (U14). It is written only by setCoordinateText(),
        //! which is the one path that can read a string back into numbers.
        CoordinateTextRole
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
    //! All three components land in a single dataChanged(), so the line plot
    //! re-solves once for a pasted coordinate rather than three times. \a units
    //! is the project's unit system, which resolves an elevation with no unit
    //! suffix, and \a order says whether the text leads with the easting or the
    //! latitude — it must be the same order the field was rendered with, or the
    //! coordinate comes back transposed. See cwCoordinateText for both.
    //!
    //! \a text is kept on the row (CoordinateTextRole) so an editor can re-offer
    //! it, unless it reads back as exactly what the row already renders — that
    //! string is the machine's, not the user's, and a row with none of its own
    //! renders the same thing anyway. Committing an unchanged field is therefore
    //! a no-op whether or not the row has stored text, which it has to be: the
    //! field is opened and left far more often than it is edited.
    Q_INVOKABLE QString setCoordinateText(int row,
                                          const QString& text,
                                          cwUnits::UnitSystem units,
                                          cwCoordinateText::AxisOrder order);

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

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTATIONVALIDATOR_H
#define CWSTATIONVALIDATOR_H

//Our includes
#include "cwValidator.h"

//Qt includes
#include <QQmlEngine>
#include <QRegularExpression>

class cwStationValidator : public cwValidator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(StationValidator)

    // True when the validator is bound to an input that references a
    // station inside an externally-backed cave or trip (Attached or, in
    // Phase 3, Scope). Survex, Compass and Walls files all permit
    // characters native CaveWhere station names cannot - dotted
    // namespacing, hyphens inside names, and (Walls) embedded spaces -
    // and the validator must accept whatever the upstream file uses or
    // a user would not be able to type the name of a station that
    // already exists in their data. Defaults to false so untouched
    // call sites keep the strict Native regex.
    Q_PROPERTY(bool external READ external WRITE setExternal NOTIFY externalChanged FINAL)

public:
    explicit cwStationValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;

    bool external() const { return m_external; }
    void setExternal(bool external);

    // Strict native station-name regex: letters, digits, hyphen, and
    // underscore. Used everywhere the input belongs to a CaveWhere-native
    // cave or trip - we do not want users typing dotted or whitespace
    // names into native data and silently breaking export round-trips.
    static QRegularExpression validCharactersRegex();
    static QRegularExpression invalidCharactersRegex();

    // Relaxed station-name regex used inside externally-backed entities.
    // Accepts everything in validCharactersRegex() plus dots (Survex /
    // Compass nested namespaces, "block.station") and a literal space
    // (Walls feet-and-inches quirk). Master plan section 7.2.
    static QRegularExpression externalStationCharactersRegex();
    static QRegularExpression externalInvalidCharactersRegex();

signals:
    void externalChanged();

public slots:

private:
    bool m_external = false;

};

#endif // CWSTATIONVALIDATOR_H

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

class cwStationValidator : public cwValidator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(StationValidator)
public:
    explicit cwStationValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;

    static QRegularExpression validCharactersRegex();
    static QRegularExpression invalidCharactersRegex();

signals:

public slots:

};

#endif // CWSTATIONVALIDATOR_H

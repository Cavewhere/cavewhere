/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTATIONVALIDATOR_H
#define CWSTATIONVALIDATOR_H

#include "cwValidator.h"

class cwStationValidator : public cwValidator
{
    Q_OBJECT
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

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPASSVALIDATOR_H
#define CWCOMPASSVALIDATOR_H

#include "cwValidator.h"

class cwCompassValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwCompassValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;
    static bool check(double);

signals:

public slots:

};

/**
  Checks to make sure the value is correct, greater than 0.0
  */
inline bool cwCompassValidator::check(double value) {
    return value >= 0.0 && value < 360.0;
}


#endif // CWCOMPASSVALIDATOR_H

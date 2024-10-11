/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCLINOVALIDATOR_H
#define CWCLINOVALIDATOR_H

#include "cwValidator.h"

class cwClinoValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwClinoValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;
    static bool check(double value);
signals:

public slots:

};

/**
  Checks to make sure value is correct, between -90 and 90
  */
inline bool cwClinoValidator::check(double value) {
    return value >= -90.0 && value <= 90.0;
}

#endif // CWCLINOVALIDATOR_H

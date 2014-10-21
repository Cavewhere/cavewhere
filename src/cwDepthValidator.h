/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDEPTHVALIDATOR_H
#define CWDEPTHVALIDATOR_H

#include "cwValidator.h"

class cwDepthValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwDepthValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;
    static bool check(double value);
signals:

public slots:

};

/**
  Checks to make sure value is correct, between -90 and 90
  */
inline bool cwDepthValidator::check(double value) {
    return value >= -90.0 && value <= 90.0;
}

#endif // CWDEPTHVALIDATOR_H

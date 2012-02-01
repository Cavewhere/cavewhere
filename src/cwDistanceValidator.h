#ifndef CWDISTANCEVALIDATOR_H
#define CWDISTANCEVALIDATOR_H

#include "cwValidator.h"

class cwDistanceValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwDistanceValidator(QObject *parent = 0);
    virtual State validate( QString & input, int & pos ) const;
    Q_INVOKABLE virtual int validate( QString input ) const;

signals:

public slots:

};

#endif // CWDISTANCEVALIDATOR_H

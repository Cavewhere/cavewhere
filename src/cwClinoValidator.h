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

signals:

public slots:

};

#endif // CWCLINOVALIDATOR_H

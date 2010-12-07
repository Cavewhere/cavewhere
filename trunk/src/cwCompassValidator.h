#ifndef CWCOMPASSVALIDATOR_H
#define CWCOMPASSVALIDATOR_H

#include <cwValidator.h>

class cwCompassValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwCompassValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;

signals:

public slots:

};

#endif // CWCOMPASSVALIDATOR_H

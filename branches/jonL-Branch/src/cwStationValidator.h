#ifndef CWSTATIONVALIDATOR_H
#define CWSTATIONVALIDATOR_H

#include <cwValidator.h>

class cwStationValidator : public cwValidator
{
    Q_OBJECT
public:
    explicit cwStationValidator(QObject *parent = 0);

    State validate( QString & input, int & pos ) const;
    Q_INVOKABLE int validate( QString input ) const;

signals:

public slots:

};

#endif // CWSTATIONVALIDATOR_H

#ifndef CWVALIDATOR_H
#define CWVALIDATOR_H

#include <QValidator>

class cwValidator : public QValidator
{
    Q_OBJECT
public:
    explicit cwValidator(QObject *parent = 0);

    Q_INVOKABLE virtual int validate( QString input ) const = 0;

signals:

public slots:

};

#endif // CWVALIDATOR_H

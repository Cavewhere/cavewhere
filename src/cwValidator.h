/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWVALIDATOR_H
#define CWVALIDATOR_H

#include <QValidator>
#include <QDebug>

class cwValidator : public QValidator
{
    Q_OBJECT

    Q_PROPERTY(QString errorText READ errorText WRITE setErrorText NOTIFY errorTextChanged)

public:
    explicit cwValidator(QObject *parent = 0);

    Q_INVOKABLE virtual int validate( QString input ) const;
    virtual QValidator::State validate(QString& input, int& pos) const;

    QString errorText() const;
    void setErrorText(QString errorText);
signals:
    void errorTextChanged();

public slots:

private:
    QString ErrorText; //!< The text that the user see if they fail to set the value correctly
};

Q_DECLARE_METATYPE(cwValidator*)

/**
Gets errorText
*/
inline QString cwValidator::errorText() const {
    return ErrorText;
}
#endif // CWVALIDATOR_H

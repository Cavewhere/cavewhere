#include "cwValidator.h"

cwValidator::cwValidator(QObject *parent) :
    QValidator(parent)
{
}

int cwValidator::validate(QString input) const
{
    int pos = 0;
    return (int)validate(input, pos);
}

QValidator::State cwValidator::validate(QString & input, int & pos) const
{
    Q_UNUSED(input);
    Q_UNUSED(pos);
    return QValidator::Invalid;
}

/**
Sets errorText
*/
void cwValidator::setErrorText(QString errorText) {
    if(ErrorText != errorText) {
        ErrorText = errorText;
        emit errorTextChanged();
    }
}

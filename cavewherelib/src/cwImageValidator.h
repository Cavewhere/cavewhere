/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGEVALIDATOR_H
#define CWIMAGEVALIDATOR_H

#include <QObject>
#include <QStringList>
#include <QQmlEngine>

/**
 * @brief The cwImageValidator class
 */
class cwImageValidator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ImageValidator)

    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit cwImageValidator(QObject *parent = 0);
    
    Q_INVOKABLE QStringList validateImages(QStringList imagePaths);

    QString errorMessage() const;
    Q_INVOKABLE void clearErrorMessage();

signals:
    void errorMessageChanged();

public slots:

private:
    QString ErrorMessage; //!<

    void setErrorMessage(QString errorMessage);
};

/**
Gets errorMessage
*/
inline QString cwImageValidator::errorMessage() const {
    return ErrorMessage;
}



#endif // CWIMAGEVALIDATOR_H

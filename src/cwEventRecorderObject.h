/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWEVENTRECORDEROBJECT_H
#define CWEVENTRECORDEROBJECT_H

//Qt includes
#include <QHash>
#include <QByteArray>
#include <QHash>
#include <QVariant>
#include <QSharedPointer>

class cwEventRecorderObject
{
public:
    cwEventRecorderObject();
    cwEventRecorderObject(const QObject* object);
    cwEventRecorderObject(QByteArray fullname);

    bool isValid() const;

    QByteArray fullName() const;

    static QObject* findObject(QObject* rootObject, const cwEventRecorderObject& object);

    bool operator==(const QObject* object) const;
    bool operator!=(const QObject* object) const;

private:

    QSharedPointer<cwEventRecorderObject> Parent;
    QByteArray Type;
    QByteArray Name;
    QHash<QByteArray, QVariant> Properties;

    void populateProperties(const QObject *object);
    bool operatorEqualHelper(const cwEventRecorderObject& other) const;


};

#endif // CWEVENTRECORDEROBJECT_H

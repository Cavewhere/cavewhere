/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwEventRecorderObject.h"

//Qt includes
#include <QMetaProperty>
#include <QMetaObject>
#include <QDebug>

cwEventRecorderObject::cwEventRecorderObject()
{
}

cwEventRecorderObject::cwEventRecorderObject(const QObject *object)
{
    if(object != nullptr) {
        populateProperties(object);
//        Parent = QSharedPointer<cwEventRecorderObject>(new cwEventRecorderObject(object->parent()));
    }
}

cwEventRecorderObject::cwEventRecorderObject(QByteArray fullname)
{
    Q_ASSERT(!fullname.isEmpty());
    QString name = QString::fromLocal8Bit(fullname);
    QStringList properties = name.split(";");
    Q_ASSERT(!properties.isEmpty());
    QStringList nameTypeList = properties.first().split(":");

    Q_ASSERT(nameTypeList.size() == 2);
    Name = nameTypeList.at(0).toLocal8Bit();
    Type = nameTypeList.at(1).toLocal8Bit();

    QRegExp keyValueRegex("(.+)=(.+)");
    foreach(const QString& property, properties) {
        if(keyValueRegex.exactMatch(property)) {
            QString key = keyValueRegex.cap(0);
            QString value = keyValueRegex.cap(1);
            Properties.insert(key.toLocal8Bit(), value);
        }
    }
}

/**
 * @brief cwEventRecorderObject::isValid
 * @return True if the object is valid or false if it's not
 */
bool cwEventRecorderObject::isValid() const
{
    return !Type.isEmpty();
}

QByteArray cwEventRecorderObject::fullName() const
{
    QString fullName;
    fullName += QString::fromLocal8Bit(Name);
    fullName += ":";
    fullName += QString::fromLocal8Bit(Type);

    foreach(const QByteArray& key, Properties.keys()) {
        QVariant value = Properties.value(key, QVariant());
        if(value.canConvert<QString>()) {
            fullName += ";";
            fullName += QString("%1=%2")
                    .arg(QString::fromLocal8Bit(key))
                    .arg(value.toString());
        }
    }

    return fullName.toLocal8Bit();
}

/**
 * @brief cwEventRecorderObject::findObject
 * @param rootObject
 * @param object
 * @return
 *
 * This will recursively search for the object by cwEventRecorderObject name
 */
QObject *cwEventRecorderObject::findObject(QObject *rootObject, const cwEventRecorderObject &object)
{
    if(rootObject == nullptr) {
        return nullptr;
    }

//    qDebug() << "\t" << object.fullName();
//    qDebug() << "\t" << cwEventRecorderObject(rootObject).fullName();
//    qDebug() << "\t\t" << (object == rootObject);
    if(object == rootObject) {
        return rootObject;
    }

    foreach(QObject* child, rootObject->children()) {
        QObject* foundObject = findObject(child, object);
        if(foundObject != nullptr) {
            return foundObject;
        }
    }

    return nullptr;
}

/**
 * @brief cwEventRecorderObject::operator ==
 * @param object
 * @return
 */
bool cwEventRecorderObject::operator==(const QObject *object) const
{
    cwEventRecorderObject recorderObject(object);
    const cwEventRecorderObject* current1 = this;
    const cwEventRecorderObject* current2 = &recorderObject;

    while(current1->operatorEqualHelper(*current2)) {
        current1 = current1->Parent.data();
        current2 = current2->Parent.data();

        if(current1 == nullptr || current2 == nullptr) {
            if(current1 == nullptr && current2 == nullptr) {
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

/**
 * @brief cwEventRecorderObject::operator !=
 * @param object
 * @return
 */
bool cwEventRecorderObject::operator!=(const QObject *object) const
{
    return !operator==(object);
}

/**
 * @brief cwEventRecorderObject::populateProperties
 * @param object
 */
void cwEventRecorderObject::populateProperties(const QObject *object)
{
    Q_ASSERT(object != nullptr);

    QString typeString(object->metaObject()->className());
    typeString.replace(QRegExp("(\\w+_QMLTYPE)_\\d+"), "\\1");
    typeString.replace(QRegExp("(\\w+_QML)_\\d+"), "\\1");

    Type = typeString.toLocal8Bit();
    Name = object->objectName().toLocal8Bit();

    for(int i = 0; i < object->metaObject()->propertyCount(); i++) {
        QMetaProperty property = object->metaObject()->property(i);
        QByteArray propertyName = QByteArray(property.name());
        QVariant value = property.read(object);
        Properties.insert(propertyName, value);
    }
}

/**
 * @brief cwEventRecorderObject::operatorEqualHelper
 * @param other
 * @return
 */
bool cwEventRecorderObject::operatorEqualHelper(const cwEventRecorderObject &other) const
{
    if(Name == other.Name && Type == other.Type) {
//        qDebug() << "Name and type are equal!" << Name << Type;
        foreach(const QByteArray& name, Properties.keys()) {
            QVariant property = Properties.value(name);
            QVariant otherProperty = other.Properties.value(name);
//            qDebug() << "Property:" << name << property.toString() << otherProperty.toString() << (property.toString() == otherProperty.toString());
            if(property.canConvert<QString>() && otherProperty.canConvert<QString>()) {
                if(property.toString() != otherProperty.toString()) {
                    return false;
                }
            }
        }
        return true;
    }
//    qDebug() << "Name and Type NOT equal:" << Name << other.Name << (Name == other.Name);
//    qDebug() << "Type Not equal:" << Type << other.Type << (Type == other.Type);
    return false;
}

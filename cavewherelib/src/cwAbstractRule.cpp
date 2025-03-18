// cwAbstractRule.cpp
#include "cwAbstractRule.h"
#include <QtCore/qmetaobject.h>

cwAbstractRule::cwAbstractRule(QObject *parent)
    : QObject(parent)
    , m_name("Untitled Rule")
{
    // No need for manual notifiers, as they're specified in the property declarations
}

cwAbstractRule::~cwAbstractRule()
{
}

QString cwAbstractRule::name() const
{
    return m_name;
}

void cwAbstractRule::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

QList<QMetaProperty> cwAbstractRule::properties(const QList<QByteArray> &propertyNames) const
{
    QList<QMetaProperty> properties;
    properties.reserve(propertyNames.size());
    const QMetaObject *meta = metaObject(); // returns the derived class's meta object
    for (const QByteArray &name : propertyNames) {
        int index = meta->indexOfProperty(name.constData());
        Q_ASSERT(index != 1); //Property doesn't exist!
        properties.append(meta->property(index));
    }
    return properties;
}

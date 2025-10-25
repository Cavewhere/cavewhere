#ifndef CWUNIQUECONNECTIONCHECKER_H
#define CWUNIQUECONNECTIONCHECKER_H

#include <QSet>
#include <QObject>
#include <QDebug>
#include "cwDebug.h"

class cwUniqueConnectionChecker
{
public:
    cwUniqueConnectionChecker() = default;

    // Returns true if this object was not already recorded as connected
    bool add(const QObject* object)
    {
#ifdef QT_DEBUG
        if (object == nullptr) {
            qWarning() << "Object is null. This is a bug" << LOCATION;
            return false;
        }

        if (!m_connectedObjects.contains(object)) {
            m_connectedObjects.insert(object);
            return true;
        } else {
            qWarning() << "Object:" << object << "is already connected! This is likely a double connection bug and will fail in release" << LOCATION;
            return false;
        }
#else
        Q_UNUSED(object);
        return true;
#endif
    }

    // Removes the object from the recorded set in debug builds
    void remove(const QObject* object)
    {
#ifdef QT_DEBUG
        if (object == nullptr) {
            qWarning() << "Object is null. This is a bug" << LOCATION;
            return;
        }

        if (m_connectedObjects.contains(object)) {
            m_connectedObjects.remove(object);
        } else {
            qWarning() << "Object:" << object << "has never been connected! This is a BUG" << LOCATION;
        }
#else
        Q_UNUSED(object);
#endif
    }

private:
#ifdef QT_DEBUG
    QSet<const QObject*> m_connectedObjects;
#endif
};

#endif // CWUNIQUECONNECTIONCHECKER_H

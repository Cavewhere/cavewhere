#ifndef CWCONNECTIONREGISTRY_H
#define CWCONNECTIONREGISTRY_H

// Our includes
#include "cwUniqueConnectionChecker.h"

// Qt includes
#include <QObject>

/**
 * @brief Couples unique-connection bookkeeping with the actual connect/disconnect.
 *
 * Each registry belongs to one receiver QObject — the object whose slots and lambdas
 * the connections target. `add()` records an object and runs its wiring only when the
 * object was not already recorded; `remove()` unrecords the object and tears down every
 * connection between it and the receiver in one wholesale disconnect. Pairing the two
 * halves this way stops the "record" and "wire" lists (and their teardown mirrors) from
 * drifting apart — the connection asymmetry behind issue #576.
 *
 * The registry owns only the primary object's connections. Callers that also wire a
 * secondary source object (a child model, a transformation) or keep bespoke bookkeeping
 * undo those alongside the matching `remove()` call — the registry deliberately does not
 * try to model those, keeping it a thin, predictable primitive.
 */
class cwConnectionRegistry
{
public:
    explicit cwConnectionRegistry(QObject* receiver) :
        m_receiver(receiver)
    {
    }

    // Records object and runs wire() only if it was not already recorded. Returns true
    // when object was newly recorded and wired, false when it was already present.
    template <typename Wire>
    bool add(const QObject* object, Wire&& wire)
    {
        if(!m_checker.add(object)) {
            return false;
        }
        wire();
        return true;
    }

    // Unrecords object and disconnects every connection from it to the receiver.
    void remove(const QObject* object)
    {
        m_checker.remove(object);
        QObject::disconnect(object, nullptr, m_receiver, nullptr);
    }

private:
    QObject* m_receiver;
    cwUniqueConnectionChecker m_checker;
};

#endif // CWCONNECTIONREGISTRY_H

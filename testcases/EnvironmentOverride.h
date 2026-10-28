#ifndef ENVIRONMENTOVERRIDE_H
#define ENVIRONMENTOVERRIDE_H

//Qt includes
#include <QByteArray>
#include <QtGlobal>

//! Sets an environment variable, or unsets it for an empty value, for as
//! long as it lives, and puts back what was there before, so a failing
//! check still leaves the environment the way the suite found it
class EnvironmentOverride
{
public:
    EnvironmentOverride(const char* name, const QByteArray& value) :
        m_name(name),
        m_hadValue(qEnvironmentVariableIsSet(name)),
        m_previous(qgetenv(name))
    {
        if (value.isEmpty()) {
            qunsetenv(m_name);
        } else {
            qputenv(m_name, value);
        }
    }

    ~EnvironmentOverride()
    {
        if (m_hadValue) {
            qputenv(m_name, m_previous);
        } else {
            qunsetenv(m_name);
        }
    }

    EnvironmentOverride(const EnvironmentOverride&) = delete;
    EnvironmentOverride& operator=(const EnvironmentOverride&) = delete;

private:
    const char* m_name;
    bool m_hadValue;
    QByteArray m_previous;
};

#endif // ENVIRONMENTOVERRIDE_H

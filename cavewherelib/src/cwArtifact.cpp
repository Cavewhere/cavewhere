// cwArtifact.cpp
#include "cwArtifact.h"

cwArtifact::cwArtifact(QObject *parent)
    : QObject(parent)
    , m_name("Untitled")
{
}

cwArtifact::cwArtifact(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
{
}

cwArtifact::~cwArtifact()
{
}

QString cwArtifact::name() const
{
    return m_name;
}

void cwArtifact::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

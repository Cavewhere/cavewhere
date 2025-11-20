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

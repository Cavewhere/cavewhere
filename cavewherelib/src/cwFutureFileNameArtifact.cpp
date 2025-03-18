#include "cwFutureFileNameArtifact.h"


void cwFutureFileNameArtifact::setFilename(const QFuture<QString> &newFilename)
{
    m_filename = newFilename;
    emit filenameChanged();
}

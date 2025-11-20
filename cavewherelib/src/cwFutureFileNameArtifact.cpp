#include "cwFutureFileNameArtifact.h"


void cwFutureFileNameArtifact::setFilename(const QFuture<Monad::ResultString> &newFilename)
{
    m_filename = newFilename;
    emit filenameChanged();
}

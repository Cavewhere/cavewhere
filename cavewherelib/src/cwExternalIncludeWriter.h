/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALINCLUDEWRITER_H
#define CWEXTERNALINCLUDEWRITER_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPocketTopoTranslator.h"
#include "cwSurvexExporterUtils.h"

//Qt includes
#include <QString>
#include <QTextStream>

/**
 * How a driver reads an attached file: the region solve's driver and the
 * station harvest's both go through here, so a format cavern cannot read
 * reaches them the same way.
 */
namespace cwExternalIncludeWriter {

/**
 * Writes `*include "<absolutePath>"`, or, for a PocketTopo .top, which cavern
 * has no reader for, the Survex cwPocketTopoTranslator turns it into, using
 * \a pocketTopo's fix system and declinations (its topPath is replaced by
 * \a absolutePath). Returns false and writes nothing when a .top cannot be
 * read.
 */
CAVEWHERE_LIB_EXPORT bool write(QTextStream& stream,
                                const QString& absolutePath,
                                const cwPocketTopoTranslator::Input& pocketTopo,
                                cwSurvexExporterUtils::CsScope& scope);

} // namespace cwExternalIncludeWriter

#endif // CWEXTERNALINCLUDEWRITER_H

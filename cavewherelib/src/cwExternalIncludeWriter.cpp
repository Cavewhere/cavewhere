/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalIncludeWriter.h"
#include "cwExternalCenterlineScanner.h"

bool cwExternalIncludeWriter::write(QTextStream& stream,
                                    const QString& absolutePath,
                                    const cwPocketTopoTranslator::Input& pocketTopo,
                                    cwSurvexExporterUtils::CsScope& scope)
{
    if (cwExternalCenterlineScanner::formatFor(absolutePath)
        == cwExternalCenterlineScanner::Format::PocketTopo) {
        cwPocketTopoTranslator::Input input = pocketTopo;
        input.topPath = absolutePath;
        const std::optional<QString> translated = cwPocketTopoTranslator::translate(input, scope);
        if (translated.has_value()) {
            stream << *translated;
            return true;
        }
        return false;
    }

    stream << "*include \"" << absolutePath << "\"" << Qt::endl;
    return true;
}

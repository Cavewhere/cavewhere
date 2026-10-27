/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPASSMAKTRANSLATOR_H
#define CWCOMPASSMAKTRANSLATOR_H

//Our includes
#include "cwFixStation.h"
#include "cwGlobals.h"
#include "cwSurvexExporterUtils.h"

//Qt includes
#include <QList>
#include <QString>

//Std includes
#include <optional>

/**
 * The Survex a driver writes in place of `*include file.mak`, so that cavern
 * reads a Compass project's fixes through their datum and zone. Cavern's own
 * .mak reader (survex datain.c) hands a "[f,...]" fix to fix_station as raw
 * output coordinates, while a Survex *fix goes through the input *cs.
 *
 * The text is one unnamed *begin holding, per DAT the .mak lists, a
 * `*begin <survey>` block named the way cavern names the DAT's survey
 * (cwCompassMakFile::datSurveyName), so the stations keep their names:
 *
 *   - `*cs` and `*declination auto` from the base location ('@') when a datum
 *     reads it, the way cavern sets its declination location;
 *   - `*include` of the DAT;
 *   - `*fix` and `*entrance` for each station the '#' line fixes, under the
 *     `*cs` its datum and zone name, with feet converted to meters, or under
 *     the output system when they name none, which is where cavern put it;
 *
 * After the blocks, one `*equate` per link station joins it to the station of
 * the same name in the most recent earlier DAT that has one, as cavern does.
 * `*case preserve` keeps Compass's case-sensitive names, and `*set names`
 * admits the punctuation they use.
 */
namespace cwCompassMakTranslator {

struct Input {
    //! The project's copy of the .mak.
    QString makPath;
    //! The system the driver's *cs out names, empty when it names none. With
    //! no output system cavern refuses an input *cs and `*declination auto`,
    //! so the fixes are written as raw coordinates and no location is set.
    QString outputCS;
    //! The node's own fixes on the file's stations, named as the file's
    //! stations are ("<survey>.<station>"), already validated.
    QList<cwFixStation> nodeFixes;
};

/**
 * The Survex text for \a input, read with \a scope's system in force around
 * it, or nothing when the .mak cannot be read, a DAT path cannot be quoted,
 * a fix bracket is unreadable (cavern then reports it), or a survey or station name has a character Survex cannot spell (a blank,
 * '.', ';', '"', '\', '*' or a non-ASCII character). The caller then includes
 * the .mak as it is.
 */
CAVEWHERE_LIB_EXPORT std::optional<QString> translate(const Input& input,
                                                      const cwSurvexExporterUtils::CsScope& scope);

} // namespace cwCompassMakTranslator

#endif // CWCOMPASSMAKTRANSLATOR_H

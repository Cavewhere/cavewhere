/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOCKETTOPOTRANSLATOR_H
#define CWPOCKETTOPOTRANSLATOR_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPocketTopoFile.h"
#include "cwSurvexExporterUtils.h"

//Qt includes
#include <QHash>
#include <QString>

//Std includes
#include <optional>

/**
 * The Survex a driver writes in place of `*include file.top`, since cavern has
 * no PocketTopo reader. The driver writes the `*begin <rootLabel>` around it;
 * the text is one namespace, because PocketTopo stations are file-global.
 *
 * After the `*units` and `*data` header, each trip in file order is a switch
 * inside that block: a `; trip #n : <name>` comment, `*date`, one
 * declination directive, `*team` lines, then its shots in file order. All the
 * readings of one station pair recorded in a row are written together, each
 * as its own leg in the first reading's direction (a reading recorded the
 * other way gets azimuth +180 and its clino negated), so cavern merges them;
 * the splays met inside that run follow it as `*flags splay` legs to `..`.
 * A zero-length leg between two named stations is `*equate`. Shots that
 * belong to no trip are left out. The references follow the trips as `*fix`
 * in meters.
 *
 * A plain station `N` cannot share a file with a dotted `N.x` in Survex, so
 * when both occur `N` is written `N_` everywhere (see stationNames()).
 */
namespace cwPocketTopoTranslator {

struct Input {
    //! The project's copy of the .top.
    QString topPath;
    //! The system the references are read under, put in scope before the
    //! `*fix` lines (a trip's `*declination auto` can leave another one there).
    //! Empty reads them in whatever system is in scope.
    QString fixCS;
    //! Where `*declination auto` is taken for a trip whose declination is
    //! Auto, when the node has a location. Without one, an Auto trip takes its
    //! value from injectedDeclinations.
    std::optional<cwSurvexExporterUtils::DeclinationContext> autoDeclinationLocation;
    //! Degrees for each Auto trip, keyed by its index in the file (0-based),
    //! used when there is no autoDeclinationLocation. A trip with no entry
    //! gets 0.
    QHash<int, double> injectedDeclinations;
};

/**
 * The Survex text for \a input, read with \a scope's system in force around
 * it, or nothing when the .top cannot be read.
 */
CAVEWHERE_LIB_EXPORT std::optional<QString> translate(const Input& input,
                                                      const cwSurvexExporterUtils::CsScope& scope);

/**
 * The name the translation writes for each station \a document uses, keyed by
 * StationId::raw(): PocketTopo's own name, except a plain `N` that some dotted
 * `N.x` shares, which becomes `N_`.
 */
CAVEWHERE_LIB_EXPORT QHash<quint32, QString> stationNames(const cwPocketTopoFile::Document& document);

} // namespace cwPocketTopoTranslator

#endif // CWPOCKETTOPOTRANSLATOR_H

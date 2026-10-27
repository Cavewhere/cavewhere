/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEX3DFILEREADER_H
#define CWSURVEX3DFILEREADER_H

//Our includes
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>

// Survex's closure error for the traverse a leg sits in, in standard deviations:
// total (e), horizontal (h) and vertical (v).
struct cwLegError {
    float e = 0.0f;
    float h = 0.0f;
    float v = 0.0f;
};

// Unordered pair of solved station keys -> the error of the traverse the leg sits in
using cwLegErrorsByShot = QHash<QPair<QString, QString>, cwLegError>;

/**
 * Reads cavern's .3d output into a position lookup and a survey network.
 *
 * Coordinates arrive in whatever *cs out named, which for CaveWhere's export is
 * the project's local projection — centered on the project and small enough to
 * narrow to QVector3D without losing metres to float.
 */
class CAVEWHERE_LIB_EXPORT cwSurvex3DFileReader
{
public:
    // Every station is keyed by cwStation::canonicalKey, except the members of
    // a case pair (labels that lowercase alike at different coordinates), which
    // key by cwStation::caseDistinctKey.
    struct NetworkAndLookup {
        cwSurveyNetwork network;
        cwStationPositionLookup lookup;
        cwSplayTipsByStation splayTips;
        //! Keys of the stations the data fixes with *fix (or a format's own
        //! fix), in .3d order. The origin cavern invents for a survey with no
        //! fix is excluded.
        QStringList fixedStations;

        // The img_ERROR_INFO closure error of every leg that sits in a loop,
        // keyed by legKeyOfKeys() of its two station keys. Legs outside any
        // loop (dead ends, articulation legs, hanging traverses), splays, and
        // traverses cavern reports with zero error are absent.
        cwLegErrorsByShot legErrors;

        //! Each solved key's spelling: survey levels lowercased, the station
        //! name as cavern wrote it.
        QHash<QString, QString> spellings;

        //! Each solved key's label as cavern wrote it, with the separator
        //! turned to '.': every level in cavern's case.
        QHash<QString, QString> labels;
    };

    // The sorted pair of canonical station names, so a leg reads the same
    // key in either direction.
    static QPair<QString, QString> legKey(const QString& from, const QString& to);

    //! The leg between two solved keys, ordered, keys kept as given.
    static QPair<QString, QString> legKeyOfKeys(const QString& fromKey, const QString& toKey);

    // Parses a .3d file once, returning the survey network (station names,
    // shot connectivity, positions), a standalone position lookup, and the
    // splay tips. Two-pass using img_rewind(): pass 1 indexes LABEL items,
    // pass 2 resolves LINE endpoints by coordinate, naming each end under the
    // leg's own survey.
    NetworkAndLookup readNetworkAndLookup(const QString& threeDFilePath);
};

#endif // CWSURVEX3DFILEREADER_H

/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStationRenamer.h"

cwStationRenamer::cwStationRenamer()
{
}

const QRegExp cwStationRenamer::InvalidCharRegExp("[^-_a-zA-Z0-9]");

/**
 * @brief cwStationRenamer::createStation creates a new station based upon the
 * given originalName.
 * @param originalName the name of the station in the original data (e.g.
 * Compass or Walls import)
 * @return a cwStation with originalName if it is valid, and otherwise with all
 * invalid chars in originalName replaced with _ and a _1, _2, etc. at the end
 * if necessary to disambiguate.  Since input formats are case-sensitive but
 * Cavewhere is not, if for instance the data contain LD40 and ld40, one will be
 * given a number suffix so that Cavewhere doesn't consider them the same.
 * So for example, with the following stations:
 *   LD40
 *   ld40
 *   FR:W$B
 *   FR:W%B
 *   FR:w%B
 * The created station names would be:
 *   LD40
 *   ld40_1
 *   FR_W_B
 *   FR_W_B_1
 *   FR_w_B_2
 */
cwStation cwStationRenamer::createStation(QString originalName)
{
    QString newName = OriginalToRenamedStations.value(originalName);

    if(newName.isEmpty()) {
        newName = originalName;

        // replace invalid chars with "_"
        // e.g. FR:WB$ -> FR_WB_
        newName.replace(InvalidCharRegExp, "_");

        // if uppercase of new name already exists try adding _1, _2, etc.
        // until that doesn't exist
        if (UpperCaseRenamedStations.contains(newName.toUpper())) {
            int num = 1;
            QString numberedName;
            do {
                numberedName = QString("%1_%2").arg(newName).arg(num);
            } while (UpperCaseRenamedStations.contains(numberedName.toUpper()));
            newName = numberedName;
        }

        Q_ASSERT(cwStation::nameIsValid(newName));

        UpperCaseRenamedStations << newName.toUpper();
        OriginalToRenamedStations.insert(originalName, newName);
        RenamedToOriginalStations.insert(newName, originalName);
    }

    return cwStation(newName);
}

/**
 * @brief cwStationRenamer::clear
 *
 * This clears all the history for this station renamer. All created stations will be forgotten.
 */
void cwStationRenamer::clear()
{
    OriginalToRenamedStations.clear();
    RenamedToOriginalStations.clear();
    UpperCaseRenamedStations.clear();
}

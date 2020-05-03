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

/**
 * @brief cwStationRenamer::createStation creates a new station based upon the
 * given originalName.
 * @param originalName the name of the station in the original data (e.g.
 * Compass or Walls import)
 * @return a cwStation with originalName if it is valid, and otherwise with all
 * invalid chars in originalName replaced with _ and a -1, -2, etc. at the end
 * or -1-, -2-, etc. before the number if necessary to disambiguate.
 * Since input formats are case-sensitive but Cavewhere is not, if for instance
 * the data contain LD40 and ld40, one will be given a disambiguating number
 * infix or suffix so that Cavewhere doesn't consider them the same.
 * So for example, with the following stations:
 *   LD40
 *   ld40
 *   FR:W$B
 *   FR:W%B
 *   FR:w%B
 * The created station names would be:
 *   LD40
 *   ld-1-40
 *   FR_W_B
 *   FR_W_B-1
 *   FR_w_B-2
 */
cwStation cwStationRenamer::createStation(QString originalName)
{
    if(originalName.isEmpty()) {
        return cwStation();
    }

    QString newName = OriginalToRenamedStations.value(originalName);

    if(newName.isEmpty()) {
        newName = originalName;

        // replace invalid chars with "_"
        // e.g. FR:WB$ -> FR_WB_
        newName.replace(invalidCharRegExp(), "_");

        // if uppercase of new name already exists, add something in to disambiguate
        if (UpperCaseRenamedStations.contains(newName.toUpper())) {
            QRegExp namePartRegExp("\\d+\\D*$");

            // determine where to insert the disambiguating number.
            // if there are digits in the name, we will insert the disambiguating number
            // before the last string of digits.  Otherwise, we'll append it to the end
            int insertIndex = namePartRegExp.indexIn(newName);
            if (insertIndex < 0) insertIndex = newName.length();
            QString prefix = newName.left(insertIndex);
            QString suffix = newName.mid(insertIndex);

            int num = 1;
            QString numberedName;
            do {
                if (suffix.length()) {
                    numberedName = QString("%1-%2-%3").arg(prefix).arg(num).arg(suffix);
                }
                else {
                    numberedName = QString("%1-%2").arg(prefix).arg(num);
                }
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

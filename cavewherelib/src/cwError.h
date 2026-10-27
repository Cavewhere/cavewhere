/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSURVEYCHUNKERROR_H
#define CWSURVEYCHUNKERROR_H

//Qt includes
#include <QSharedDataPointer>
#include <QObject>
#include <QQmlEngine>
#include <QUuid>

//Our includes
class cwErrorData;
#include "cwGlobals.h"

/**
 * Stable, cross-version identifiers for the cwErrors whose user suppression must
 * survive their message text changing between releases (see cwError::errorTypeId).
 * Values are stable once written to disk (the survey ids live in
 * StationShot.ignoredWarnings), so each value keeps its meaning forever: append
 * a new id at the end with the next number, and leave existing values as they
 * are. Allocate every id here rather than as a literal at the use site, so two
 * features get distinct values. 0 is the default "no stable identity" for
 * transient errors.
 */
enum class cwErrorTypeId : int {
    None = 0,
    FixStationOutlier = 1,   //!< a fix far from the survey cluster
    FixStationDomain = 2,    //!< a fix outside its CS's valid domain
    FixStationReference = 3, //!< a fix names a station not in the survey

    // Survey-editor warnings, one id per (kind x cell) so a station's or
    // shot's ignored set can name exactly one box
    SurveyMissingLeft = 4,
    SurveyMissingRight = 5,
    SurveyMissingUp = 6,
    SurveyMissingDown = 7,
    SurveyMissingCompass = 8,       //!< LRUD-only zero-length shot
    SurveyMissingBackCompass = 9,
    SurveyMissingClino = 10,        //!< LRUD-only zero-length shot
    SurveyMissingBackClino = 11,
    SurveyCompassDisagreement = 12, //!< on the frontsight compass cell
    SurveyClinoDisagreement = 13,   //!< on the frontsight clino cell

    UnconnectedStations = 14,       //!< a trip's or attached file's stations that the solve could not tie to the cave
    AttachedFileUnfixed = 15,       //!< an attached file that fixes no station, placed at the origin in a georeferenced project
    AttachedFileDefaultDatum = 16,  //!< an attached Compass .mak that names a UTM zone but no datum, read in North American 1927
    AttachedFileFixWithoutCS = 17,  //!< an attachment with fixes that name no coordinate system and no system chosen for them, placed with its first such fix at the project origin
    AttachedFileReadWarnings = 18,  //!< advisories the scan of an attached file raised: a missing *include, a case-fallback match, an encoding fallback, a value it could not parse
    SurveyEmptyShot = 19,           //!< on the distance cell of an empty shot inside a chunk
    AttachedFileUnsolved = 20,      //!< a survey file attached to a trip inside a source root, which the solve never reads
};

/**
 * @brief The cwError class
 *
 * The error supports generic errors created by users while entering data. For example
 * the cwSurveyChunk generates errors for missing distance, compass, clino and etc. data. These
 * errors are reported to the cavewhere user for them to correct. There are two types or errors:
 * Fatal and Warning. Fatal errors prevent cavewhere from finishing processing data, for example
 * loop closure, where as Warning help the user from entering bad data, for example compass of by
 * 2 degs error. Warnings can generally be suppressed by the user.
 *
 * Also see cwErrorModel.
 */
class CAVEWHERE_LIB_EXPORT cwError
{
    Q_GADGET
    QML_VALUE_TYPE(cwError)

    Q_PROPERTY(bool suppressed READ suppressed WRITE setSupressed)
    Q_PROPERTY(int errorTypeId READ errorTypeId WRITE setErrorTypeId)
    Q_PROPERTY(QString message READ message WRITE setMessage)
    Q_PROPERTY(QString detail READ detail WRITE setDetail)
    Q_PROPERTY(QUuid targetId READ targetId WRITE setTargetId)
    Q_PROPERTY(ErrorType type READ type WRITE setType)

public:
    enum ErrorType {
        Warning, //Survex should still run
        Fatal, //Survex will not run
        NoError
    };
    Q_ENUM(ErrorType)

    cwError();
    cwError(const QString& message, ErrorType type = Warning);
    cwError(const cwError &);
    cwError &operator=(const cwError &);
    ~cwError();

    ErrorType type() const;
    void setType(ErrorType type);

    int errorTypeId() const;
    void setErrorTypeId(int errorTypeId);

    bool suppressed() const;
    void setSupressed(bool suppressed);

    QString message() const;
    void setMessage(QString message);

    //! Supporting text a view shows beneath the message, such as the station
    //! names a warning counts. Empty when the message says everything.
    QString detail() const;
    void setDetail(QString detail);

    //! The id of the object the user edits to resolve this error: a fix
    //! station, a trip, or a survey node. Null when no single object does.
    QUuid targetId() const;
    void setTargetId(const QUuid& targetId);

    bool operator==(const cwError& error) const;
    bool operator!=(const cwError& error) const;

    static bool isFatal(const cwError& error) {
        return error.type() == Fatal;
    }

    static bool containsFatal(const QList<cwError>& errors) {
        return std::find_if(errors.begin(), errors.end(), &cwError::isFatal) != errors.end();
    }

private:
    QSharedDataPointer<cwErrorData> data;
};

//For exposing enum to qml
//See qt doc: https://doc.qt.io/qt-6/qtqml-cppintegration-definetypes.html#:~:text=Value%20types%20have%20lower%20case,to%20expose%20the%20enumeration%20separately.
class cwErrorDerived : public cwError
{
    Q_GADGET
};

namespace cwErrorDerivedForeign {
    Q_NAMESPACE
    QML_NAMED_ELEMENT(CwError)
    QML_FOREIGN_NAMESPACE(cwErrorDerived)
}


Q_DECLARE_METATYPE(cwError)


#endif // CWSURVEYCHUNKERROR_H

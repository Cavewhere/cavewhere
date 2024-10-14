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

//Our includes
class cwErrorData;
#include "cwGlobals.h"

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

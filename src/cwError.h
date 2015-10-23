/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSURVEYCHUNKERROR_H
#define CWSURVEYCHUNKERROR_H

#include <QSharedDataPointer>
#include <QObject>

class cwErrorData;

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
class cwError
{
    Q_GADGET
    Q_ENUMS(ErrorType)

    Q_PROPERTY(bool suppressed READ suppressed WRITE setSupressed)
    Q_PROPERTY(int index READ index WRITE setIndex)
    Q_PROPERTY(int role READ role WRITE setRole)
    Q_PROPERTY(int errorTypeId READ errorTypeId WRITE setErrorTypeId)
    Q_PROPERTY(QString message READ message WRITE setMessage)
    Q_PROPERTY(QObject* parent READ parent WRITE setParent)

public:
    enum ErrorType {
        Warning, //Survex should still run
        Fatal, //Survex will not run
        NoError
    };

    cwError();
    cwError(const cwError &);
    cwError &operator=(const cwError &);
    ~cwError();

    ErrorType type() const;
    void setType(ErrorType type);

    int index() const;
    void setIndex(int index);

    int role() const;
    void setRole(int role);

    int errorTypeId() const;
    void setErrorTypeId(int errorTypeId);

    bool suppressed() const;
    void setSupressed(bool suppressed);

    QString message() const;
    void setMessage(QString message);

    QObject* parent() const;
    void setParent(QObject* parent);

    bool operator==(const cwError& error) const;
    bool operator!=(const cwError& error) const;

private:
    QSharedDataPointer<cwErrorData> data;
};

Q_DECLARE_METATYPE(cwError)

#endif // CWSURVEYCHUNKERROR_H

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

class cwSurveyChunkErrorData;

class cwSurveyChunkError
{
    Q_GADGET
    Q_ENUMS(Error ErrorType)

    Q_PROPERTY(bool suppressed READ suppressed WRITE setSupressed)
    Q_PROPERTY(Error error READ error WRITE setError)
    Q_PROPERTY(ErrorType type READ type WRITE setType)
    Q_PROPERTY(QString message READ message WRITE setMessage)

public:
    enum ErrorType {
        Warning, //Survex should still run
        Fatal, //Survex will not run
        Unknown
    };

    enum Error {
        NoError,
        MissingData,
        DataNotValid,
        DataDuplicated,
        BackSightDisagreement
    };

    cwSurveyChunkError();
    cwSurveyChunkError(const cwSurveyChunkError &);
    cwSurveyChunkError &operator=(const cwSurveyChunkError &);
    ~cwSurveyChunkError();

    ErrorType type() const;
    void setType(ErrorType type);

    Error error() const;
    void setError(Error error);

    bool suppressed() const;
    void setSupressed(bool suppressed);

    QString message() const;
    void setMessage(QString message);

    bool operator==(const cwSurveyChunkError& error) const;

private:
    QSharedDataPointer<cwSurveyChunkErrorData> data;
};

Q_DECLARE_METATYPE(cwSurveyChunkError)

#endif // CWSURVEYCHUNKERROR_H

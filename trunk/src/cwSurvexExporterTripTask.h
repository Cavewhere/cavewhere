#ifndef CWSURVEXEXPORTERTRIPTASK_H
#define CWSURVEXEXPORTERTRIPTASK_H

//Our includes
#include "cwSurvexExporterTask.h"
class cwTrip;
class cwSurveyChunk;

//Qt includes
class QTextStream;


class cwSurvexExporterTripTask : public cwSurvexExporterTask
{
    Q_OBJECT

public:
    explicit cwSurvexExporterTripTask(QObject *parent = 0);

    void setData(const cwTrip& trip);

    void writeTrip(QTextStream& stream, cwTrip* trip);

signals:

protected:
    virtual void runTask();

public slots:

private:
    cwTrip* Trip;
    static const int TextPadding;

    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);

};

#endif // CWSURVEXEXPORTERTRIPTASK_H

#ifndef CWSURVEXGLOBALDATA_H
#define CWSURVEXGLOBALDATA_H

//Our includes
class cwSurvexBlockData;

//Qt includes
#include <QObject>
#include <QList>

class cwSurvexGlobalData : public QObject
{
    friend class cwSurvexImporter;

public:
    cwSurvexGlobalData(QObject* object);

    QList<cwSurvexBlockData*> blocks() const;


private:
    QList<cwSurvexBlockData*> RootBlocks;

    void AddFixSatation();
    void AddEquate();

    void setBlocks(QList<cwSurvexBlockData*> blocks);
};

inline QList<cwSurvexBlockData*> cwSurvexGlobalData::blocks() const {
    return RootBlocks;
}


#endif // CWSURVEXGLOBALDATA_H

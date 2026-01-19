/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEAMMEMBER_H
#define CWTEAMMEMBER_H

//Qt includes
#include <QString>
#include <QStringList>
#include <QUuid>
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwTeamMember {
public:
    explicit cwTeamMember();
    cwTeamMember(QString name, QStringList jobs);

    QUuid id() const { return m_id; }
    void setId(const QUuid id) { m_id = id; }

    void setName(QString name);
    QString name() const;

    void setJobs(QStringList jobs);
    QStringList jobs() const;

private:
    QUuid m_id;
    QString Name;
    QStringList Jobs;
};

inline QString cwTeamMember::name() const {
    return Name;
}

inline QStringList cwTeamMember::jobs() const {
    return Jobs;
}


#endif // CWTEAMMEMBER_H

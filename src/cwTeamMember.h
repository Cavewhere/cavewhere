#ifndef CWTEAMMEMBER_H
#define CWTEAMMEMBER_H

//Qt includes
#include <QString>
#include <QStringList>

class cwTeamMember
{
public:
    explicit cwTeamMember();
    cwTeamMember(QString name, QStringList jobs);

    void setName(QString name);
    QString name() const;

    void setJobs(QStringList jobs);
    QStringList jobs() const;

private:
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

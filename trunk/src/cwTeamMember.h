#ifndef CWTEAMMEMBER_H
#define CWTEAMMEMBER_H

//Qt includes
#include <QString>
#include <QStringList>

class cwTeamMember
{
public:
    explicit cwTeamMember();
    cwTeamMember(QString name, QStringList roles);

    void setName(QString name);
    QString name() const;

    void setRoles(QStringList roles);
    QStringList roles() const;

private:
    QString Name;
    QStringList Roles;
};

inline QString cwTeamMember::name() const {
    return Name;
}

inline QStringList cwTeamMember::roles() const {
    return Roles;
}


#endif // CWTEAMMEMBER_H

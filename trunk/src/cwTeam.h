#ifndef CWTEAM_H
#define CWTEAM_H

//Qt includes
#include <QAbstractListModel>

//Our includes
#include <cwTeamMember.h>

class cwTeam : public QAbstractListModel
{
    Q_OBJECT
public:
    enum {
        NameRole,

        //In a sublist
        JobRole
    } Roles;

    explicit cwTeam(QObject *parent = 0);
    cwTeam(const cwTeam& team);

    void addTeamMember(const cwTeamMember& teamMember);
    void setTeamMembers(QList<cwTeamMember> team);
    QList<cwTeamMember> teamMembers() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

signals:

public slots:

private:
    QList<cwTeamMember> Team;

    void setupRoles();

};

/**
  Gets all the team members
  */
inline QList<cwTeamMember> cwTeam::teamMembers() const {
    return Team;
}


#endif // CWTEAM_H

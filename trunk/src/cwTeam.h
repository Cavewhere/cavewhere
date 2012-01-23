#ifndef CWTEAM_H
#define CWTEAM_H

//Qt includes
#include <QAbstractListModel>

//Our includes
#include <cwTeamMember.h>

class cwTeam : public QAbstractListModel
{
    Q_OBJECT

    Q_ENUMS(TeamModelRoles)

public:
    enum TeamModelRoles{
        NameRole,

        //In a sublist
        JobsRole
    };

    explicit cwTeam(QObject *parent = 0);
    cwTeam(const cwTeam& team);

    Q_INVOKABLE void addTeamMember();
    void addTeamMember(const cwTeamMember& teamMember);

    Q_INVOKABLE void removeTeamMember(int row);

    void setTeamMembers(QList<cwTeamMember> team);
    QList<cwTeamMember> teamMembers() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE void setData(int index, cwTeam::TeamModelRoles role, const QVariant& data);
    bool setData(QModelIndex index, int role, const QVariant& data);


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

/**
  \brief Adds a team member to the team
  */
inline void cwTeam::addTeamMember() {
    addTeamMember(cwTeamMember());
}



#endif // CWTEAM_H

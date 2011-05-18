//Our includes
#include "cwTeam.h"

//Qt includes
#include <QByteArray>

cwTeam::cwTeam(QObject *parent) :
    QAbstractListModel(parent)
{
    setupRoles();
}

/**
  Copy constructor for the team
  */
cwTeam::cwTeam(const cwTeam& team) :
    QAbstractListModel(NULL)
{
    setupRoles();

    //Copy the data
    Team = team.Team;
}

void cwTeam::addTeamMember(const cwTeamMember& teamMember) {
    beginInsertRows(QModelIndex(), Team.size(), Team.size());
    Team.append(teamMember);
    endInsertRows();
}

void cwTeam::setTeamMembers(QList<cwTeamMember> team) {
    beginResetModel();
    Team = team;
    endResetModel();
}

int cwTeam::rowCount(const QModelIndex &/*parent*/) const {
    return Team.size();
}

QVariant cwTeam::data(const QModelIndex &index, int role) const {
    const cwTeamMember& teamMember = Team.at(index.row());
    switch(role) {
    case NameRole:
        return teamMember.name();
    case SurveyRolesRole:
        return teamMember.roles();
    }
    return QVariant();
}

/**
  Sets up the roles for this model, this is used for qml
  */
void cwTeam::setupRoles() {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[SurveyRolesRole] = "roles";
    setRoleNames(roles);
}

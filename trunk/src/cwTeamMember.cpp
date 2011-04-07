#include "cwTeamMember.h"

cwTeamMember::cwTeamMember() {
}

cwTeamMember::cwTeamMember(QString name, QStringList roles) {
    Name = name;
    Roles = roles;
}

/**
  Sets the name of a team member of a survey trip
  */
void cwTeamMember::setName(QString name) {
    Name = name;
}

/**
  Sets the roles of a team member
  */
void cwTeamMember::setRoles(QStringList roles)  {
    Roles = roles;
}

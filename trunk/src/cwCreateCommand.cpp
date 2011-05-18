#include "cwCreateCommand.h"

const QString cwCreateCommand::CommandText = "Create Object";

cwCreateCommand::cwCreateCommand(QObject* object) {
    OwnsPointer = false;
    ObjectPtr = QWeakPointer<QObject>(object);
    setText(CommandText);
}

cwCreateCommand::~cwCreateCommand() {
    if(ObjectPtr.isNull()) { return; }
    if(!OwnsPointer) { return; }
    ObjectPtr.data()->deleteLater();
}

void cwCreateCommand::redo() {
    OwnsPointer = false;
}

void cwCreateCommand::undo() {
    OwnsPointer = true;
}

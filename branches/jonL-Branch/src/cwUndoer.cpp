#include "cwUndoer.h"

cwUndoer::cwUndoer(QUndoStack* stack)
{
    setUndoStack(stack);
}

/**
  \brief Pushes a undo command onto the undo stack

  If the undo stack hasn't been set, this does nothing

  */
void cwUndoer::pushUndo(QUndoCommand* command) {
    if(Stack == NULL) {
        command->redo();
        delete command;
        return;
    }
    Stack->push(command);
}

/**
  \brief See QUndoStack::beginMacro() Qt docs for details

  If the undo stack hasn't been set, this does nothing
  */
void cwUndoer::beginUndoMacro(const QString& text) {
    if(Stack == NULL) { return; }
    Stack->beginMacro(text);
}

/**
  \brief See QUndoStack::endMacro() Qt docs for details

  If the undo stack hasn't been set, this does nothing
  */
void cwUndoer::endUndoMacro() {
    if(Stack == NULL) { return; }
    Stack->endMacro();
}

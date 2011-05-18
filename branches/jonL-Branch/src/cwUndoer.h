#ifndef CWUNDOER_H
#define CWUNDOER_H

//Qt includes
#include <QUndoStack>

/**
  \brief This allows objects to undo themselves and have
  access to the undoStack.

  See the Undo Framework in Qt docs for details
  */
class cwUndoer
{
public:
    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

protected:
    cwUndoer(QUndoStack* undoStack = NULL);
    void pushUndo(QUndoCommand* command);
    void beginUndoMacro(const QString& text);
    void endUndoMacro();

private:
    QUndoStack* Stack;

};

/**
  \brief Gets the current undo stack
  */
inline QUndoStack* cwUndoer::undoStack() const {
    return Stack;
}


inline void cwUndoer::setUndoStack(QUndoStack* undoStack)  {
    Stack = undoStack;
}

#endif // CWUNDOER_H

/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALUNDOSTACK_H
#define CWGLOBALUNDOSTACK_H

//Qt includes
#include <QUndoStack>
#include <QUndoCommand>

class cwGlobalUndoStack
{
public:
    cwGlobalUndoStack();

    static void setGlobalUndoStack(QUndoStack* undoStack);
    static QUndoStack* globalUndoStack();
    static void push(QUndoCommand* command);
    static void beginMacro(const QString& text);
    static void endMacro();

private:

    static QUndoStack* UndoStack;

};

/**
  Sets the global undo stack
  */
inline void cwGlobalUndoStack::setGlobalUndoStack(QUndoStack* undoStack) {
    UndoStack = undoStack;
}

/**
  Gets the global undo stack
  */
inline QUndoStack* cwGlobalUndoStack::globalUndoStack()  {
    return UndoStack;
}

inline void cwGlobalUndoStack::push(QUndoCommand* command) {
    UndoStack->push(command);
}

inline void cwGlobalUndoStack::beginMacro(const QString& text) {
    UndoStack->beginMacro(text);
}

inline void cwGlobalUndoStack::endMacro() {
    UndoStack->endMacro();
}


#endif // CWGLOBALUNDOSTACK_H

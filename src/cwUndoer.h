/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNDOER_H
#define CWUNDOER_H

//Qt includes
#include <QUndoStack>
#include <QSet>

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

//    void addUndoChild(cwUndoer* undoer);
//    void removeUndoChild(cwUndoer* undoer);
//    void clearChildren();

protected:
    QUndoStack* UndoStack;

    cwUndoer(QUndoStack* undoStack = nullptr);
    void pushUndo(QUndoCommand* command);
    void beginUndoMacro(const QString& text);
    void endUndoMacro();

    /// You need to implement this function in the subclass
    /// Make sure you set UndoStack and then call setUndoStackForChildrenHelper()
    virtual void setUndoStackForChildren() { }

    /**
      \brief This sets the undo stack for the undoer's children

      This is a helper function when implementing setUndoStack
      */
    template <class UndoerSubClass>
    void setUndoStackForChildrenHelper(QList<UndoerSubClass*> children) {
        foreach(UndoerSubClass* undoer, children) {
            undoer->setUndoStack(UndoStack);
        }
    }
};

/**
  \brief Gets the current undo stack
  */
inline QUndoStack* cwUndoer::undoStack() const {
    return UndoStack;
}

///**
//  \brief Clears all the children
//  */
//inline void cwUndoer::clearChildren() {
//    ChildUndors.clear();
//}

///**
//  Adds a child under
//  */
//void cwUndoer::addUndoChild(cwUndoer* undoer) {
//    ChildUndors.insert(undoer);
//}

///**
//  \brief Removes a child
//  */
//void cwUndoer::removeUndoChild(cwUndoer* undoer) {
//    ChildUndors.remove(undoer);
//}



#endif // CWUNDOER_H

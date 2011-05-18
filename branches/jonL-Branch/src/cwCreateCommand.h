#ifndef CWCREATECOMMAND_H
#define CWCREATECOMMAND_H

#include <QUndoCommand>
#include <QWeakPointer>
#include <QObject>

/**
  \brief The create command is
  */
class cwCreateCommand : public QUndoCommand
{
public:
    cwCreateCommand(QObject* object);
    ~cwCreateCommand();
    void redo();
    void undo();

private:
    static const QString CommandText;
    QWeakPointer<QObject> ObjectPtr;
    bool OwnsPointer;
};

#endif // CWCREATECOMMAND_H

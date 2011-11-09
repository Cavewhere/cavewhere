#ifndef CWINTERACTION_H
#define CWINTERACTION_H

#include <QDeclarativeItem>

/**
    This is the base class for all interaction

    The interaction hold a pointer to a interaction manager.
  */
class cwInteraction : public QDeclarativeItem
{
    Q_OBJECT

public:
    explicit cwInteraction(QDeclarativeItem *parent = 0);

    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();

signals:
    void activated(cwInteraction* interaction);
    void deactivated(cwInteraction* interaction);

public slots:


private:

};

Q_DECLARE_METATYPE(cwInteraction*)


#endif // CWINTERACTION_H

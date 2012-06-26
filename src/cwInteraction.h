#ifndef CWINTERACTION_H
#define CWINTERACTION_H

#include <QQuickItem>

/**
    This is the base class for all interaction

    The interaction hold a pointer to a interaction manager.
  */
class cwInteraction : public QQuickItem
{
    Q_OBJECT

public:
    explicit cwInteraction(QQuickItem *parent = 0);

public slots:
    void activate();
    void deactivate();

signals:
    void activated(cwInteraction* interaction);
    void deactivated(cwInteraction* interaction);

public slots:


private:

};

Q_DECLARE_METATYPE(cwInteraction*)


#endif // CWINTERACTION_H

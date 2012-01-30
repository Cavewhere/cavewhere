#ifndef CWTASKPROGRESSDIALOG_H
#define CWTASKPROGRESSDIALOG_H

//Our includes
#include "ui_cwTaskProgressDialog.h"
class cwTask;


class cwTaskProgressDialog : public QDialog, private Ui::cwTaskProgressDialog
{
    Q_OBJECT

public:
    explicit cwTaskProgressDialog(QWidget *parent = 0);
    void setTask(cwTask* task);

protected:
    void changeEvent(QEvent *e);

protected slots:
    void started();
    void stopped();
    void finished();

    void stopTask();

private:
    cwTask* Task;

};

#endif // CWTASKPROGRESSDIALOG_H

//Our includes
#include "cwTaskProgressDialog.h"
#include "cwTask.h"

cwTaskProgressDialog::cwTaskProgressDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);
}

void cwTaskProgressDialog::setTask(cwTask* task) {
    Task = task;

    ProgressBar->setValue(0);
    ProgressBar->setMaximum(task->numberOfSteps());
    StatusLabel->clear();

    connect(StopButton, SIGNAL(clicked()), SLOT(stopTask()));

    connect(task, SIGNAL(numberOfStepsChanged(int)), ProgressBar, SLOT(setMaximum(int)));
    connect(task, SIGNAL(progressed(int)), ProgressBar, SLOT(setValue(int)));
    connect(task, SIGNAL(statusMessage(QString)), StatusLabel, SLOT(setText(QString)));

    connect(task, SIGNAL(started()), SLOT(started()));
    connect(task, SIGNAL(stopped()), SLOT(stopped()));
    connect(task, SIGNAL(finished()), SLOT(finished()));


}

void cwTaskProgressDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

void cwTaskProgressDialog::started() {
    ProgressBar->setValue(0);
}

void cwTaskProgressDialog::stopped() {
    StatusLabel->setText("Stopped");
    close();
}

void cwTaskProgressDialog::finished() {
    StatusLabel->setText("Finished!");
    close();
}

void cwTaskProgressDialog::stopTask() {
    StatusLabel->setText("Stopping");
    Task->stop();
}

//Our includes
#include "cwFileDialogHelper.h"

//Qt includes
#include <QFileDialog>

cwFileDialogHelper::cwFileDialogHelper(QObject *parent) :
    QObject(parent)
{
}

/**
  \brief Open's a new file dialog
  */
void cwFileDialogHelper::open() {
    QFileDialog* dialog = new QFileDialog();
    dialog->setFileMode(QFileDialog::ExistingFiles);
    connect(dialog, SIGNAL(filesSelected(QStringList)), this, SIGNAL(filesSelected(QStringList)));
    dialog->show(); //dialog->open(this, SIGNAL(filesSelected(QStringList)));
}

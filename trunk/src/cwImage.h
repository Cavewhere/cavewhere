#ifndef CWIMAGE_H
#define CWIMAGE_H

//Qt includes
#include <QList>

/**
  \brief This class stores id's to all the images in the database


  It stores the image path to original in the database,
  all the mipmap levels, and a icon version that's less than 512 by 512 pixels
  */
class cwImage {
public:
    cwImage();

    void setMipmaps(QList<int> mipmaps);
    QList<int> mipmaps();

    void setIcon(int iconId);
    void icon();

    void setOriginal(int id);
    void original();

private:
    QList<int> Mipmaps;
    int IconId;
    int OriginalId;

};

#endif // CWIMAGE_H

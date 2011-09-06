#ifndef CWNOTETRANFORMATION_H
#define CWNOTETRANFORMATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>

/**
  This class is useful for allowing automatic station referencing.  This holds the page of note's
  scale (example 40:1 and rotation that rotates the page such that north is up, ie. parallel the y axis.
  */
class cwNoteTranformation
{
public:
    cwNoteTranformation();

    double scale() const;
    void setScale(double scale);

    double northUp() const;
    void setNorthUp(double degrees);

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() {
            Scale = 0.0;
            North = 0.0;
        }

        double Scale;
        double North;

    };

    QSharedDataPointer<PrivateData> Data;
};

/**
  \brief Sets the scale of the notes

  This should be 500:1 where 1 unit on the page of notes equals to 500 units in cave.

  For example, 1cm on the page of notes equals to 5m in the cave.
  */
inline double cwNoteTranformation::scale() const {
    return Data->Scale;
}

/**
  \brief Sets the scale of the notes
  */
inline void cwNoteTranformation::setScale(double scale) {
    Data->Scale = scale;
}

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
inline double cwNoteTranformation::northUp() const {
    return Data->North;
}

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
inline void cwNoteTranformation::setNorthUp(double degrees) {
    Data->North = degrees;
}

#endif // CWNOTETRANFORMATION_H

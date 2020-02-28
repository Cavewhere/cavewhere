//Catch includes
#include <catch.hpp>

//Our includes
#include "cwImage.h"

TEST_CASE("cwImages isValid methods should return correctly", "[cwImage]") {
    cwImage image;

    SECTION("Original") {
        image.setOriginal(3);
        CHECK(image.isValid() == false);

        image.setOriginal(0);
        CHECK(image.isValid() == false);

        image.setOriginal(-1);
        CHECK(image.isValid() == false);

        image.setOriginal(-10);
        CHECK(image.isValid() == false);
        CHECK(image.isIconValid() == false);
        CHECK(image.isMipmapsValid() == false);
    }

    SECTION("Icon") {
        image.setIcon(3);
        CHECK(image.isIconValid() == true);

        image.setIcon(0);
        CHECK(image.isIconValid() == true);

        image.setIcon(-1);
        CHECK(image.isIconValid() == false);

        image.setIcon(-10);
        CHECK(image.isIconValid() == false);
        CHECK(image.isValid() == false);
        CHECK(image.isMipmapsValid() == false);
    }

    SECTION("Mipmaps") {
        image.setMipmaps({});
        CHECK(image.isMipmapsValid() == false);

        image.setMipmaps({2, 3, 6, 7});
        CHECK(image.isMipmapsValid() == true);

        image.setMipmaps({2, 0, 6, 7});
        CHECK(image.isMipmapsValid() == true);

        image.setMipmaps({2, 0, 6, -1, 7});
        CHECK(image.isMipmapsValid() == false);

        image.setMipmaps({-1, 0, 6, 7});
        CHECK(image.isMipmapsValid() == false);

        image.setMipmaps({2, 0, 6, -1});
        CHECK(image.isMipmapsValid() == false);
        CHECK(image.isIconValid() == false);
        CHECK(image.isValid() == false);
    }

}

TEST_CASE("cwImage ==operator should work correctly", "[cwImage]") {

    cwImage image;
    image.setOriginal(1);
    image.setMipmaps({2, 3, 4});
    image.setIcon(5);
    image.setOriginalSize(QSize(50, 51));
    image.setOriginalDotsPerMeter(1000);

    cwImage image2 = image;
    CHECK(image == image2);

    cwImage image3 = image;

    SECTION("Set icon") {
        image.setIcon(6);
        CHECK(image != image3);
    }
    SECTION("Set original") {
        image.setOriginal(-1);
        CHECK(image != image3);
    }
    SECTION("Set Mips") {
        image.setMipmaps({72});
        CHECK(image != image3);
    }
    SECTION("Set icon") {
        image.setOriginalSize(QSize(54, 30));
        CHECK(image != image3);
    }
    SECTION("Set icon") {
        image.setOriginalDotsPerMeter(200);
        CHECK(image != image3);
    }
}

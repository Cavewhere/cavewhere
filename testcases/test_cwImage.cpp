//Catch includes
#include <catch.hpp>

//Our includes
#include "cwImage.h"

TEST_CASE("cwImages isValid methods should return correctly", "[cwImage]") {
    cwImage image;

    SECTION("Original") {
        image.setOriginal(3);
        CHECK(image.isValid() == true);

        image.setOriginal(0);
        CHECK(image.isValid() == true);

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

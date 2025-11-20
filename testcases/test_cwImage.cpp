//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwImage.h"

TEST_CASE("cwImages isValid methods should return correctly", "[cwImage]") {
    cwImage image;

    SECTION("Original") {
        image.setOriginal(3);
        image.setMipmaps({2});
        image.setIcon(4);
        CHECK(image.isOriginalValid() == true);
        CHECK(image.mode() == cwImage::Mode::Ids);

        image.setOriginal(0);
        CHECK(image.isOriginalValid() == true);
        CHECK(image.mode() == cwImage::Mode::Ids);

        image.setOriginal(-1);
        CHECK(image.isOriginalValid() == false);
        CHECK(image.mode() == cwImage::Mode::Ids);

        image.setOriginal(-10);
        CHECK(image.isOriginalValid() == false);
        CHECK(image.isIconValid() == true);
        CHECK(image.isMipmapsValid() == true);
        CHECK(image.mode() == cwImage::Mode::Ids);
    }

    SECTION("Icon") {
        image.setIcon(3);
        CHECK(image.isIconValid() == true);
        CHECK(image.mode() == cwImage::Mode::Ids);

        image.setIcon(0);
        CHECK(image.isIconValid() == true);
        CHECK(image.mode() == cwImage::Mode::Ids);

        image.setIcon(-1);
        CHECK(image.isIconValid() == false);

        image.setIcon(-10);
        CHECK(image.isIconValid() == false);
        CHECK(image.isOriginalValid() == false);
        CHECK(image.isMipmapsValid() == false);
        CHECK(image.mode() == cwImage::Mode::Ids);
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
        CHECK(image.isOriginalValid() == false);
        CHECK(image.mode() == cwImage::Mode::Ids);
    }

    SECTION("Path-based image mode") {
        cwImage pathImage;
        CHECK(pathImage.mode() == cwImage::Mode::Invalid);

        pathImage.setPath("some/file.png");
        CHECK(pathImage.mode() == cwImage::Mode::Path);
        CHECK(pathImage.path() == "some/file.png");

        SECTION("copy the path") {
            auto copy = pathImage;
            copy.setPath("some/file2.png");

            CHECK(pathImage.mode() == cwImage::Mode::Path);
            CHECK(pathImage.path() == "some/file.png");

            CHECK(copy.mode() == cwImage::Mode::Path);
            CHECK(copy.path() == "some/file2.png");

        }
    }
}


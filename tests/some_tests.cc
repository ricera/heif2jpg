#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <ultrahdr_api.h>

#include "../app/heif2jpg.h"
// #include "../app/heif2jpg_savefile.h"

/* Test static functions */
#include "../app/heif2jpg_savefile.cc"

TEST_GROUP(Main)
{

};

TEST(Main, derive_output_filename_basic)
{
    std::string input = "test.heif";
    std::string suffix = "jpg";

    std::string output = "test.jpg";

    STRCMP_EQUAL(output.c_str(), derive_output_filename(input, suffix).c_str());
}

TEST(Main, derive_output_filename_folders)
{
    std::string input = "test/test.heif";
    std::string suffix = "jpg";

    std::string output = "test/test.jpg";

    STRCMP_EQUAL(output.c_str(), derive_output_filename(input, suffix).c_str());
}

TEST(Main, verify_uhdr_image_encode_setup)
{
    uhdr_raw_image_t test_raw_uhdr_image = {};
    struct heif2jpg_encode_options encode_options = {};
    size_t test_yh = 32;
    size_t test_yw = 64;

    init_uhdr_raw_image(test_raw_uhdr_image, encode_options, test_yw, test_yh);

    LONGS_EQUAL(test_yh, test_raw_uhdr_image.h);
    LONGS_EQUAL(test_yw, test_raw_uhdr_image.w);

    CHECK_FALSE(test_raw_uhdr_image.planes[UHDR_PLANE_Y] == nullptr);
    CHECK_FALSE(test_raw_uhdr_image.planes[UHDR_PLANE_UV] == nullptr);
    CHECK_TRUE(test_raw_uhdr_image.planes[UHDR_PLANE_V] == nullptr);

    free(test_raw_uhdr_image.planes[UHDR_PLANE_Y]);
    free(test_raw_uhdr_image.planes[UHDR_PLANE_UV]);
}

TEST(Main, null_heif_image_handle_pointer_failure)
{
    struct heif2jpg_encode_options encode_options = {};
    int ret = 0;

    ret = save_uhdr_jpg_file(nullptr, nullptr, encode_options, "test.jpg");

    LONGS_EQUAL(EINVAL, ret);
}
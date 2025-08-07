#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <ultrahdr_api.h>

#include "../app/heif2jpg.h"

/* Test static functions */
#include "../app/encode_uhdr.cc"

TEST_GROUP(Main)
{

};

TEST_GROUP(EncodeUhdr)
{
    struct heif_image *test_image;
    struct heif2jpg_encode_options encode_options = {};
    struct heif_error herror;

    TEST_SETUP()
    {

    }

    void init_420_image(uint8_t bit_depth)
    {
        herror = heif_image_create(32, 32,
            heif_colorspace_YCbCr,
            heif_chroma_420, &test_image);
        LONGS_EQUAL(0, herror.code);

        herror = heif_image_add_plane(test_image, heif_channel_Y, 32, 32, bit_depth);
        LONGS_EQUAL(0, herror.code);
        herror = heif_image_add_plane(test_image, heif_channel_Cb, 16, 16, bit_depth);
        LONGS_EQUAL(0, herror.code);
        herror = heif_image_add_plane(test_image, heif_channel_Cr, 16, 16, bit_depth);
        LONGS_EQUAL(0, herror.code);
    }

    void init_420_8bit_image()
    {
        init_420_image(8);
    }

    TEST_TEARDOWN()
    {
        // heif_image_create() called
        if (test_image) {
            heif_image_release(test_image);
            test_image = nullptr;
        }
    }
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

TEST(EncodeUhdr, private_verify_uhdr_image_encode_setup)
{
    uhdr_raw_image_t test_raw_uhdr_image = {};
    struct heif2jpg_encode_options encode_options = {};
    const size_t test_yh = 32;
    const size_t test_yw = 64;

    init_uhdr_raw_image(test_raw_uhdr_image, encode_options, test_yw, test_yh);

    LONGS_EQUAL(test_yh, test_raw_uhdr_image.h);
    LONGS_EQUAL(test_yw, test_raw_uhdr_image.w);

    CHECK_FALSE(test_raw_uhdr_image.planes[UHDR_PLANE_Y] == nullptr);
    CHECK_FALSE(test_raw_uhdr_image.planes[UHDR_PLANE_UV] == nullptr);
    CHECK_TRUE(test_raw_uhdr_image.planes[UHDR_PLANE_V] == nullptr);

    LONGS_EQUAL(test_yw, test_raw_uhdr_image.stride[UHDR_PLANE_Y]);
    LONGS_EQUAL(test_yw, test_raw_uhdr_image.stride[UHDR_PLANE_UV]);
    LONGS_EQUAL(0, test_raw_uhdr_image.stride[UHDR_PLANE_V]);

    free(test_raw_uhdr_image.planes[UHDR_PLANE_Y]);
    free(test_raw_uhdr_image.planes[UHDR_PLANE_UV]);
}

TEST(EncodeUhdr, public_null_heif_image_handle_pointer_failure)
{
    struct heif2jpg_encode_options encode_options = {};
    int ret = 0;

    ret = save_uhdr_jpg_file(nullptr, encode_options, "test.jpg");

    LONGS_EQUAL(EINVAL, ret);
}

TEST(EncodeUhdr, public_fail_on_8bpp_input_image)
{
    int ret = 0;

    init_420_8bit_image();

    ret = save_uhdr_jpg_file(test_image, encode_options, "test.jpg");
    LONGS_EQUAL(ENOSYS, ret);
}

TEST(EncodeUhdr, bad_output_file_failure)
{
    int ret = 0;

    init_420_image(10);

    LONGS_EQUAL(0, herror.code);

    ret = save_uhdr_jpg_file(test_image, encode_options, "");
    LONGS_EQUAL(EINVAL, ret);
}
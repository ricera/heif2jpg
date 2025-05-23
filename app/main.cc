// SPDX: BSD 2-Clause License
/**
 * Application for reading in an HEIF file
 *
 *   Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *   Copyright (c) 2025 Eric Joyner <erj@erj.cc>
 */

#include <Windows.h>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <libheif/heif.h>
#include <libheif/heif_image.h>

#include <ultrahdr_api.h>

/* Progress functions obtained from libheif's examples/heif_dec.cc */
static int max_value_progress = 0;

void start_progress(enum heif_progress_step step, int max_progress,
                    void *progress_user_data)
{
    max_value_progress = max_progress;
}

void on_progress(enum heif_progress_step step, int progress,
                 void *progress_user_data)
{
    std::cout << "decoding image... " << progress * 100 / max_value_progress << "%\r";
    std::cout.flush();
}

void end_progress(enum heif_progress_step step, void *progress_user_data)
{
    std::cout << std::endl;
}

/* Obtained from libheif's examples/heif_dec.cc */
class LibHeifInitializer
{
public:
    LibHeifInitializer() { heif_init(nullptr); }
    ~LibHeifInitializer() { heif_deinit(); }
};

/* Obtained from libheif's examples/heif_dec.cc */
class ContextReleaser
{
public:
    ContextReleaser(struct heif_context *ctx) : ctx_(ctx) {}

    ~ContextReleaser()
    {
        heif_context_free(ctx_);
    }

private:
    struct heif_context *ctx_;
};

std::string derive_output_filename(const std::string &input_filename,
                                   const std::string &suffix)
{
    std::string input_stem;
    size_t dot_pos = input_filename.rfind('.');

    if (dot_pos != std::string::npos)
        input_stem = input_filename.substr(0, dot_pos);
    else
        input_stem = input_filename;

    return std::string(input_stem + "." + suffix);
}

int save_p010_file(struct heif_image_handle *handle, heif_image *image,
                   std::string output_filename)
{
    // todo: convert to C++ style
    FILE *fp = fopen(output_filename.c_str(), "wb");
    if (!fp)
    {
        std::cerr << "Can't open " << output_filename << ": "
                  << strerror(errno) << std::endl;
        return 9;
    }

    int y_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
    int cb_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cb);
    int cr_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cr);

    int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);
    std::cout << "Input luma bit depth: " << bit_depth << std::endl;
    printf("Encoding image with Y=%d, Cb=%d, Cr=%d bits per pixel\n", y_bpp, cb_bpp, cr_bpp);

    size_t y_stride, cb_stride, cr_stride;
    const uint8_t *yp = heif_image_get_plane_readonly2(image, heif_channel_Y, &y_stride);
    const uint8_t *cbp = heif_image_get_plane_readonly2(image, heif_channel_Cb, &cb_stride);
    const uint8_t *crp = heif_image_get_plane_readonly2(image, heif_channel_Cr, &cr_stride);

    assert(y_stride > 0);
    assert(cb_stride > 0);
    assert(cr_stride > 0);

    int yw = heif_image_get_width(image, heif_channel_Y);
    int yh = heif_image_get_height(image, heif_channel_Y);
    int cw = heif_image_get_width(image, heif_channel_Cb);
    int ch = heif_image_get_height(image, heif_channel_Cb);

    if (yw < 0 || cw < 0)
    {
        fclose(fp);
        std::cerr << "Invalid Y or C plane width in decoded image." << std::endl;
        return 10;
    }

    /* If 10-bit image output, use P010 format as output */
    if (y_bpp == 10)
    {
        printf("Output in P010 YUV format\n");

        const uint16_t *yp_16 = (const uint16_t *)yp;
        const uint16_t *cbp_16 = (const uint16_t *)cbp;
        const uint16_t *crp_16 = (const uint16_t *)crp;

        /* In P010, values are encoded in the 10 most significant bits, so the decoded plane cannot
         * be written out to the output file as-is, unlike in 8-bit YUV420 below
         */
        for (int y = 0; y < yh; y++)
        {
            for (int z = 0; z < yw; z++)
            {
                uint16_t word = *(yp_16 + z + (y * yw));
                word = (word << 6); // Little Endian
                fwrite(&word, 2, 1, fp);
            }
        }

        /* The U and V planes are interleaved in P010;
         * U == Cb, and V == Cr
         */
        for (int y = 0; y < ch; y++)
        {
            for (int z = 0; z < cw; z++)
            {
                uint16_t word = *(cbp_16 + z + (y * cw));
                word = (word << 6); // Little Endian
                fwrite(&word, 2, 1, fp);

                word = *(crp_16 + z + (y * cw));
                word = (word << 6); // Little Endian
                fwrite(&word, 2, 1, fp);
            }
        }
    }
    else
    {
        /* Encode as 8-bit image in C420 */
        printf("Output in C420 YUV format\n");

        for (int y = 0; y < yh; y++)
        {
            fwrite(yp + y * y_stride, 1, yw, fp);
        }

        for (int y = 0; y < ch; y++)
        {
            fwrite(cbp + y * cb_stride, 1, cw, fp);
        }

        for (int y = 0; y < ch; y++)
        {
            fwrite(crp + y * cr_stride, 1, cw, fp);
        }
    }

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    /* Automatically inits and deinits the library in main() scope */
    LibHeifInitializer initializer;
    struct heif_error err;
    int ret;

    if (argc < 2)
    {
        std::cerr << "A filename is required." << std::endl;
        return 1;
    }

    std::string input_filename(argv[1]);
    std::string output_filename = derive_output_filename(input_filename, "p010");

    std::cout << "Output file: " << output_filename << std::endl;

    /* Check for valid file */
    // Can it be opened?
    std::ifstream istr(input_filename.c_str(), std::ios_base::binary);
    if (istr.fail())
    {
        std::cerr << "Input file doesn't exist!" << std::endl;
        return 2;
    }
    // Does it have a valid box length?
    // Does it have a compatible heif filetype? e.g. heic

    /* Read the file */
    heif_context *ctx = heif_context_alloc();
    if (!ctx)
    {
        std::cerr << "HEIF context allocation failed." << std::endl;
        return 3;
    }

    ContextReleaser cr(ctx);

    err = heif_context_read_from_file(ctx, input_filename.c_str(), nullptr);
    if (err.code != 0)
    {
        std::cerr << "Could not read HEIF/AVIF file: " << err.message << std::endl;
        return 4;
    }

    int num_images = heif_context_get_number_of_top_level_images(ctx);
    if (num_images == 0)
    {
        std::cerr << "File doesn't contain any images!" << std::endl;
        return 5;
    }
    else if (num_images != 1)
    {
        std::cerr << "No support for more than 1 image." << std::endl;
        return 6;
    }

    struct heif_image_handle *handle;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code)
    {
        std::cerr << "Could not read HEIF image: " << err.message << std::endl;
        return 7;
    }

    // This is a spectacularly odd construction -- from libheif's heif_dec.cc
    std::unique_ptr<heif_decoding_options, void (*)(heif_decoding_options *)>
        decode_options(heif_decoding_options_alloc(), heif_decoding_options_free);
    decode_options->strict_decoding = false;
    decode_options->decoder_id = nullptr;

    decode_options->start_progress = start_progress;
    decode_options->on_progress = on_progress;
    decode_options->end_progress = end_progress;

    // This currently only is supposed to work on Nikon HEIF images, so the chroma is hardcoded to 4:2:0
    // This is also only supposed to go out to libultrahdr to make a jpg via P010 data, so we want YUV format planes
    heif_image *img;
    err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_420, decode_options.get());
    if (err.code)
    {
        std::cerr << "Could not decode HEIF image: " << err.message << std::endl;
        return 8;
    }

    /* Write out raw P010 file */
    ret = save_p010_file(handle, img, output_filename);
    if (ret)
        return ret;

    uhdr_color_gamut_t hdr_cg;
    uhdr_create_encoder();

    /* Done */
    std::cout << "Success!" << std::endl;

    return 0;
}
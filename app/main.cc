// SPDX-License-Identifier: BSD-2-Clause
/**
 * Application for reading in an HEIF file and converting it to an ultrahdr jpg
 *
 *   Copyright (c) 2025 Eric Joyner <erj@erj.cc>
 */

#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <argparse/argparse.hpp>

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

struct heif2jpg_encode_options {
    uhdr_color_gamut_t color_gamut;
    uhdr_color_range_t color_range;
    uhdr_color_transfer_t color_transfer;
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

int save_uhdr_jpg_file(struct heif_image_handle *handle,
    heif_image *image,
    struct heif2jpg_encode_options encode_options,
    std::string output_filename)
{
    uhdr_error_info_t status;
    int ret;

    std::ofstream fp(output_filename, std::ios::out | std::ios::binary);
    if (!fp.good())
    {
        std::cerr << "Can't open " << output_filename << ": "
                  << strerror(errno) << std::endl;
        return 9;
    }
    
    /* Get HEIF image parameters, arrays, pointers */
    int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);
    std::cout << "Input luma bit depth: " << bit_depth << std::endl;
    
    int y_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
    int cb_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cb);
    int cr_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cr);

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
        fp.close();
        std::cerr << "Invalid Y or C plane width in decoded image." << std::endl;
        return 10;
    }

    /* Setup libultrahdr structures for output */
    uhdr_raw_image_t raw_uhdr_image{};

    raw_uhdr_image.fmt = UHDR_IMG_FMT_24bppYCbCrP010;
    raw_uhdr_image.range = encode_options.color_range;
    raw_uhdr_image.cg = encode_options.color_gamut;
    raw_uhdr_image.ct = encode_options.color_transfer;
    raw_uhdr_image.w = yw;
    raw_uhdr_image.h = yh;

    raw_uhdr_image.planes[UHDR_PLANE_Y] = malloc(2 * yw * yh);
    raw_uhdr_image.planes[UHDR_PLANE_UV] = malloc(2 * (yw/2) * (yh/2) * 2);
    raw_uhdr_image.planes[UHDR_PLANE_V] = nullptr;
    raw_uhdr_image.stride[UHDR_PLANE_Y] = yw;
    raw_uhdr_image.stride[UHDR_PLANE_UV] = yw;
    raw_uhdr_image.stride[UHDR_PLANE_V] = 0;

    /* If 10-bit image output, encode in memory in P010 format for input into libultrahdr */
    if (y_bpp == 10)
    {
        std::cout << "Encoding image in P010 format in memory" << std::endl;

        const uint16_t *yp_16 = (const uint16_t *)yp;
        const uint16_t *cbp_16 = (const uint16_t *)cbp;
        const uint16_t *crp_16 = (const uint16_t *)crp;

        size_t word_pos = 0;

        /* In P010, values are encoded in the 10 most significant bits. */
        for (int y = 0; y < yh; y++)
        {
            for (int z = 0; z < yw; z++)
            {
                uint16_t word = *(yp_16 + z + (y * yw));
                word = (word << 6); // Little Endian
                
                ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_Y]))[word_pos] = word;
                word_pos++;
            }
        }

        /* The U and V planes are interleaved in P010;
         * U == Cb, and V == Cr
         */

        word_pos = 0;
        for (int y = 0; y < ch; y++)
        {
            for (int z = 0; z < cw; z++)
            {
                uint16_t word = *(cbp_16 + z + (y * cw));
                word = (word << 6); // Little Endian

                ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_UV]))[word_pos] = word;
                word_pos++;

                word = *(crp_16 + z + (y * cw));
                word = (word << 6); // Little Endian

                ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_UV]))[word_pos] = word;
                word_pos++;
            }
        }

        /* Raw image memory is set; setup encoder */
        uhdr_codec_private_t* handle = uhdr_create_encoder();
        status = uhdr_enc_set_raw_image(handle, &raw_uhdr_image, UHDR_HDR_IMG);
        if (status.error_code != UHDR_CODEC_OK) {
            if (status.has_detail) {
                std::cerr << "UHDR encoder: " << status.detail << std::endl;
            }
            uhdr_release_encoder(handle);
            return 11;
        }

        uhdr_enc_set_quality(handle, 95, UHDR_BASE_IMG);
        uhdr_enc_set_quality(handle, 95, UHDR_GAIN_MAP_IMG);
        uhdr_enc_set_using_multi_channel_gainmap(handle, false);
        uhdr_enc_set_gainmap_scale_factor(handle, 1);
        uhdr_enc_set_gainmap_gamma(handle, 1.0f);
        uhdr_enc_set_preset(handle, UHDR_USAGE_BEST_QUALITY);

        status = uhdr_encode(handle);
        if (status.error_code != UHDR_CODEC_OK) {
            if (status.has_detail) {
                std::cerr << "UHDR encoder: " << status.detail << std::endl;
            }
            uhdr_release_encoder(handle);
            return 12;
        }

        auto encoded_output = uhdr_get_encoded_stream(handle);
        uhdr_compressed_image_t output_image{};
    
        output_image.data = malloc(encoded_output->data_sz);
        memcpy(output_image.data, encoded_output->data, encoded_output->data_sz);
        output_image.capacity = output_image.data_sz = encoded_output->data_sz;
#if 0
        output_image.cg = encoded_output->cg;
        output_image.ct = encoded_output->ct;
        output_image.range = encoded_output->range;
#endif

        uhdr_release_encoder(handle);

        if (fp.is_open()) {
            fp.write(static_cast<char*>(output_image.data), output_image.data_sz);
        } else {
            std::cerr << "Unable to write to file after encoding: " << output_filename << std::endl;
            return 13;
        }

    } else {
        std::cerr << "8-bit input not supported yet." << std::endl;
        return 10;
    }

    return 0;
}

int save_p010_file(struct heif_image_handle *handle, heif_image *image,
                   std::string output_filename)
{
    std::ofstream fp(output_filename, std::ios::out | std::ios::binary);
    if (!fp.good())
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
        fp.close();
        std::cerr << "Invalid Y or C plane width in decoded image." << std::endl;
        return 10;
    }

    /* If 10-bit image output, use P010 format as output */
    if (y_bpp == 10)
    {
        std::cout << "Output in P010 YUV format" << std::endl;

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
                fp.write((char *)&word, 2);
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
                // fwrite(&word, 2, 1, fp);
                fp.write((char *)&word, 2);

                word = *(crp_16 + z + (y * cw));
                word = (word << 6); // Little Endian
                fp.write((char *)&word, 2);
            }
        }
    }
    else
    {
        std::cerr << "8-bit input not supported yet." << std::endl;
        return 10;
    }

    fp.close();
    return 0;
}

int main(int argc, char **argv)
{
    /* Automatically inits and deinits the library in main() scope */
    LibHeifInitializer initializer;
    struct heif_error err;
    int ret;

    /* Use imported argparser library to handle input arguments */
    argparse::ArgumentParser argparser("heif2jpg");
    argparser.add_argument("input_file")
        .help("File path to HEIF file to convert");
    argparser.add_argument("output_file")
        .default_value(std::string("-"))
        .help("File path to JPG file to write to");
    argparser.add_argument("-p")
        .default_value(false)
        .help("Output a p010 encoded raw image instead of a jpeg")
        .flag();
    argparser.add_argument("-c")
        .default_value(2)
        .help("Input color gamut: 0 = BT709, 1 = Display P3, 2 = BT2100")
        .scan<'i', int>();
    argparser.add_argument("-r")
        .default_value(1)
        .help("Input color range: 0 = limited, 1 = full")
        .scan<'i', int>();
    argparser.add_argument("-t")
        .default_value(1)
        .help("Input color transfer function: 0 = Linear, 1 = HLG, 2 = PQ, 3 = SRGB")
        .scan<'i', int>();

    try {
        argparser.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << argparser;
        return 1;
    }

    std::string input_filename = argparser.get<std::string>("input_file");
    std::string output_filename = argparser.get<std::string>("output_file");

    bool output_p010 = argparser.get<bool>("-p");
    if (output_filename.starts_with("-")) {
        if (output_p010)
            output_filename = derive_output_filename(input_filename, "p010");
        else
            output_filename = derive_output_filename(input_filename, "uhdr.jpg");
    }
    std::cout << "Output file path: " << output_filename << std::endl;

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
        std::cerr << "libheif: HEIF context allocation failed." << std::endl;
        return 3;
    }

    ContextReleaser cr(ctx);

    err = heif_context_read_from_file(ctx, input_filename.c_str(), nullptr);
    if (err.code != 0)
    {
        std::cerr << "libheif: Could not read HEIF/AVIF file: " <<
            err.message << std::endl;
        return 4;
    }

    int num_images = heif_context_get_number_of_top_level_images(ctx);
    if (num_images == 0)
    {
        std::cerr << "libheif: File doesn't contain any images!" << std::endl;
        return 5;
    }
    else if (num_images != 1)
    {
        std::cerr << "libheif: No support for more than 1 image." << std::endl;
        return 6;
    }

    struct heif_image_handle *handle;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code)
    {
        std::cerr << "libheif: Could not read HEIF image: " << err.message << std::endl;
        return 7;
    }

    // This is a spectacularly odd construction -- from libheif's heif_dec.cc
    std::unique_ptr<heif_decoding_options, void (*)(heif_decoding_options *)>
        decode_options(heif_decoding_options_alloc(), heif_decoding_options_free);
    decode_options->strict_decoding = false;
    decode_options->decoder_id = nullptr;
    decode_options->convert_hdr_to_8bit = false;

    decode_options->start_progress = start_progress;
    decode_options->on_progress = on_progress;
    decode_options->end_progress = end_progress;

    // This currently only is supposed to work on Nikon HEIF images, so the chroma is hardcoded to 4:2:0
    // This is also only supposed to go out to libultrahdr to make a jpg via P010 data, so we want YUV format planes
    heif_image *img;
    err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_420, decode_options.get());
    if (err.code)
    {
        std::cerr << "llibheif: Could not decode HEIF image: " << err.message << std::endl;
        return 8;
    }

    /* Determine output file format */
    if (output_p010) {
        ret = save_p010_file(handle, img, output_filename);
        if (ret)
            return ret;
    } else {
        struct heif2jpg_encode_options encode_options;
        encode_options.color_gamut = (uhdr_color_gamut_t)argparser.get<int>("-c");
        encode_options.color_range = (uhdr_color_range_t)argparser.get<int>("-r");
        encode_options.color_transfer =
            (uhdr_color_transfer_t)argparser.get<int>("-t");

        ret = save_uhdr_jpg_file(handle, img, encode_options, output_filename);
        if (ret)
            return ret;
    }

    /* Done */
    std::cout << "Success!" << std::endl;

    return 0;
}
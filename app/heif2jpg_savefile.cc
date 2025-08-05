#include "heif2jpg_savefile.h"

/**
 * Setup libultrahdr structures for output
 */
static void init_uhdr_raw_image(uhdr_raw_image_t &raw_uhdr_image,
    struct heif2jpg_encode_options &encode_options,
    size_t yw, size_t yh)
{
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
}

int save_uhdr_jpg_file(struct heif_image_handle *handle,
    struct heif_image *image,
    struct heif2jpg_encode_options encode_options,
    std::string output_filename)
{
    uhdr_error_info_t status;
    int ret;

    if (!handle) {
        std::cerr << "Invalid pointer for HEIF image handle" << std::endl;
        return EINVAL;
    }

    std::ofstream fp(output_filename, std::ios::out | std::ios::binary);
    if (!fp.good())
    {
        std::cerr << "Can't open " << output_filename << ": "
                  << strerror(errno) << std::endl;
        return errno;
    }
    
    /* Get HEIF image parameters, arrays, pointers */  
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

    uhdr_raw_image_t raw_uhdr_image;
    init_uhdr_raw_image(raw_uhdr_image, encode_options, yw, yh);

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
                uint16_t word = *(yp_16 + z + (y * (y_stride / 2)));
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
                uint16_t word = *(cbp_16 + z + (y * (cb_stride / 2)));
                word = (word << 6); // Little Endian

                ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_UV]))[word_pos] = word;
                word_pos++;

                word = *(crp_16 + z + (y * (cr_stride / 2)));
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

        if (encode_options.new_width > 0) {
            float scale_factor = (float)encode_options.new_width / yw;
            uint16_t new_height = (uint16_t)std::round(yh * scale_factor);

            uhdr_add_effect_resize(handle, encode_options.new_width, new_height);
        }
 
        uhdr_enc_set_quality(handle, encode_options.quality, UHDR_BASE_IMG);
        uhdr_enc_set_quality(handle, encode_options.quality, UHDR_GAIN_MAP_IMG);
        uhdr_enc_set_using_multi_channel_gainmap(handle, false);
        uhdr_enc_set_gainmap_scale_factor(handle, 1);
        uhdr_enc_set_gainmap_gamma(handle, 1.0f);
        uhdr_enc_set_preset(handle, UHDR_USAGE_BEST_QUALITY);

        std::cout << "Encoding as ultra HDR jpeg..." << std::endl;

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
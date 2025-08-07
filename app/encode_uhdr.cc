#include "encode_uhdr.h"

class ResReleaser
{
public:
    ResReleaser(std::ofstream *fp) : fp_(fp) {}

    void set_uhdr_handle(uhdr_codec_private_t *handle)
    {
        handle_ = handle;
    }

    ~ResReleaser()
    {
        fp_->close();
        delete fp_;

        if (handle_ != nullptr)
            uhdr_release_encoder(handle_);
    }

private:
    std::ofstream *fp_ = nullptr;
    uhdr_codec_private_t *handle_ = nullptr;
};

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

static int open_output_file(std::string &filename, std::ofstream **fp)
{
    std::ofstream *new_fp;
    
    new_fp = new std::ofstream(filename, std::ios::out | std::ios::binary);
    if (!new_fp->good())
    {
        std::cerr << "Can't open " << filename << ": "
                  << strerror(errno) << std::endl;
        delete new_fp;
        return errno;
    }

    *fp = new_fp;
    return 0;
}

struct heif2jpg_image_params {
    /* Bits per pixel */
    int y_bpp;
    int cb_bpp;
    int cr_bpp;

    /* Pointers to pixel data */
    uint8_t const *yp;
    uint8_t const *cbp;
    uint8_t const *crp;

    /* Plane width and height */
    int yw;
    int yh;
    int cw; // c[w|h] applies to both Cb and Cr planes
    int ch;

    /* Pixel strides for decoded image */
    size_t y_stride;
    size_t cb_stride;
    size_t cr_stride;
};

static int get_image_parameters(struct heif_image *image,
    struct heif2jpg_image_params *params)
{
    size_t y_stride, cb_stride, cr_stride;
    int yw, cw;

    yw = heif_image_get_width(image, heif_channel_Y);
    cw = heif_image_get_width(image, heif_channel_Cb);

    if (yw < 0 || cw < 0) {
        std::cerr << "Invalid Y or C plane width in decoded image." << std::endl;
        return EINVAL;
    }
    
    params->yp = heif_image_get_plane_readonly2(image, heif_channel_Y, &y_stride);
    params->cbp = heif_image_get_plane_readonly2(image, heif_channel_Cb, &cb_stride);
    params->crp = heif_image_get_plane_readonly2(image, heif_channel_Cr, &cr_stride);

    if (y_stride <= 1 || cb_stride <= 1 || cr_stride <= 1) {
        std::cerr << "Invalid plane stride in decoded image:" << std::endl;
        std::cerr << "Y: " << y_stride << ", Cb: " << cb_stride <<
            ", Cr: " << cr_stride << std::endl;
        return EINVAL;
    }

    params->y_stride = y_stride;
    params->cb_stride = cb_stride;
    params->cr_stride = cr_stride;

    params->yw = yw;
    params->cw = cw;
    params->yh = heif_image_get_height(image, heif_channel_Y);
    params->ch = heif_image_get_height(image, heif_channel_Cb);
    /* Cr and Cb planes should have the same height and width */
     
    params->y_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
    params->cb_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cb);
    params->cr_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_Cr);

    return 0;
}

static void setup_uhdr_image_planes(uhdr_raw_image_t &raw_uhdr_image,
    struct heif2jpg_image_params &params)
{
    const uint16_t *yp_16 = (const uint16_t *)params.yp;
    const uint16_t *cbp_16 = (const uint16_t *)params.cbp;
    const uint16_t *crp_16 = (const uint16_t *)params.crp;

    size_t word_pos = 0;
    uint16_t word;

    /* In P010, values are encoded in the 10 most significant bits;
       the values are Little Endian
    */
    for (int y = 0; y < params.yh; y++)
    {
        for (int z = 0; z < params.yw; z++)
        {
            word = *(yp_16 + z + (y * (params.y_stride / 2)));
            word <<= 6;
            
            ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_Y]))[word_pos] = word;
            word_pos++;
        }
    }

    /* The U and V planes are interleaved in P010;
       U == Cb, and V == Cr
    */
    word_pos = 0;
    for (int y = 0; y < params.ch; y++)
    {
        for (int z = 0; z < params.cw; z++)
        {
            word = *(cbp_16 + z + (y * (params.cb_stride / 2)));
            word <<= 6;

            ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_UV]))[word_pos] = word;
            word_pos++;

            word = *(crp_16 + z + (y * (params.cr_stride / 2)));
            word <<= 6;

            ((uint16_t *)(raw_uhdr_image.planes[UHDR_PLANE_UV]))[word_pos] = word;
            word_pos++;
        }
    }
}

void set_uhdr_enc_params(uhdr_codec_private_t *uhdr_handle,
    struct heif2jpg_encode_options &encode_options,
    struct heif2jpg_image_params &params)
{
    /* Scale output image if a non-zero new width was specified */
    if (encode_options.new_width > 0) {
        float scale_factor = (float)encode_options.new_width / params.yw;
        uint16_t new_height = (uint16_t)std::round(params.yh * scale_factor);

        uhdr_add_effect_resize(uhdr_handle, encode_options.new_width, new_height);
    }

    uhdr_enc_set_quality(uhdr_handle, encode_options.quality, UHDR_BASE_IMG);
    uhdr_enc_set_quality(uhdr_handle, encode_options.quality, UHDR_GAIN_MAP_IMG);
    uhdr_enc_set_using_multi_channel_gainmap(uhdr_handle, false);
    uhdr_enc_set_gainmap_scale_factor(uhdr_handle, 1);
    uhdr_enc_set_gainmap_gamma(uhdr_handle, 1.0f);
    uhdr_enc_set_preset(uhdr_handle, UHDR_USAGE_BEST_QUALITY);
}

int save_uhdr_jpg_file(struct heif_image *image,
    struct heif2jpg_encode_options encode_options,
    std::string output_filename)
{
    uhdr_compressed_image_t *encoded_output;
    struct heif2jpg_image_params params;
    uhdr_codec_private_t* uhdr_handle;
    uhdr_raw_image_t raw_uhdr_image;
    uhdr_error_info_t status;
    std::ofstream *fp;
    int ret;

    if (!image) {
        std::cerr << "Invalid pointer to decoded heif image!" << std::endl;
        return EINVAL;
    }

    ret = get_image_parameters(image, &params);
    if (ret)
        return ret;

    if (params.y_bpp != 10) {
        std::cerr << "Non 10-bit input is not supported." << std::endl;
        return ENOSYS;
    }

    ret = open_output_file(output_filename, &fp);
    if (ret)
        return ret;
    ResReleaser res_releaser(fp);

    /* Setup P010 planes for input into UHDR encoder */
    init_uhdr_raw_image(raw_uhdr_image, encode_options, params.yw, params.yh);
    std::cout << "Encoding image in P010 format in memory..." << std::endl;
    setup_uhdr_image_planes(raw_uhdr_image, params);

    uhdr_handle = uhdr_create_encoder();
    if (!uhdr_handle) {
        std::cerr << "Error creating UHDR encoder!" << std::endl;
        return EIO;
    }
    res_releaser.set_uhdr_handle(uhdr_handle);

    status = uhdr_enc_set_raw_image(uhdr_handle, &raw_uhdr_image, UHDR_HDR_IMG);
    if (status.error_code != UHDR_CODEC_OK) {
        if (status.has_detail)
            std::cerr << "UHDR encoder: " << status.detail << std::endl;
        return EIO;
    }

    set_uhdr_enc_params(uhdr_handle, encode_options, params);

    std::cout << "Encoding as ultra HDR jpeg..." << std::endl;
    status = uhdr_encode(uhdr_handle);
    if (status.error_code != UHDR_CODEC_OK) {
        if (status.has_detail) {
            std::cerr << "UHDR encoder: " << status.detail << std::endl;
        }
        return EIO;
    }

    encoded_output = uhdr_get_encoded_stream(uhdr_handle);
    fp->write(static_cast<char*>(encoded_output->data), encoded_output->data_sz);

    return 0;
}
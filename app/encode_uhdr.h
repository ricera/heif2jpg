/* HEIF coversion ops */
#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include <libheif/heif.h>

#include <ultrahdr_api.h>

struct heif2jpg_encode_options {
    uhdr_color_gamut_t color_gamut;
    uhdr_color_range_t color_range;
    uhdr_color_transfer_t color_transfer;
    uint16_t new_width;
    uint8_t quality;
};

int save_uhdr_jpg_file(struct heif_image *image,
    struct heif2jpg_encode_options encode_options,
    std::string output_filename);
#include <cstring>

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

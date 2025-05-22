/**
 * Test application for building and checking includes
 */

#include <Windows.h>
#include <cstring>
#include <iostream>

#include <libheif/heif.h>

// Used from libheif's examples/heif_dec.cc
class LibHeifInitializer {
public:
    LibHeifInitializer() { heif_init(nullptr); }
    ~LibHeifInitializer() { heif_deinit(); }
};

int main(int argc, char** argv)
{
    // automatically inits and deinits the library in main() scope
    LibHeifInitializer initializer;

    heif_context* ctx = heif_context_alloc();

    if (ctx)
        printf("HEIF Context Allocated!\n");
    else
        printf("HEIF Context Allocation failed.\n");

    heif_context_free(ctx);

    return 0;
}
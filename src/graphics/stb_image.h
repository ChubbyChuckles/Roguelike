/* Placeholder minimal stb_image stub. Replace with full stb_image for standalone PNG loading if
 * SDL_image absent. */
#ifndef STB_IMAGE_H
#define STB_IMAGE_H

#include <stddef.h> /* for NULL */

typedef unsigned char stbi_uc;
static inline stbi_uc* stbi_load_from_memory(const stbi_uc* buffer, int len, int* x, int* y,
                                             int* comp, int req_comp)
{
    (void) buffer;
    (void) len;
    (void) x;
    (void) y;
    (void) comp;
    (void) req_comp;
    return NULL;
}
static inline void stbi_image_free(void* p) { (void) p; }

#endif

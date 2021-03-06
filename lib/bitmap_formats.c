/*
 * Copyright (c) Imazen LLC.
 * No part of this project, including this file, may be copied, modified,
 * propagated, or distributed except as permitted in COPYRIGHT.txt.
 * Licensed under the GNU Affero General Public License, Version 3.0.
 * Commercial licenses available at http://imageresizing.net/
 */
#ifdef _MSC_VER
#pragma unmanaged
#endif

#include "imageflow_private.h"
#include <string.h>

const int FLOW_MAX_BYTES_PP = 16;

// Ha, ha. The result of sx * sy * FLOW_MAX_BYTES_PP will overflow if the result is bigger than INT_MAX
// causing it to wrap around and be true. This is what the sx < INT_MAX / sy code does

static bool are_valid_bitmap_dimensions(int sx, int sy)
{
    return (sx > 0 && sy > 0 // positive dimensions
            && sx < INT_MAX / sy // no integer overflow
            && sx * FLOW_MAX_BYTES_PP < ((INT_MAX - FLOW_MAX_BYTES_PP) / sy)); // then we can safely check
}

uint32_t flow_pixel_format_bytes_per_pixel(flow_pixel_format format) { return (uint32_t)format; }
FLOW_HINT_HOT FLOW_HINT_PURE

    struct flow_bitmap_bgra *
    flow_bitmap_bgra_create_header(flow_c * context, int sx, int sy)
{
    struct flow_bitmap_bgra * im;
    if (!are_valid_bitmap_dimensions(sx, sy)) {
        FLOW_error(context, flow_status_Invalid_dimensions);
        return NULL;
    }
    im = (struct flow_bitmap_bgra *)FLOW_calloc(context, 1, sizeof(struct flow_bitmap_bgra));
    if (im == NULL) {
        FLOW_error(context, flow_status_Out_of_memory);
        return NULL;
    }
    im->w = sx;
    im->h = sy;
    im->pixels = NULL;
    im->pixels_readonly = true;
    im->stride_readonly = true;
    im->borrowed_pixels = true;
    im->can_reuse_space = false;
    return im;
}

struct flow_bitmap_bgra * flow_bitmap_bgra_create(flow_c * context, int sx, int sy, bool zeroed,
                                                  flow_pixel_format format)
{
    struct flow_bitmap_bgra * im = flow_bitmap_bgra_create_header(context, sx, sy);
    if (im == NULL) {
        FLOW_add_to_callstack(context);
        return NULL;
    }
    im->fmt = format;
    im->stride = im->w * flow_pixel_format_bytes_per_pixel(im->fmt);
    im->pixels_readonly = false;
    im->stride_readonly = false;
    im->borrowed_pixels = false;
    im->alpha_meaningful = im->fmt == flow_bgra32;
    if (zeroed) {
        im->pixels = (unsigned char *)FLOW_calloc_owned(context, im->h * im->stride, sizeof(unsigned char), im);
    } else {
        im->pixels = (unsigned char *)FLOW_malloc_owned(context, im->h * im->stride, im);
    }
    if (im->pixels == NULL) {
        FLOW_destroy(context, im);
        FLOW_error(context, flow_status_Out_of_memory);
        return NULL;
    }
    return im;
}

void flow_bitmap_bgra_destroy(flow_c * context, struct flow_bitmap_bgra * im) { FLOW_destroy(context, im); }

struct flow_bitmap_float * flow_bitmap_float_create_header(flow_c * context, int sx, int sy, int channels)
{
    struct flow_bitmap_float * im;

    if (!are_valid_bitmap_dimensions(sx, sy)) {
        FLOW_error(context, flow_status_Invalid_dimensions);
    }

    im = (struct flow_bitmap_float *)FLOW_calloc(context, 1, sizeof(struct flow_bitmap_float));
    if (im == NULL) {
        FLOW_error(context, flow_status_Out_of_memory);
        return NULL;
    }
    im->w = sx;
    im->h = sy;
    im->pixels = NULL;
    im->pixels_borrowed = true;
    im->channels = channels;
    im->float_stride = sx * channels;
    im->float_count = im->float_stride * sy;
    im->alpha_meaningful = channels == 4;
    im->alpha_premultiplied = true;
    return im;
}

struct flow_bitmap_float * flow_bitmap_float_create(flow_c * context, int sx, int sy, int channels, bool zeroed)
{
    struct flow_bitmap_float * im = flow_bitmap_float_create_header(context, sx, sy, channels);
    if (im == NULL) {
        FLOW_add_to_callstack(context);
        return NULL;
    }
    im->pixels_borrowed = false;
    if (zeroed) {
        im->pixels = (float *)FLOW_calloc_owned(context, im->float_count, sizeof(float), im);
    } else {
        im->pixels = (float *)FLOW_malloc_owned(context, im->float_count * sizeof(float), im);
    }
    if (im->pixels == NULL) {
        FLOW_destroy(context, im);
        FLOW_error(context, flow_status_Out_of_memory);
        return NULL;
    }
    return im;
}

void flow_bitmap_float_destroy(flow_c * context, struct flow_bitmap_float * im) { FLOW_destroy(context, im); }

bool flow_bitmap_bgra_compare(flow_c * c, struct flow_bitmap_bgra * a, struct flow_bitmap_bgra * b, bool * equal_out)
{
    if (a == NULL || b == NULL) {
        FLOW_error(c, flow_status_Null_argument);
        return false;
    }
    *equal_out = false;
    if (a->w != b->w || a->h != b->h || a->fmt != b->fmt) {
        return true;
    }
    // TODO: compare bgcolor and alpha_meaningful?
    // Dont' compare the full stride (padding), it could be windowed!
    uint32_t row_length = umin(b->stride, b->w * flow_pixel_format_bytes_per_pixel(b->fmt));
    for (uint32_t i = 0; i < b->h; i++) {
        if (memcmp(a->pixels + (i * a->stride), b->pixels + (i * b->stride), row_length) != 0) {
            *equal_out = false;
            return true;
        }
    }
    *equal_out = true;
    return true;
}

const char * flow_pixel_format_get_name(flow_pixel_format f, bool alpha_meaningful)
{
    switch (f) {
        case flow_bgr24:
            return "flow_bgr24";
        case flow_bgra32:
            return alpha_meaningful ? "flow_bgra32" : "Bgr32";
        default:
            return "?";
    }
}

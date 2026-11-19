#include "gfx/image.h"

#include <stdbool.h>

typedef struct Column {
    int x;
    int offset;
    int top;
    int height;
} Column;

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static int interpolate(int from, int to, int offset, int span)
{
    return from + (to - from) * offset / span;
}

static Column column_at(Quad quad, int x)
{
    int offset = x - quad.left;
    return (Column){
        .x = x,
        .offset = offset,
        .top = interpolate(quad.left_top, quad.right_top, offset, quad.width),
        .height = interpolate(quad.left_height, quad.right_height, offset, quad.width),
    };
}

static Color *pixel_address(Canvas *canvas, int x, int y)
{
    Rect bounds = canvas->bounds;
    return &canvas->pixels[(y - bounds.y) * bounds.width + (x - bounds.x)];
}

enum {
    STEP_SHIFT = 16
};

typedef struct Sampler {
    const Color *column;
    uint32_t position;
    uint32_t step;
} Sampler;

static Sampler sampler_at(const Column *column, const Image *image, Quad quad, int skip)
{
    uint32_t height = (uint32_t)column->height;
    uint32_t step = (((uint32_t)image->height << STEP_SHIFT) + height - 1) / height;
    int u_col = column->offset * image->width / quad.width;
    if (quad.left_height != quad.right_height && column->height > 0) {
        int64_t num = (int64_t)column->offset * quad.right_height * image->width;
        int64_t den = (int64_t)quad.width * column->height;
        u_col = (int)(num / den);
        if (u_col < 0) {
            u_col = 0;
        }
        if (u_col >= image->width) {
            u_col = image->width - 1;
        }
    }
    return (Sampler){
        .column = &image->pixels[u_col],
        .position = step * (uint32_t)skip,
        .step = step,
    };
}

static void draw_column(Canvas *canvas, Column column, const Image *image, Quad quad)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    int width = image->width;
    Sampler sampler = sampler_at(&column, image, quad, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    for (int count = last - first; count > 0; count--) {
        *target = sampler.column[(int)(sampler.position >> STEP_SHIFT) * width];
        target += stride;
        sampler.position += sampler.step;
    }
}

static unsigned light_at(QuadLight light, Column column, int quad_width)
{
    return (unsigned)interpolate(light.left, light.right, column.offset, quad_width);
}

static void draw_lit_column(Canvas *canvas, Column column, const Image *image, Quad quad,
                            unsigned light)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    int width = image->width;
    Sampler sampler = sampler_at(&column, image, quad, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    for (int count = last - first; count > 0; count--) {
        *target = color_scale(sampler.column[(int)(sampler.position >> STEP_SHIFT) * width], light);
        target += stride;
        sampler.position += sampler.step;
    }
}

static void draw_reflected_column(Canvas *canvas, Column column, const Image *image, Quad quad,
                                  unsigned light, int rows)
{
    Rect clip = canvas->clip;
    int top = column.top + column.height;
    int first = max_of(top, clip.y);
    int last = min_of(top + rows, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int skip = first - top;
    int stride = canvas->bounds.width;
    int width = image->width;
    int last_row = image->height - 1;
    Sampler sampler = sampler_at(&column, image, quad, skip);
    uint32_t fade_step = ((uint32_t)light << COLOR_LIGHT_SHIFT) / (uint32_t)rows;
    uint32_t faded = ((uint32_t)light << COLOR_LIGHT_SHIFT) - fade_step * (uint32_t)skip;
    Color *target = pixel_address(canvas, column.x, first);
    for (int count = last - first; count > 0; count--) {
        int row = max_of(last_row - (int)(sampler.position >> STEP_SHIFT), 0);
        *target = color_scale(sampler.column[row * width], faded >> COLOR_LIGHT_SHIFT);
        target += stride;
        sampler.position += sampler.step;
        faded -= fade_step;
    }
}

static void fill_column(Canvas *canvas, Column column, Color color)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    for (int y = first; y < last; y++) {
        *pixel_address(canvas, column.x, y) = color;
    }
}

static bool quad_visible(const Canvas *canvas, Quad quad, int *first, int *last)
{
    Rect clip = canvas->clip;
    *first = max_of(quad.left, clip.x);
    *last = min_of(quad.left + quad.width, clip.x + clip.width);
    return quad.width > 0 && *first < *last;
}


typedef struct SmoothSampler {
    const Color *col0;
    const Color *col1;
    uint32_t position;
    uint32_t step;
    unsigned u_weight;
} SmoothSampler;

static Color blend565(Color c0, Color c1, unsigned w)
{
    if (w == 0 || c0 == c1) {
        return c0;
    }
    if (w >= 32) {
        return c1;
    }
    unsigned r0 = (c0 >> 11) & 0x1F;
    unsigned g0 = (c0 >> 5) & 0x3F;
    unsigned b0 = c0 & 0x1F;
    unsigned r1 = (c1 >> 11) & 0x1F;
    unsigned g1 = (c1 >> 5) & 0x3F;
    unsigned b1 = c1 & 0x1F;
    unsigned r = (r0 * (32 - w) + r1 * w + 16) >> 5;
    unsigned g = (g0 * (32 - w) + g1 * w + 16) >> 5;
    unsigned b = (b0 * (32 - w) + b1 * w + 16) >> 5;
    return (Color)((r << 11) | (g << 5) | b);
}

static Color sample_bilinear(const Color *col0, const Color *col1, int width,
                             int v0, int v1, unsigned u_w, unsigned v_w)
{
    Color p00 = col0[v0 * width];
    if (u_w == 0 && v_w == 0) {
        return p00;
    }
    Color p10 = col1[v0 * width];
    Color p01 = col0[v1 * width];
    Color p11 = col1[v1 * width];
    Color top = blend565(p00, p10, u_w);
    Color bot = blend565(p01, p11, u_w);
    return blend565(top, bot, v_w);
}

static SmoothSampler smooth_sampler_at(const Column *column, const Image *image, Quad quad, int skip)
{
    uint32_t height = (uint32_t)column->height;
    uint32_t step = (((uint32_t)image->height << STEP_SHIFT) + height - 1) / height;
    uint32_t u_fixed = 0;
    if (quad.width > 0) {
        if (quad.left_height != quad.right_height && column->height > 0) {
            int64_t num = (int64_t)column->offset * quad.right_height * (int64_t)(image->width << STEP_SHIFT);
            int64_t den = (int64_t)quad.width * column->height;
            int64_t res = num / den;
            u_fixed = res > 0 ? (uint32_t)res : 0;
        } else {
            u_fixed = (uint32_t)(((int64_t)column->offset * (image->width << STEP_SHIFT)) / quad.width);
        }
    }
    int u0 = (int)(u_fixed >> STEP_SHIFT);
    if (u0 < 0) {
        u0 = 0;
    }
    if (u0 >= image->width) {
        u0 = image->width - 1;
    }
    int u1 = u0 + 1 < image->width ? u0 + 1 : u0;
    unsigned u_weight = (u_fixed & 0xFFFF) >> 11;
    return (SmoothSampler){
        .col0 = &image->pixels[u0],
        .col1 = &image->pixels[u1],
        .position = step * (uint32_t)skip,
        .step = step,
        .u_weight = u_weight,
    };
}

static void draw_column_smooth(Canvas *canvas, Column column, const Image *image, Quad quad)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    int width = image->width;
    int max_row = image->height - 1;
    SmoothSampler sampler = smooth_sampler_at(&column, image, quad, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    uint32_t pos = sampler.position;
    for (int count = last - first; count > 0; count--) {
        int v0 = (int)(pos >> STEP_SHIFT);
        if (v0 > max_row) {
            v0 = max_row;
        }
        int v1 = v0 + 1 <= max_row ? v0 + 1 : v0;
        unsigned v_weight = (pos & 0xFFFF) >> 11;
        *target = sample_bilinear(sampler.col0, sampler.col1, width, v0, v1, sampler.u_weight, v_weight);
        target += stride;
        pos += sampler.step;
    }
}

static void draw_lit_column_smooth(Canvas *canvas, Column column, const Image *image, Quad quad,
                                   unsigned light)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    int width = image->width;
    int max_row = image->height - 1;
    SmoothSampler sampler = smooth_sampler_at(&column, image, quad, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    uint32_t pos = sampler.position;
    for (int count = last - first; count > 0; count--) {
        int v0 = (int)(pos >> STEP_SHIFT);
        if (v0 > max_row) {
            v0 = max_row;
        }
        int v1 = v0 + 1 <= max_row ? v0 + 1 : v0;
        unsigned v_weight = (pos & 0xFFFF) >> 11;
        Color c = sample_bilinear(sampler.col0, sampler.col1, width, v0, v1, sampler.u_weight, v_weight);
        *target = color_scale(c, light);
        target += stride;
        pos += sampler.step;
    }
}

static void draw_reflected_column_smooth(Canvas *canvas, Column column, const Image *image, Quad quad,
                                         unsigned light, int rows)
{
    Rect clip = canvas->clip;
    int top = column.top + column.height;
    int first = max_of(top, clip.y);
    int last = min_of(top + rows, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int skip = first - top;
    int stride = canvas->bounds.width;
    int width = image->width;
    int last_row = image->height - 1;
    SmoothSampler sampler = smooth_sampler_at(&column, image, quad, skip);
    uint32_t fade_step = ((uint32_t)light << COLOR_LIGHT_SHIFT) / (uint32_t)rows;
    uint32_t faded = ((uint32_t)light << COLOR_LIGHT_SHIFT) - fade_step * (uint32_t)skip;
    Color *target = pixel_address(canvas, column.x, first);
    uint32_t pos = sampler.position;
    for (int count = last - first; count > 0; count--) {
        int v0 = max_of(last_row - (int)(pos >> STEP_SHIFT), 0);
        int v1 = v0 > 0 ? v0 - 1 : 0;
        unsigned v_weight = (pos & 0xFFFF) >> 11;
        Color c = sample_bilinear(sampler.col0, sampler.col1, width, v0, v1, sampler.u_weight, v_weight);
        *target = color_scale(c, faded >> COLOR_LIGHT_SHIFT);
        target += stride;
        pos += sampler.step;
        faded -= fade_step;
    }
}

void image_draw_quad_smooth(Canvas *canvas, Image image, Quad quad)
{
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_column_smooth(canvas, column, &image, quad);
        }
    }
}

void image_draw_quad_smooth_lit(Canvas *canvas, Image image, Quad quad, QuadLight light)
{
    if (light.left >= COLOR_LIGHT_FULL && light.right >= COLOR_LIGHT_FULL) {
        image_draw_quad_smooth(canvas, image, quad);
        return;
    }
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_lit_column_smooth(canvas, column, &image, quad,
                                   light_at(light, column, quad.width));
        }
    }
}

void image_draw_reflection_smooth(Canvas *canvas, Image image, Quad quad, QuadLight light, int rows)
{
    int first = 0;
    int last = 0;
    if (rows <= 0 || image.width <= 0 || image.height <= 0 ||
        !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_reflected_column_smooth(canvas, column, &image, quad,
                                         light_at(light, column, quad.width), rows);
        }
    }
}


void image_draw_quad(Canvas *canvas, Image image, Quad quad)
{
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_column(canvas, column, &image, quad);
        }
    }
}

void image_draw_quad_lit(Canvas *canvas, Image image, Quad quad, QuadLight light)
{
    if (light.left >= COLOR_LIGHT_FULL && light.right >= COLOR_LIGHT_FULL) {
        image_draw_quad(canvas, image, quad);
        return;
    }
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_lit_column(canvas, column, &image, quad,
                            light_at(light, column, quad.width));
        }
    }
}

void image_draw_reflection(Canvas *canvas, Image image, Quad quad, QuadLight light, int rows)
{
    int first = 0;
    int last = 0;
    if (rows <= 0 || image.width <= 0 || image.height <= 0 ||
        !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_reflected_column(canvas, column, &image, quad,
                                  light_at(light, column, quad.width), rows);
        }
    }
}

void image_fill_quad(Canvas *canvas, Quad quad, Color color)
{
    int first = 0;
    int last = 0;
    if (!quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            fill_column(canvas, column, color);
        }
    }
}

#include "gfx/text.h"

#include <stddef.h>

#include "core/utf8.h"

static Rect glyph_area(const Glyph *glyph, Point pen)
{
    return (Rect){pen.x + glyph->left, pen.y - glyph->top, glyph->width, glyph->height};
}

static unsigned isqrt_6bit(unsigned n)
{
    unsigned res = 0;
    unsigned bit = 1u << 10;
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

Color text_blend(Color background, Color foreground, uint8_t level)
{
    static const uint8_t SHARPENED[16] = {
        0, 0, 2, 4, 6, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15
    };
    if (level >= COLOR_ALPHA_OPAQUE) {
        return foreground;
    }
    uint8_t effective = SHARPENED[level];
    if (effective == 0) {
        return background;
    }
    if (effective >= COLOR_ALPHA_OPAQUE) {
        return foreground;
    }
    unsigned back_r = (background >> 11) & 0x1F;
    unsigned back_g = (background >> 5) & 0x3F;
    unsigned back_b = background & 0x1F;
    unsigned fore_r = (foreground >> 11) & 0x1F;
    unsigned fore_g = (foreground >> 5) & 0x3F;
    unsigned fore_b = foreground & 0x1F;
    unsigned back_weight = (unsigned)COLOR_ALPHA_OPAQUE - effective;
    unsigned r_sq = (back_r * back_r * back_weight + fore_r * fore_r * effective + 7u) / 15u;
    unsigned g_sq = (back_g * back_g * back_weight + fore_g * fore_g * effective + 7u) / 15u;
    unsigned b_sq = (back_b * back_b * back_weight + fore_b * fore_b * effective + 7u) / 15u;
    return (Color)((isqrt_6bit(r_sq) << 11) | (isqrt_6bit(g_sq) << 5) | isqrt_6bit(b_sq));
}

static void draw_glyph(Canvas *canvas, TextStyle style, const Glyph *glyph, Point pen)
{
    Rect area = glyph_area(glyph, pen);
    Rect visible = rect_intersection(area, canvas->clip);
    if (rect_is_empty(visible)) {
        return;
    }
    const uint8_t *bitmap = font_glyph_bitmap(style.font, glyph);
    int stride = canvas->bounds.width;
    Color *row =
        &canvas->pixels[(visible.y - canvas->bounds.y) * stride + (visible.x - canvas->bounds.x)];
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        Color *pixel = row;
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            uint8_t level = font_bitmap_level(bitmap, glyph, x - area.x, y - area.y);
            if (level > 0) {
                *pixel = text_blend(*pixel, style.color, level);
            }
            pixel++;
        }
        row += stride;
    }
}

int text_width(const Font *font, const char *text)
{
    int width = 0;
    const char *cursor = text;
    for (uint32_t codepoint = utf8_next(&cursor); codepoint != 0; codepoint = utf8_next(&cursor)) {
        const Glyph *glyph = font_find_glyph(font, codepoint);
        if (glyph != NULL) {
            width += glyph->advance;
        }
    }
    return width;
}

static size_t byte_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

int text_draw(Canvas *canvas, TextStyle style, Point pen, const char *text)
{
    return text_draw_span(canvas, style, pen, (TextSpan){text, byte_length(text)});
}

int text_draw_span(Canvas *canvas, TextStyle style, Point pen, TextSpan span)
{
    const char *cursor = span.start;
    const char *end = span.start + span.byte_count;
    while (cursor < end) {
        const Glyph *glyph = font_find_glyph(style.font, utf8_next(&cursor));
        if (glyph == NULL) {
            continue;
        }
        draw_glyph(canvas, style, glyph, pen);
        pen.x += glyph->advance;
    }
    return pen.x;
}

enum {
    SUBPIXEL_BITS = 8,
    SUBPIXEL_ONE = 1 << SUBPIXEL_BITS,
    SUBPIXEL_MASK = SUBPIXEL_ONE - 1,
    FULL_LEVEL = 15,
    EDGE_FLOOR = 4,
    EDGE_CEILING = 11
};

static int level_at(const uint8_t *bitmap, const Glyph *glyph, int x, int y)
{
    if (x < 0 || y < 0 || x >= glyph->width || y >= glyph->height) {
        return 0;
    }
    return font_bitmap_level(bitmap, glyph, x, y);
}

static int source_position(int target, int scale)
{
    return ((2 * target + 1) * SUBPIXEL_ONE) / (2 * scale) - SUBPIXEL_ONE / 2;
}

static uint8_t smoothed_level(const uint8_t *bitmap, const Glyph *glyph, int x, int y, int scale)
{
    int source_x = source_position(x, scale) + SUBPIXEL_ONE;
    int source_y = source_position(y, scale) + SUBPIXEL_ONE;
    int left = (source_x >> SUBPIXEL_BITS) - 1;
    int top = (source_y >> SUBPIXEL_BITS) - 1;
    int across = source_x & SUBPIXEL_MASK;
    int down = source_y & SUBPIXEL_MASK;
    int upper = level_at(bitmap, glyph, left, top) * (SUBPIXEL_ONE - across) +
                level_at(bitmap, glyph, left + 1, top) * across;
    int lower = level_at(bitmap, glyph, left, top + 1) * (SUBPIXEL_ONE - across) +
                level_at(bitmap, glyph, left + 1, top + 1) * across;
    int level = (upper * (SUBPIXEL_ONE - down) + lower * down) >> (2 * SUBPIXEL_BITS);
    int sharpened = (level - EDGE_FLOOR) * FULL_LEVEL / (EDGE_CEILING - EDGE_FLOOR);
    return (uint8_t)(sharpened < 0 ? 0 : sharpened > FULL_LEVEL ? FULL_LEVEL : sharpened);
}

void text_draw_glyph_scaled(Canvas *canvas, TextStyle style, Rect box, uint32_t codepoint,
                            int scale)
{
    const Glyph *glyph = font_find_glyph(style.font, codepoint);
    if (glyph == NULL || scale < 1) {
        return;
    }
    int width = glyph->width * scale;
    int height = glyph->height * scale;
    Rect area = {box.x + (box.width - width) / 2, box.y + (box.height - height) / 2, width, height};
    Rect visible = rect_intersection(rect_intersection(area, box), canvas->clip);
    const uint8_t *bitmap = font_glyph_bitmap(style.font, glyph);
    int stride = canvas->bounds.width;
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        Color *pixel =
            &canvas->pixels[(y - canvas->bounds.y) * stride + (visible.x - canvas->bounds.x)];
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            uint8_t level = smoothed_level(bitmap, glyph, x - area.x, y - area.y, scale);
            if (level > 0) {
                *pixel = text_blend(*pixel, style.color, level);
            }
            pixel++;
        }
    }
}

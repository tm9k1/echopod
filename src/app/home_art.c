#include "home_art.h"

#include "gfx/image.h"
#include "ui/label.h"

enum {
    COVER_MARGIN = 2,
    REFLECTION_LIGHT = 80,
    NOTICE_MARGIN = 6
};

static const Color PANEL_BACKGROUND = COLOR_RGB(8, 8, 10);
static const Color NOTICE_TEXT = COLOR_RGB(200, 200, 206);

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme)
{
    *art = (HomeArt){.covers = covers, .store = store, .player = player, .theme = theme};
}

static uint32_t album_count(const HomeArt *art)
{
    return art->store->available ? library_music_count(&art->store->library, LIBRARY_ALBUMS) : 0;
}

static bool playing_album(const HomeArt *art, uint16_t *album)
{
    PlayerStatus status = player_status(art->player);
    LibraryTrack track;
    if (!status.has_track || !library_read_track(&art->store->library, status.track, &track)) {
        return false;
    }
    *album = track.album;
    return true;
}

static void start_search(HomeArt *art, uint32_t from, uint32_t count, uint32_t now_ms)
{
    art->searching = true;
    art->probe = (uint16_t)(from % count);
    art->probes_left = count;
    art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
}

static bool probe_next(HomeArt *art, uint32_t count)
{
    uint16_t album = (uint16_t)(art->probe % count);
    art->probe = (uint16_t)((album + 1u) % count);
    art->probes_left--;
    if (!cover_cache_has_cover(art->covers, album)) {
        return false;
    }
    art->album = album;
    art->chosen = true;
    return true;
}

static void search(HomeArt *art, uint32_t count)
{
    for (int tried = 0; tried < HOME_ART_ALBUMS_PER_UPDATE && art->searching; tried++) {
        art->searching = art->probes_left > 0 && !probe_next(art, count);
    }
}

static void cycle(HomeArt *art, uint32_t count, uint32_t now_ms)
{
    if (!art->cycling) {
        art->cycling = true;
        start_search(art, 0, count, now_ms);
    } else if ((int32_t)(now_ms - art->next_change_ms) >= 0) {
        start_search(art, (uint32_t)art->album + 1u, count, now_ms);
    }
    search(art, count);
}

static void forget_album(HomeArt *art)
{
    art->cycling = false;
    art->searching = false;
    art->chosen = false;
}

static void follow_playing(HomeArt *art, uint16_t album)
{
    forget_album(art);
    art->album = album;
    art->chosen = true;
}

void home_art_update(void *context, uint32_t now_ms)
{
    HomeArt *art = context;
    uint32_t count = album_count(art);
    uint16_t album;
    if (count == 0) {
        forget_album(art);
    } else if (playing_album(art, &album)) {
        follow_playing(art, album);
    } else if (art->covers->available) {
        cycle(art, count, now_ms);
    }
    art->has_album = art->chosen;
    if (art->chosen) {
        cover_cache_load_full(art->covers, art->album);
    }
}

void home_art_draw(void *context, Canvas *canvas, Rect area)
{
    const HomeArt *art = context;
    bool below = theme_is_portrait(art->theme);
    Rect divider =
        below ? (Rect){area.x, area.y, area.width, 1} : (Rect){area.x, area.y, 1, area.height};
    canvas_fill_rect(canvas, area, PANEL_BACKGROUND);
    canvas_fill_rect(canvas, divider, art->theme->colors.divider);
    if (art->notice != NULL) {
        TextStyle style = {.font = art->theme->fonts.small, .color = NOTICE_TEXT};
        Rect line = {area.x + NOTICE_MARGIN, area.y, area.width - 2 * NOTICE_MARGIN, area.height};
        Rect text = label_centered_area(style.font, line, art->notice);
        label_draw(canvas, style, (Label){.area = text, .text = art->notice});
        return;
    }
    Image cover;
    if (!art->has_album || cover_cache_full(art->covers, art->album, &cover) != COVER_LOADED) {
        return;
    }
    int avail_x = below ? area.x : area.x + 1;
    int avail_y = below ? area.y + 1 : area.y;
    int avail_w = below ? area.width : area.width - 1;
    int avail_h = below ? area.height - 1 : area.height;
    int max_w = avail_w - 2 * COVER_MARGIN;
    int max_h = avail_h - 2 * COVER_MARGIN;
    if (max_w <= 0 || max_h <= 0) {
        return;
    }
    int draw_w = max_w;
    int draw_h = max_h;
    if (cover.width > 0 && cover.height > 0) {
        if ((int64_t)cover.width * max_h > (int64_t)max_w * cover.height) {
            draw_w = max_w;
            draw_h = (int)(((int64_t)max_w * cover.height + cover.width / 2) / cover.width);
        } else {
            draw_h = max_h;
            draw_w = (int)(((int64_t)max_h * cover.width + cover.height / 2) / cover.height);
        }
    }
    if (draw_w < 1) {
        draw_w = 1;
    }
    if (draw_h < 1) {
        draw_h = 1;
    }
    int left = avail_x + (avail_w - draw_w) / 2;
    int top = avail_y + (avail_h - draw_h) / 2;
    Quad quad = {left, draw_w, top, draw_h, top, draw_h};
    image_draw_quad_smooth(canvas, cover, quad);
    int rows = area.y + area.height - (top + draw_h);
    if (rows >= 8) {
        image_draw_reflection_smooth(canvas, cover, quad,
                                     (QuadLight){REFLECTION_LIGHT, REFLECTION_LIGHT}, rows);
    }
}

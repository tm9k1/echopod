#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "sim_display.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "core/key.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "sim_input.h"
#include "sim_storage.h"

enum {
    BITS_PER_PIXEL = 16,
    PANEL_ROW_BYTES = HAL_DISPLAY_LONG_SIDE * 2,
    FULL_INTENSITY = 255,
    DIMMEST_LEVEL_OFFSET = 2,
    SIM_CASING_BASE_W = 478,
    SIM_CASING_BASE_H = 324
};

typedef enum SimButtonId {
    SIM_BTN_POWER,
    SIM_BTN_VOL_DOWN,
    SIM_BTN_VOL_UP,
    SIM_BTN_PLAY,
    SIM_BTN_MENU,
    SIM_BTN_BACK,
    SIM_BTN_FILES,
    SIM_BTN_ROTATE,
    SIM_BTN_CLOSE,
    SIM_BTN_COUNT
} SimButtonId;

typedef struct SimChassisButton {
    SimButtonId id;
    Key key;
    SDL_Rect rect;
    bool pressed;
    const char *label;
} SimChassisButton;

typedef struct SimDisplay {
    uint16_t panel[HAL_DISPLAY_LONG_SIDE][HAL_DISPLAY_LONG_SIDE];
    int width;
    int height;
    int scale;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Texture *casing_texture;
    int rotation;
    HalDisplayRotation hal_rotation;
    float screen_center_x;
    float screen_center_y;
    bool powered;
    uint8_t brightness;
    int win_width;
    int win_height;
    SDL_Rect casing_draw_rect;
    SDL_Rect screen_draw_rect;
    SDL_Rect hit_screen_rect;
    SimChassisButton buttons[SIM_BTN_COUNT];
} SimDisplay;

static const uint8_t FONT5X7[59][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x5F, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50},
    {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x14, 0x08, 0x3E, 0x08, 0x14},
    {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00},
    {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00},
    {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08},
    {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}
};

static SimDisplay display = {.width = HAL_DISPLAY_WIDTH,
                             .height = HAL_DISPLAY_HEIGHT,
                             .scale = 2,
                             .powered = true,
                             .brightness = HAL_DISPLAY_BRIGHTNESS_MAX};

static void show_panel(void);

static void draw_char5x7(SDL_Renderer *renderer, char c, int x, int y, SDL_Color color)
{
    if (c < 32 || c > 90) {
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else {
            return;
        }
    }
    const uint8_t *cols = FONT5X7[(int)(c - 32)];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = cols[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 1) {
                SDL_RenderDrawPoint(renderer, x + col, y + row);
            }
        }
    }
}

static void draw_text5x7(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color)
{
    int cur_x = x;
    while (*text != '\0') {
        draw_char5x7(renderer, *text, cur_x, y, color);
        cur_x += 6;
        text++;
    }
}

typedef struct {
    SimButtonId id;
    Key key;
    const char *label;
    float x_tex;
    float y_tex;
    float w_tex;
    float h_tex;
} SimPhysicalButtonDef;

static const SimPhysicalButtonDef PHYSICAL_BUTTONS[6] = {
    {SIM_BTN_POWER,    KEY_POWER,       "POWER",  49.0f,  0.0f, 48.0f, 24.0f},
    {SIM_BTN_VOL_DOWN, KEY_VOLUME_DOWN, "VOL -", 119.0f,  0.0f, 51.0f, 24.0f},
    {SIM_BTN_VOL_UP,   KEY_VOLUME_UP,   "VOL +", 170.0f,  0.0f, 51.0f, 24.0f},
    {SIM_BTN_PLAY,     KEY_PLAY,        "PLAY",  243.0f,  0.0f, 48.0f, 24.0f},
    {SIM_BTN_MENU,     KEY_MENU,        "MENU",  312.0f,  0.0f, 48.0f, 24.0f},
    {SIM_BTN_BACK,     KEY_BACK,        "BACK",  382.0f,  0.0f, 48.0f, 24.0f},
};

static void recalculate_layout(void)
{
    int s = display.scale > 0 ? display.scale : 2;
    int c_w = (int)(478.0f * (320.0f * (float)s) / 379.0f);
    int c_h = (int)(324.0f * (320.0f * (float)s) / 379.0f);
    int rot = display.rotation % 4;
    bool portrait = (rot == 1 || rot == 3);
    int cur_cw = portrait ? c_h : c_w;
    int cur_ch = portrait ? c_w : c_h;
    float dx0 = -1.0f * (float)s * 0.5f;
    float dy0 = -71.0f * (float)s * 0.5f;
    int sw = 320 * s;
    int sh = 170 * s;

    int case_margin_left = 16;
    int case_margin_top = 16;
    int case_margin_bottom = 20;
    int right_col_w = 96;

    display.win_width = case_margin_left + cur_cw + 16 + right_col_w + 16;
    display.win_height = case_margin_top + cur_ch + case_margin_bottom;

    int case_x = case_margin_left;
    int case_y = case_margin_top;
    float cx = (float)case_x + (float)cur_cw * 0.5f;
    float cy = (float)case_y + (float)cur_ch * 0.5f;

    display.casing_draw_rect = (SDL_Rect){(int)(cx - (float)c_w * 0.5f), (int)(cy - (float)c_h * 0.5f), c_w, c_h};

    float scx, scy;
    if (rot == 0) {
        scx = cx + dx0;
        scy = cy + dy0;
    } else if (rot == 1) {
        scx = cx - dy0;
        scy = cy + dx0;
    } else if (rot == 2) {
        scx = cx - dx0;
        scy = cy - dy0;
    } else {
        scx = cx + dy0;
        scy = cy - dx0;
    }
    display.screen_center_x = scx;
    display.screen_center_y = scy;

    if (portrait) {
        display.hit_screen_rect = (SDL_Rect){(int)(scx - (float)sh * 0.5f), (int)(scy - (float)sw * 0.5f), sh, sw};
    } else {
        display.hit_screen_rect = (SDL_Rect){(int)(scx - (float)sw * 0.5f), (int)(scy - (float)sh * 0.5f), sw, sh};
    }
    display.screen_draw_rect = display.hit_screen_rect;

    int ux = case_x + cur_cw + 16;
    int uw = right_col_w;
    display.buttons[SIM_BTN_CLOSE] = (SimChassisButton){SIM_BTN_CLOSE, (Key)KEY_COUNT, {ux + uw - 26, 12, 26, 26}, false, "X"};
    display.buttons[SIM_BTN_ROTATE] = (SimChassisButton){SIM_BTN_ROTATE, (Key)KEY_COUNT, {ux, 48, uw, 34}, false, "ROTATE"};
    display.buttons[SIM_BTN_FILES] = (SimChassisButton){SIM_BTN_FILES, (Key)KEY_COUNT, {ux, 90, uw, 34}, false, "FILES"};

    float scale = (float)c_w / 478.0f;
    float CX_tex = 478.0f * 0.5f;
    float CY_tex = 324.0f * 0.5f;

    for (int i = 0; i < 6; i++) {
        const SimPhysicalButtonDef *pb = &PHYSICAL_BUTTONS[i];
        float btn_cx_tex = pb->x_tex + pb->w_tex * 0.5f;
        float btn_cy_tex = pb->y_tex + pb->h_tex * 0.5f;
        float u = (btn_cx_tex - CX_tex) * scale;
        float v = (btn_cy_tex - CY_tex) * scale;

        float rx, ry, rw, rh;
        if (rot == 0) {
            rx = u;
            ry = v;
            rw = pb->w_tex * scale;
            rh = pb->h_tex * scale;
        } else if (rot == 1) {
            rx = -v;
            ry = u;
            rw = pb->h_tex * scale;
            rh = pb->w_tex * scale;
        } else if (rot == 2) {
            rx = -u;
            ry = -v;
            rw = pb->w_tex * scale;
            rh = pb->h_tex * scale;
        } else {
            rx = v;
            ry = -u;
            rw = pb->h_tex * scale;
            rh = pb->w_tex * scale;
        }

        SDL_Rect rect = {
            (int)(cx + rx - rw * 0.5f),
            (int)(cy + ry - rh * 0.5f),
            (int)rw,
            (int)rh
        };
        display.buttons[pb->id] = (SimChassisButton){pb->id, pb->key, rect, false, pb->label};
    }
}

static SDL_HitTestResult hit_test_callback(SDL_Window *win, const SDL_Point *area, void *data)
{
    (void)win;
    (void)data;
    for (int i = 0; i < SIM_BTN_COUNT; i++) {
        if (SDL_PointInRect(area, &display.buttons[i].rect)) {
            return SDL_HITTEST_NORMAL;
        }
    }
    if (SDL_PointInRect(area, &display.hit_screen_rect)) {
        return SDL_HITTEST_NORMAL;
    }
    return SDL_HITTEST_DRAGGABLE;
}

static bool area_fits_panel(int x, int y, int width, int height)
{
    return x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= display.width &&
           y + height <= display.height;
}

static bool make_texture(void)
{
    if (display.texture != NULL) {
        SDL_DestroyTexture(display.texture);
    }
    display.texture = SDL_CreateTexture(display.renderer, SDL_PIXELFORMAT_RGB565,
                                        SDL_TEXTUREACCESS_STREAMING, display.width, display.height);
    return display.texture != NULL;
}

static void load_casing_texture(void)
{
    static const char *PATHS[] = {
        "assets/casing/echo_mini_casing.bmp",
        "echo_mini_casing.bmp",
        "../assets/casing/echo_mini_casing.bmp",
        "../../assets/casing/echo_mini_casing.bmp"
    };
    for (size_t i = 0; i < sizeof PATHS / sizeof PATHS[0]; i++) {
        SDL_Surface *surf = SDL_LoadBMP(PATHS[i]);
        if (surf != NULL) {
            display.casing_texture = SDL_CreateTextureFromSurface(display.renderer, surf);
            SDL_FreeSurface(surf);
            if (display.casing_texture != NULL) {
                return;
            }
        }
    }
}

bool sim_display_open(int scale)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    display.scale = scale > 0 ? scale : 2;
    display.rotation = 0;
    recalculate_layout();
    display.window =
        SDL_CreateWindow("EchoPod", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         display.win_width, display.win_height, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    if (display.window == NULL) {
        return false;
    }
    SDL_SetWindowHitTest(display.window, hit_test_callback, NULL);
    display.renderer = SDL_CreateRenderer(display.window, -1, SDL_RENDERER_ACCELERATED);
    if (display.renderer == NULL) {
        return false;
    }
    load_casing_texture();
    return make_texture();
}

void sim_display_close(void)
{
    if (display.casing_texture != NULL) {
        SDL_DestroyTexture(display.casing_texture);
    }
    if (display.texture != NULL) {
        SDL_DestroyTexture(display.texture);
    }
    if (display.renderer != NULL) {
        SDL_DestroyRenderer(display.renderer);
    }
    if (display.window != NULL) {
        SDL_DestroyWindow(display.window);
    }
    display = (SimDisplay){.width = HAL_DISPLAY_WIDTH,
                           .height = HAL_DISPLAY_HEIGHT,
                           .scale = 2,
                           .powered = true,
                           .brightness = HAL_DISPLAY_BRIGHTNESS_MAX};
}

void sim_display_set_title(const char *title)
{
    if (display.window != NULL) {
        SDL_SetWindowTitle(display.window, title);
    }
}

void sim_display_rotate_clockwise(void)
{
    display.rotation = (display.rotation + 1) % 4;
    recalculate_layout();
    if (display.window != NULL) {
        SDL_SetWindowSize(display.window, display.win_width, display.win_height);
        show_panel();
    }
}

void sim_display_open_files(void)
{
    const char *dir = sim_storage_internal_directory();
    if (dir == NULL || dir[0] == '\0') {
        dir = "internal_storage";
    }
#ifdef _WIN32
    char win_path[MAX_PATH];
    strncpy(win_path, dir, sizeof(win_path) - 1);
    win_path[sizeof(win_path) - 1] = '\0';
    for (char *p = win_path; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
    char full_path[MAX_PATH];
    if (GetFullPathNameA(win_path, MAX_PATH, full_path, NULL) > 0) {
        ShellExecuteA(NULL, "open", full_path, NULL, NULL, SW_SHOWNORMAL);
    } else {
        ShellExecuteA(NULL, "open", win_path, NULL, NULL, SW_SHOWNORMAL);
    }
#endif
}

bool sim_display_is_power_pressed(void)
{
    return display.buttons[SIM_BTN_POWER].pressed || sim_input_is_power_down();
}

bool sim_display_handle_event(const SDL_Event *event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Point pt = {event->button.x, event->button.y};
        for (int i = 0; i < SIM_BTN_COUNT; i++) {
            if (SDL_PointInRect(&pt, &display.buttons[i].rect)) {
                display.buttons[i].pressed = true;
                if (display.buttons[i].id == SIM_BTN_CLOSE) {
                    return false;
                }
                if (display.buttons[i].id == SIM_BTN_ROTATE) {
                    sim_display_rotate_clockwise();
                    return true;
                }
                if (display.buttons[i].id == SIM_BTN_FILES) {
                    sim_display_open_files();
                    return true;
                }
                sim_input_inject(display.buttons[i].key, true, hal_clock_milliseconds());
                return true;
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < SIM_BTN_COUNT; i++) {
            if (display.buttons[i].pressed) {
                display.buttons[i].pressed = false;
                if (display.buttons[i].id != SIM_BTN_CLOSE &&
                    display.buttons[i].id != SIM_BTN_ROTATE &&
                    display.buttons[i].id != SIM_BTN_FILES) {
                    sim_input_inject(display.buttons[i].key, false, hal_clock_milliseconds());
                }
            }
        }
    }
    return true;
}

bool sim_display_save_bmp(const char *path)
{
    if (display.renderer != NULL) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
            0, display.win_width, display.win_height, 24, SDL_PIXELFORMAT_RGB24);
        if (surface == NULL) {
            return false;
        }
        if (SDL_RenderReadPixels(display.renderer, NULL, SDL_PIXELFORMAT_RGB24, surface->pixels, surface->pitch) != 0) {
            SDL_FreeSurface(surface);
            return false;
        }
        bool saved = SDL_SaveBMP(surface, path) == 0;
        SDL_FreeSurface(surface);
        return saved;
    }
    SDL_Surface *frame =
        SDL_CreateRGBSurfaceWithFormatFrom(display.panel, display.width, display.height,
                                           BITS_PER_PIXEL, PANEL_ROW_BYTES, SDL_PIXELFORMAT_RGB565);
    if (frame == NULL) {
        return false;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(frame, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(frame);
    if (converted == NULL) {
        return false;
    }
    bool saved = SDL_SaveBMP(converted, path) == 0;
    SDL_FreeSurface(converted);
    return saved;
}

uint32_t hal_display_panel_id(void)
{
    return 0;
}

void hal_display_set_rotation(HalDisplayRotation rotation)
{
    display.hal_rotation = rotation;
    int target_rot = 0;
    if (rotation == HAL_DISPLAY_PORTRAIT) {
        target_rot = 1;
    } else if (rotation == HAL_DISPLAY_PORTRAIT_FLIPPED) {
        target_rot = 3;
    }
    display.rotation = target_rot;
    bool landscape = rotation == HAL_DISPLAY_LANDSCAPE;
    int width = landscape ? HAL_DISPLAY_LONG_SIDE : HAL_DISPLAY_SHORT_SIDE;
    int height = landscape ? HAL_DISPLAY_SHORT_SIDE : HAL_DISPLAY_LONG_SIDE;
    display.width = width;
    display.height = height;
    SDL_memset(display.panel, 0, sizeof display.panel);
    recalculate_layout();
    if (display.window != NULL) {
        SDL_SetWindowSize(display.window, display.win_width, display.win_height);
        make_texture();
        show_panel();
    }
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    if (!area_fits_panel(x, y, width, height)) {
        SDL_Log("display write outside the panel: %d,%d %dx%d", x, y, width, height);
        return;
    }
    for (int row = 0; row < height; row++) {
        SDL_memcpy(&display.panel[y + row][x], &pixels[row * width],
                   (size_t)width * sizeof(uint16_t));
    }
}

static Uint8 brightness_intensity(uint8_t level)
{
    int span = HAL_DISPLAY_BRIGHTNESS_MAX + DIMMEST_LEVEL_OFFSET;
    return (Uint8)(FULL_INTENSITY * (level + DIMMEST_LEVEL_OFFSET) / span);
}

static void draw_button_icon(SDL_Renderer *renderer, SimButtonId id, int cx, int cy)
{
    if (id == SIM_BTN_POWER) {
        SDL_RenderDrawLine(renderer, cx, cy - 5, cx, cy - 1);
        SDL_RenderDrawLine(renderer, cx - 3, cy - 3, cx - 4, cy);
        SDL_RenderDrawLine(renderer, cx - 4, cy, cx - 4, cy + 2);
        SDL_RenderDrawLine(renderer, cx - 4, cy + 2, cx - 2, cy + 4);
        SDL_RenderDrawLine(renderer, cx - 2, cy + 4, cx + 2, cy + 4);
        SDL_RenderDrawLine(renderer, cx + 2, cy + 4, cx + 4, cy + 2);
        SDL_RenderDrawLine(renderer, cx + 4, cy + 2, cx + 4, cy);
        SDL_RenderDrawLine(renderer, cx + 4, cy, cx + 3, cy - 3);
    } else if (id == SIM_BTN_VOL_DOWN) {
        SDL_RenderDrawLine(renderer, cx - 5, cy, cx + 5, cy);
        SDL_RenderDrawLine(renderer, cx - 5, cy + 1, cx + 5, cy + 1);
    } else if (id == SIM_BTN_VOL_UP) {
        SDL_RenderDrawLine(renderer, cx - 5, cy, cx + 5, cy);
        SDL_RenderDrawLine(renderer, cx - 5, cy + 1, cx + 5, cy + 1);
        SDL_RenderDrawLine(renderer, cx, cy - 5, cx, cy + 5);
        SDL_RenderDrawLine(renderer, cx + 1, cy - 5, cx + 1, cy + 5);
    } else if (id == SIM_BTN_PLAY) {
        for (int p = 0; p < 6; p++) {
            SDL_RenderDrawLine(renderer, cx - 7 + p, cy - (5 - p), cx - 7 + p, cy + (5 - p));
        }
        SDL_RenderDrawLine(renderer, cx + 2, cy - 5, cx + 2, cy + 5);
        SDL_RenderDrawLine(renderer, cx + 3, cy - 5, cx + 3, cy + 5);
        SDL_RenderDrawLine(renderer, cx + 6, cy - 5, cx + 6, cy + 5);
        SDL_RenderDrawLine(renderer, cx + 7, cy - 5, cx + 7, cy + 5);
    } else if (id == SIM_BTN_MENU) {
        SDL_RenderDrawLine(renderer, cx - 6, cy - 4, cx + 6, cy - 4);
        SDL_RenderDrawLine(renderer, cx - 6, cy - 3, cx + 6, cy - 3);
        SDL_RenderDrawLine(renderer, cx - 6, cy, cx + 6, cy);
        SDL_RenderDrawLine(renderer, cx - 6, cy + 1, cx + 6, cy + 1);
        SDL_RenderDrawLine(renderer, cx - 6, cy + 4, cx + 6, cy + 4);
        SDL_RenderDrawLine(renderer, cx - 6, cy + 5, cx + 6, cy + 5);
    } else if (id == SIM_BTN_BACK) {
        SDL_RenderDrawLine(renderer, cx - 5, cy, cx + 5, cy);
        SDL_RenderDrawLine(renderer, cx - 5, cy + 1, cx + 5, cy + 1);
        SDL_RenderDrawLine(renderer, cx - 5, cy - 3, cx - 5, cy + 3);
        SDL_RenderDrawLine(renderer, cx - 4, cy - 2, cx - 4, cy + 2);
        SDL_RenderDrawLine(renderer, cx - 3, cy - 1, cx - 3, cy + 1);
        SDL_RenderDrawLine(renderer, cx + 5, cy, cx + 5, cy - 4);
    } else if (id == SIM_BTN_FILES) {
        SDL_RenderDrawLine(renderer, cx - 6, cy - 4, cx - 2, cy - 4);
        SDL_RenderDrawLine(renderer, cx - 2, cy - 4, cx - 1, cy - 2);
        SDL_RenderDrawLine(renderer, cx - 1, cy - 2, cx + 6, cy - 2);
        SDL_RenderDrawLine(renderer, cx + 6, cy - 2, cx + 6, cy + 4);
        SDL_RenderDrawLine(renderer, cx + 6, cy + 4, cx - 6, cy + 4);
        SDL_RenderDrawLine(renderer, cx - 6, cy + 4, cx - 6, cy - 4);
        SDL_RenderDrawLine(renderer, cx - 4, cy, cx + 4, cy);
    } else if (id == SIM_BTN_ROTATE) {
        SDL_RenderDrawLine(renderer, cx - 4, cy + 3, cx - 4, cy);
        SDL_RenderDrawLine(renderer, cx - 4, cy, cx - 1, cy - 4);
        SDL_RenderDrawLine(renderer, cx - 1, cy - 4, cx + 3, cy - 4);
        SDL_RenderDrawLine(renderer, cx + 3, cy - 4, cx + 6, cy - 1);
        SDL_RenderDrawLine(renderer, cx + 6, cy - 1, cx + 6, cy + 2);
        SDL_RenderDrawLine(renderer, cx + 3, cy, cx + 6, cy + 3);
        SDL_RenderDrawLine(renderer, cx + 6, cy + 3, cx + 8, cy + 1);
    } else if (id == SIM_BTN_CLOSE) {
        SDL_RenderDrawLine(renderer, cx - 4, cy - 4, cx + 4, cy + 4);
        SDL_RenderDrawLine(renderer, cx - 3, cy - 4, cx + 5, cy + 4);
        SDL_RenderDrawLine(renderer, cx + 4, cy - 4, cx - 4, cy + 4);
        SDL_RenderDrawLine(renderer, cx + 5, cy - 4, cx - 3, cy + 4);
    }
}

static void draw_button(SDL_Renderer *renderer, const SimChassisButton *btn, bool powered)
{
    SDL_Rect r = btn->rect;
    if (btn->pressed) {
        r.y += 1;
        r.x += 1;
    }
    bool is_util = (btn->id == SIM_BTN_ROTATE || btn->id == SIM_BTN_FILES || btn->id == SIM_BTN_CLOSE);
    bool is_vert = (r.h > r.w);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (is_util) {
        SDL_Rect shadow = {r.x + 1, r.y + 2, r.w, r.h};
        SDL_SetRenderDrawColor(renderer, 8, 10, 12, 180);
        SDL_RenderFillRect(renderer, &shadow);
        if (btn->pressed) {
            SDL_SetRenderDrawColor(renderer, 36, 40, 48, 255);
        } else if (btn->id == SIM_BTN_CLOSE) {
            SDL_SetRenderDrawColor(renderer, 52, 56, 66, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 56, 62, 72, 255);
        }
        SDL_RenderFillRect(renderer, &r);
        if (!btn->pressed) {
            SDL_SetRenderDrawColor(renderer, 108, 118, 134, 255);
            SDL_RenderDrawLine(renderer, r.x + 1, r.y + 1, r.x + r.w - 2, r.y + 1);
        }
        SDL_SetRenderDrawColor(renderer, 84, 92, 104, 255);
        SDL_RenderDrawRect(renderer, &r);
    } else {
        if (btn->pressed) {
            SDL_SetRenderDrawColor(renderer, 24, 28, 36, 220);
        } else {
            SDL_SetRenderDrawColor(renderer, 44, 50, 60, 180);
        }
        SDL_RenderFillRect(renderer, &r);
        if (!btn->pressed) {
            SDL_SetRenderDrawColor(renderer, 130, 142, 160, 200);
            SDL_RenderDrawLine(renderer, r.x + 1, r.y + 1, r.x + r.w - 2, r.y + 1);
        }
        SDL_SetRenderDrawColor(renderer, 76, 84, 96, 220);
        SDL_RenderDrawRect(renderer, &r);
    }

    SDL_Color fg = btn->pressed ? (SDL_Color){135, 144, 158, 255} : (SDL_Color){232, 236, 244, 255};
    int cx = r.x + r.w / 2;
    int cy = (btn->id == SIM_BTN_CLOSE) ? (r.y + r.h / 2) : (is_vert ? (r.y + r.h / 2 - 8) : (r.y + r.h / 2 - 5));

    SDL_SetRenderDrawColor(renderer, fg.r, fg.g, fg.b, fg.a);
    draw_button_icon(renderer, btn->id, cx, cy);

    if (btn->label != NULL && btn->id != SIM_BTN_CLOSE) {
        int len = (int)strlen(btn->label);
        int tw = len * 6 - 1;
        int tx = cx - tw / 2;
        int ty = is_vert ? (cy + 13) : (cy + 7);
        draw_text5x7(renderer, btn->label, tx, ty, fg);
    }

    if (btn->id == SIM_BTN_POWER) {
        int led_x, led_y;
        if (is_vert) {
            led_x = r.x + r.w / 2;
            led_y = r.y + 6;
        } else {
            led_x = r.x + r.w - 10;
            led_y = r.y + r.h / 2;
        }
        if (powered) {
            SDL_SetRenderDrawColor(renderer, 60, 230, 90, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 35, 52, 40, 255);
        }
        SDL_Rect led_rect = {led_x - 3, led_y - 3, 6, 6};
        SDL_RenderFillRect(renderer, &led_rect);
    }
}

static void show_panel(void)
{
    if (display.renderer == NULL) {
        return;
    }
    SDL_SetRenderDrawColor(display.renderer, 16, 18, 22, 255);
    SDL_RenderClear(display.renderer);

    int rot = display.rotation % 4;
    double angle = (double)rot * 90.0;
    SDL_Rect shadow = {display.casing_draw_rect.x + 3, display.casing_draw_rect.y + 4,
                       display.casing_draw_rect.w, display.casing_draw_rect.h};
    SDL_SetRenderDrawColor(display.renderer, 6, 8, 10, 255);
    SDL_RenderFillRect(display.renderer, &shadow);
    if (display.casing_texture != NULL) {
        SDL_RenderCopyEx(display.renderer, display.casing_texture, NULL,
                         &display.casing_draw_rect, angle, NULL, SDL_FLIP_NONE);
    } else {
        SDL_SetRenderDrawColor(display.renderer, 38, 42, 48, 255);
        SDL_RenderFillRect(display.renderer, &display.casing_draw_rect);
        SDL_SetRenderDrawColor(display.renderer, 70, 76, 86, 255);
        SDL_RenderDrawRect(display.renderer, &display.casing_draw_rect);
    }

    if (display.powered) {
        if (display.texture != NULL) {
            int base_rot = 0;
            if (display.hal_rotation == HAL_DISPLAY_PORTRAIT) {
                base_rot = 1;
            } else if (display.hal_rotation == HAL_DISPLAY_PORTRAIT_FLIPPED) {
                base_rot = 3;
            }
            int rel_rot = (display.rotation - base_rot + 4) % 4;
            double tex_angle = (double)rel_rot * 90.0;
            int s = display.scale > 0 ? display.scale : 2;
            int tex_w = display.width * s;
            int tex_h = display.height * s;
            SDL_Rect tex_rect = {
                (int)(display.screen_center_x - (float)tex_w * 0.5f),
                (int)(display.screen_center_y - (float)tex_h * 0.5f),
                tex_w,
                tex_h
            };
            Uint8 intensity = brightness_intensity(display.brightness);
            SDL_SetTextureColorMod(display.texture, intensity, intensity, intensity);
            SDL_RenderCopyEx(display.renderer, display.texture, NULL,
                             &tex_rect, tex_angle, NULL, SDL_FLIP_NONE);
        }
    } else {
        SDL_SetRenderDrawColor(display.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(display.renderer, &display.hit_screen_rect);
    }

    for (int i = 0; i < SIM_BTN_COUNT; i++) {
        draw_button(display.renderer, &display.buttons[i], display.powered);
    }

    const char *badge = "SNOWSKY ECHO MINI";
    int badge_len = (int)strlen(badge);
    int bx = (display.casing_draw_rect.x + display.casing_draw_rect.w / 2) - (badge_len * 6) / 2;
    int by = display.win_height - 12;
    draw_text5x7(display.renderer, badge, bx, by, (SDL_Color){100, 108, 120, 255});
    SDL_RenderPresent(display.renderer);
}

void hal_display_present(void)
{
    if (display.texture == NULL) {
        return;
    }
    SDL_UpdateTexture(display.texture, NULL, display.panel, PANEL_ROW_BYTES);
    show_panel();
}

void hal_display_set_power(bool on)
{
    display.powered = on;
    show_panel();
}

void hal_display_set_brightness(uint8_t level)
{
    display.brightness = level;
    show_panel();
}
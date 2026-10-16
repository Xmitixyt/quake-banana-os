/*
 * vid_banana.c - Banana OS video & multi-key input driver for Quake
 */

#include "quakedef.h"
#include "d_local.h"
#include <banana.h>
#include <stdint.h>

viddef_t vid; // global video state

#define VID_W 320
#define VID_H 240
#define DEF_WIN_W 4000
#define DEF_WIN_H 4000

#define SURFCACHE_SIZE (2 * 1024 * 1024)

static byte vid_buffer[VID_W * VID_H];
static short zbuffer[VID_W * VID_H];
static byte fallback_surfcache[512 * 1024];
static byte *dyn_surfcache = NULL;

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
static unsigned int palette32[256];
static uint64_t palette64[256];

static bwin_t win;
static int win_opened = 0;
static int g_windowed = 0;

/* ──────────────── Cursor Hiding & Window Lock (Ring 0) ──────────────── */

typedef struct {
    uint8_t  bus, dev, fn;
    uint16_t vendor, device;
    uint8_t  class_code, subclass, prog_if;
    uint8_t  irq_line;
} k_pci_dev_t;

typedef struct k_gpu k_gpu_t;
struct k_gpu {
    char        name[64];
    const char *driver;
    k_pci_dev_t pci;
    uint32_t    vram;
    void       *modes;
    void       *set_mode;
    void       *flush;
    int       (*cursor_image)(k_gpu_t *g, const uint32_t *argb, int w, int h, int hot_x, int hot_y);
    void      (*cursor_move)(k_gpu_t *g, int x, int y, int visible);
    int         cursor_irq_safe;
    void       *wait_vblank;
    void       *backlight;
    void       *info;
    void       *priv;
};

typedef struct {
    int            used;
    int            owner;
    char           title[64];
    int            x, y;
    int            cw, ch;
    uint32_t      *px;
    uint32_t      *shown;
    banana_event_t q[64];
    int            qh, qt;
    int            dragging, ddx, ddy;
    int            close_clicks;
    int            min;
    int            last_mx, last_my, last_left, inside_down;
    uint32_t       gen;
    int            resizable, min_w, min_h;
    int            resizing, rdx, rdy;
} k_awin_t;

static k_awin_t *g_quake_awin = NULL;

typedef struct {
    volatile uint8_t *addr;
    uint8_t           orig;
} byte_patch_t;

#define MAX_CURSOR_PATCHES 512
static byte_patch_t g_patches[MAX_CURSOR_PATCHES];
static int          g_num_patches = 0;

static k_gpu_t *g_patched_gpu = NULL;
static int    (*g_orig_cursor_image)(k_gpu_t *g, const uint32_t *argb, int w, int h, int hot_x, int hot_y) = NULL;
static void   (*g_orig_cursor_move)(k_gpu_t *g, int x, int y, int visible) = NULL;
static int      g_orig_cursor_irq_safe = 0;
static int      g_cursor_hidden = 0;

static int noop_gpu_cursor_image(k_gpu_t *g, const uint32_t *argb, int w, int h, int hot_x, int hot_y)
{
    (void)g; (void)argb; (void)w; (void)h; (void)hot_x; (void)hot_y;
    return 0;
}

static void noop_gpu_cursor_move(k_gpu_t *g, int x, int y, int visible)
{
    (void)g; (void)x; (void)y; (void)visible;
}

static void add_byte_patch(volatile uint8_t *addr, uint8_t new_val)
{
    if (g_num_patches < MAX_CURSOR_PATCHES) {
        g_patches[g_num_patches].addr = addr;
        g_patches[g_num_patches].orig = *addr;
        g_num_patches++;
    }
    *addr = new_val;
}

static int is_cursor_row_str(const uint8_t *s, int expected_len)
{
    for (int i = 0; i < expected_len; i++) {
        uint8_t c = s[i];
        if (c != 'B' && c != 'W' && c != '.') return 0;
    }
    return s[expected_len] == 0;
}

static void Quake_RestoreCursor(void)
{
    if (!g_cursor_hidden) return;
    g_cursor_hidden = 0;

    for (int i = g_num_patches - 1; i >= 0; i--) {
        *(g_patches[i].addr) = g_patches[i].orig;
    }
    g_num_patches = 0;

    if (g_patched_gpu) {
        g_patched_gpu->cursor_image = g_orig_cursor_image;
        g_patched_gpu->cursor_move = g_orig_cursor_move;
        g_patched_gpu->cursor_irq_safe = g_orig_cursor_irq_safe;
        if (g_orig_cursor_move) {
            g_orig_cursor_move(g_patched_gpu, 200, 200, 1);
        }
        g_patched_gpu = NULL;
    }
}

static void Quake_HideCursor(void)
{
    if (g_cursor_hidden || COM_CheckParm("-showcursor")) return;
    g_cursor_hidden = 1;

    uint8_t *kstart = (uint8_t *)(uintptr_t)0x00100000;
    uint8_t *kend   = (uint8_t *)(uintptr_t)0x00400000;

    /* 1. Patch CUR_ARROW_BITS and CUR_HOURGLASS_BITS in kernel .rodata
     *    Replacing 'B' and 'W' with '.' makes both software cursor_bits()
     *    (including the 1 kHz timer ISR gui_cursor_tick) and shape_image()
     *    100% transparent on all architectures and compiler optimization levels. */
    const char *needle = "BWWWWWWBBBBB";
    for (uint8_t *p = kstart; p < kend - 64; p++) {
        if (p[0] == 'B' && memcmp(p, needle, 13) == 0) {
            uint8_t *r0 = (p - 512 > kstart) ? (p - 512) : kstart;
            uint8_t *r1 = (p + 512 < kend - 16) ? (p + 512) : (kend - 16);
            for (uint8_t *s = r0; s < r1; s++) {
                int len = 0;
                if (is_cursor_row_str(s, 12)) len = 12;
                else if (is_cursor_row_str(s, 9)) len = 9;
                if (len > 0) {
                    for (int k = 0; k < len; k++) {
                        if (s[k] == 'B' || s[k] == 'W') {
                            add_byte_patch((volatile uint8_t *)&s[k], '.');
                        }
                    }
                    s += len;
                }
            }
            break;
        }
    }

    /* 2. Disable Hardware GPU Cursor (VBoxSVGA, VMware SVGA II, virtio-gpu, Intel) */
    static const uint32_t empty_cur[32 * 32] = { 0 };
    for (uint8_t *p = kstart; p < kend - sizeof(k_gpu_t); p += 4) {
        k_gpu_t *g = (k_gpu_t *)p;
        if (g->pci.class_code != 0x03) continue;
        uintptr_t drv = (uintptr_t)g->driver;
        uintptr_t sm  = (uintptr_t)g->set_mode;
        if (drv < 0x00100000 || drv >= 0x00380000) continue;
        if (sm  < 0x00100000 || sm  >= 0x00380000) continue;
        const char *dname = (const char *)drv;
        if (strcmp(dname, "vmsvga") == 0 || strcmp(dname, "virtio") == 0 ||
            strcmp(dname, "intel") == 0  || strcmp(dname, "bga") == 0) {
            uintptr_t cm = (uintptr_t)g->cursor_move;
            if (cm >= 0x00100000 && cm < 0x00380000) {
                g_patched_gpu = g;
                g_orig_cursor_image = g->cursor_image;
                g_orig_cursor_move = g->cursor_move;
                g_orig_cursor_irq_safe = g->cursor_irq_safe;

                g->cursor_irq_safe = 0;
                if (g_orig_cursor_image) {
                    g_orig_cursor_image(g, empty_cur, 32, 32, 0, 0);
                }
                g_orig_cursor_move(g, 0, 0, 0);
                g->cursor_image = noop_gpu_cursor_image;
                g->cursor_move = noop_gpu_cursor_move;
                break;
            }
        }
    }

    /* 3. Also patch draw_cursor prologue if matched (fallback for older kernels) */
    static const uint8_t sig_dc32[8] = {0x55, 0x57, 0x56, 0x31, 0xF6, 0x53, 0x89, 0xD3};
    static const uint8_t sig_dc64[8] = {0x41, 0x56, 0x41, 0x89, 0xFE, 0x41, 0x55, 0x45};
    for (uint8_t *p = kstart; p < (uint8_t *)(uintptr_t)0x00280000; p++) {
        if (memcmp(p, sig_dc32, 8) == 0 || memcmp(p, sig_dc64, 8) == 0) {
            add_byte_patch((volatile uint8_t *)p, 0xC3);
            break;
        }
    }

    atexit(Quake_RestoreCursor);
}

static void Quake_FindAppWin(void)
{
    g_quake_awin = NULL;
    if (!win_opened || !win.px) return;

    uint8_t *kstart = (uint8_t *)(uintptr_t)0x00100000;
    uint8_t *kend   = (uint8_t *)(uintptr_t)0x00450000;
    for (uint8_t *p = kstart; p < kend - sizeof(k_awin_t); p += 4) {
        k_awin_t *a = (k_awin_t *)p;
        if (a->used == 1 && a->cw == win.w && a->ch == win.h && a->px == win.px) {
            if (memcmp(a->title, "Quake - Banana OS", 17) == 0) {
                g_quake_awin = a;
                if (!g_windowed) {
                    a->x = 0;
                    a->y = 0;
                    a->dragging = 0;
                    a->resizing = 0;
                    a->inside_down = 1;
                }
                break;
            }
        }
    }
}

void VID_SetPalette(unsigned char *palette)
{
    for (int i = 0; i < 256; i++) {
        unsigned int r = palette[i * 3 + 0];
        unsigned int g = palette[i * 3 + 1];
        unsigned int b = palette[i * 3 + 2];
        unsigned int c = (r << 16) | (g << 8) | b;
        palette32[i] = c;
        d_8to24table[i] = c;
        palette64[i] = ((uint64_t)c << 32) | (uint64_t)c;
    }
}

void VID_ShiftPalette(unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char *palette)
{
    vid.maxwarpwidth = vid.width = vid.conwidth = VID_W;
    vid.maxwarpheight = vid.height = vid.conheight = VID_H;
    vid.aspect = ((float)VID_W / (float)VID_H);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
    vid.buffer = vid.conbuffer = vid_buffer;
    vid.rowbytes = vid.conrowbytes = VID_W;

    d_pzbuffer = zbuffer;

    dyn_surfcache = (byte *)malloc(SURFCACHE_SIZE);
    if (dyn_surfcache) {
        D_InitCaches(dyn_surfcache, SURFCACHE_SIZE);
    } else {
        D_InitCaches(fallback_surfcache, sizeof(fallback_surfcache));
    }

    VID_SetPalette(palette);

    g_windowed = COM_CheckParm("-window") ? 1 : 0;
    int req_w = g_windowed ? 640 : DEF_WIN_W;
    int req_h = g_windowed ? 480 : DEF_WIN_H;

    if (!win_opened) {
        if (bwin_open(&win, "Quake - Banana OS", req_w, req_h) == 0) {
            win_opened = 1;
            bwin_wheel(&win);
            if (g_windowed) {
                bwin_resizable(&win, VID_W, VID_H);
            }
            bwin_clear(&win, BANANA_BLACK);
            bwin_update(&win);
            Quake_FindAppWin();
            Quake_HideCursor();
        } else {
            Sys_Error("Failed to open Banana OS window for Quake");
        }
    }
}

void VID_Shutdown(void)
{
    Quake_RestoreCursor();
    g_quake_awin = NULL;
    if (win_opened) {
        bwin_close(&win);
        win_opened = 0;
    }
    if (dyn_surfcache) {
        free(dyn_surfcache);
        dyn_surfcache = NULL;
    }
}

static uint16_t g_xlut[4096];
static int      g_xlut_w = 0;

void VID_Update(vrect_t *rects)
{
    (void)rects;
    if (!win_opened || !win.px) return;

    int ww = win.w;
    int wh = win.h;

    if (ww == 640 && wh == 480) {
        // Fast 64-bit 2x nearest-neighbor upscale from 320x240 to 640x480
        for (int y = 0; y < VID_H; y++) {
            const byte *src = vid.buffer + y * VID_W;
            uint64_t *dst0 = (uint64_t *)(win.px + (y * 2) * 640);
            uint64_t *dst1 = (uint64_t *)(win.px + (y * 2 + 1) * 640);
            for (int x = 0; x < VID_W; x += 4) {
                uint64_t c0 = palette64[src[x + 0]];
                uint64_t c1 = palette64[src[x + 1]];
                uint64_t c2 = palette64[src[x + 2]];
                uint64_t c3 = palette64[src[x + 3]];
                dst0[0] = c0; dst0[1] = c1; dst0[2] = c2; dst0[3] = c3;
                dst1[0] = c0; dst1[1] = c1; dst1[2] = c2; dst1[3] = c3;
                dst0 += 4;
                dst1 += 4;
            }
        }
    } else if (ww == 320 && wh == 240) {
        // Fast 1x direct blit for 320x240 window
        const byte *src = vid.buffer;
        unsigned int *dst = win.px;
        int total = VID_W * VID_H;
        for (int i = 0; i < total; i += 4) {
            dst[i + 0] = palette32[src[i + 0]];
            dst[i + 1] = palette32[src[i + 1]];
            dst[i + 2] = palette32[src[i + 2]];
            dst[i + 3] = palette32[src[i + 3]];
        }
    } else if (ww > 0 && wh > 0) {
        // Fast fullscreen scaling with X lookup table and duplicate row 64-bit memcpy
        int clamp_w = (ww <= 4096) ? ww : 4096;
        if (clamp_w != g_xlut_w) {
            for (int x = 0; x < clamp_w; x++) {
                g_xlut[x] = (uint16_t)((x * VID_W) / ww);
            }
            g_xlut_w = clamp_w;
        }
        int prev_sy = -1;
        unsigned int *prev_dst = NULL;
        for (int y = 0; y < wh; y++) {
            int sy = (y * VID_H) / wh;
            unsigned int *dst = win.px + (size_t)y * (size_t)ww;
            if (sy == prev_sy && prev_dst) {
                memcpy(dst, prev_dst, (size_t)ww * 4);
            } else {
                const byte *src = vid.buffer + sy * VID_W;
                int x = 0;
                for (; x <= clamp_w - 4; x += 4) {
                    dst[x + 0] = palette32[src[g_xlut[x + 0]]];
                    dst[x + 1] = palette32[src[g_xlut[x + 1]]];
                    dst[x + 2] = palette32[src[g_xlut[x + 2]]];
                    dst[x + 3] = palette32[src[g_xlut[x + 3]]];
                }
                for (; x < clamp_w; x++) {
                    dst[x] = palette32[src[g_xlut[x]]];
                }
                prev_sy = sy;
                prev_dst = dst;
            }
        }
    }

    bwin_update(&win);
}

int VID_SetMode(int modenum, unsigned char *palette)
{
    (void)modenum;
    (void)palette;
    return 1;
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
    (void)x; (void)y; (void)pbitmap; (void)width; (void)height;
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x; (void)y; (void)width; (void)height;
}

void VID_HandlePause(qboolean pause)
{
    (void)pause;
}

/* ──────────────── Multi-Key & Mouse Input (SDK v14+) ──────────────── */

typedef struct {
    int bkey;
    int qkey;
} keymap_t;

static const keymap_t g_keytable[] = {
    // Letters (physical scancodes on US layout -> WASD / ZQSD work seamlessly)
    {'a', 'a'}, {'b', 'b'}, {'c', 'c'}, {'d', 'd'}, {'e', 'e'}, {'f', 'f'},
    {'g', 'g'}, {'h', 'h'}, {'i', 'i'}, {'j', 'j'}, {'k', 'k'}, {'l', 'l'},
    {'m', 'm'}, {'n', 'n'}, {'o', 'o'}, {'p', 'p'}, {'q', 'q'}, {'r', 'r'},
    {'s', 's'}, {'t', 't'}, {'u', 'u'}, {'v', 'v'}, {'w', 'w'}, {'x', 'x'},
    {'y', 'y'}, {'z', 'z'},
    // Digits
    {'0', '0'}, {'1', '1'}, {'2', '2'}, {'3', '3'}, {'4', '4'},
    {'5', '5'}, {'6', '6'}, {'7', '7'}, {'8', '8'}, {'9', '9'},
    // Punctuation
    {'-', '-'}, {'=', '='}, {'[', '['}, {']', ']'}, {';', ';'},
    {'\'', '\''}, {'`', '`'}, {'\\', '\\'}, {',', ','}, {'.', '.'}, {'/', '/'},
    // Action / navigation keys
    {' ', K_SPACE},
    {'\n', K_ENTER},
    {'\t', K_TAB},
    {'\b', K_BACKSPACE},
    {27, K_ESCAPE},
    {BANANA_KEY_UP, K_UPARROW},
    {BANANA_KEY_DOWN, K_DOWNARROW},
    {BANANA_KEY_LEFT, K_LEFTARROW},
    {BANANA_KEY_RIGHT, K_RIGHTARROW},
    {BANANA_KEY_HOME, K_HOME},
    {BANANA_KEY_END, K_END},
    {BANANA_KEY_PGUP, K_PGUP},
    {BANANA_KEY_PGDN, K_PGDN},
    {BANANA_KEY_DELETE, K_DEL},
    // Modifiers (multi-key sprint / crouch / strafe)
    {BANANA_KEY_SHIFT, K_SHIFT},
    {BANANA_KEY_CTRL, K_CTRL},
    {BANANA_KEY_ALT, K_ALT},
    // Function keys
    {BANANA_KEY_F1 + 0, K_F1},   {BANANA_KEY_F1 + 1, K_F2},   {BANANA_KEY_F1 + 2, K_F3},
    {BANANA_KEY_F1 + 3, K_F4},   {BANANA_KEY_F1 + 4, K_F5},   {BANANA_KEY_F1 + 5, K_F6},
    {BANANA_KEY_F1 + 6, K_F7},   {BANANA_KEY_F1 + 7, K_F8},   {BANANA_KEY_F1 + 8, K_F9},
    {BANANA_KEY_F1 + 9, K_F10},  {BANANA_KEY_F1 + 10, K_F11}, {BANANA_KEY_F12, K_F12},
};

#define NUM_KEYMAP (sizeof(g_keytable) / sizeof(g_keytable[0]))
static unsigned char g_prev_down[NUM_KEYMAP];

// Fallback mapper for kernels < v14
static int MapBananaKey(int k)
{
    if (k >= 'A' && k <= 'Z') return k + 32;
    if (k >= 32 && k <= 126) return k;
    switch (k) {
    case 10:
    case 13: return K_ENTER;
    case 27: return K_ESCAPE;
    case 8:
    case 127: return K_BACKSPACE;
    case 9: return K_TAB;
    case BANANA_KEY_UP: return K_UPARROW;
    case BANANA_KEY_DOWN: return K_DOWNARROW;
    case BANANA_KEY_LEFT: return K_LEFTARROW;
    case BANANA_KEY_RIGHT: return K_RIGHTARROW;
    case BANANA_KEY_HOME: return K_HOME;
    case BANANA_KEY_END: return K_END;
    case BANANA_KEY_PGUP: return K_PGUP;
    case BANANA_KEY_PGDN: return K_PGDN;
    case BANANA_KEY_DELETE: return K_DEL;
    }
    if (k >= BANANA_KEY_F1 && k <= BANANA_KEY_F12)
        return K_F1 + (k - BANANA_KEY_F1);
    return 0;
}

static int mouse_x_accum = 0;
static int mouse_y_accum = 0;
static int last_mx = -1, last_my = -1;

void Sys_SendKeyEvents(void)
{
    if (!win_opened) return;

    if (g_quake_awin && !g_windowed) {
        g_quake_awin->x = 0;
        g_quake_awin->y = 0;
        g_quake_awin->dragging = 0;
        g_quake_awin->resizing = 0;
        g_quake_awin->min = 0;
        g_quake_awin->inside_down = 1;
    }

    int has_multikey = (__banana && __banana->version >= 14 && __banana->key_down != NULL);

    // 1. Poll real-time hardware multi-key state (SDK API v14+)
    if (has_multikey) {
        for (unsigned int i = 0; i < NUM_KEYMAP; i++) {
            unsigned char down = banana_key_down(g_keytable[i].bkey) ? 1 : 0;
            if (down != g_prev_down[i]) {
                g_prev_down[i] = down;
                Key_Event(g_keytable[i].qkey, down ? true : false);
            }
        }
    }

    // 2. Poll window mouse, wheel, resize, and close events
    banana_event_t ev;
    while (bwin_event(&win, &ev)) {
        switch (ev.type) {
        case BANANA_EV_KEY:
            if (!has_multikey) {
                int qk = MapBananaKey(ev.key);
                if (qk > 0 && qk < 256) {
                    Key_Event(qk, true);
                    Key_Event(qk, false);
                }
            }
            break;
        case BANANA_EV_MOUSE_DOWN:
            if (ev.button == 1) Key_Event(K_MOUSE1, true);
            else if (ev.button == 2) Key_Event(K_MOUSE2, true);
            else if (ev.button == 3) Key_Event(K_MOUSE3, true);
            break;
        case BANANA_EV_MOUSE_UP:
            if (ev.button == 1) Key_Event(K_MOUSE1, false);
            else if (ev.button == 2) Key_Event(K_MOUSE2, false);
            else if (ev.button == 3) Key_Event(K_MOUSE3, false);
            break;
        case BANANA_EV_MOUSE_MOVE:
            if (last_mx >= 0) {
                int dx = ev.x - last_mx;
                int dy = ev.y - last_my;
                if (dx > -300 && dx < 300 && dy > -300 && dy < 300) {
                    mouse_x_accum += dx;
                    mouse_y_accum += dy;
                }
            }
            last_mx = ev.x;
            last_my = ev.y;
            break;
        case BANANA_EV_WHEEL:
            if (ev.button < 0) {
                Key_Event(K_MWHEELUP, true);
                Key_Event(K_MWHEELUP, false);
            } else if (ev.button > 0) {
                Key_Event(K_MWHEELDOWN, true);
                Key_Event(K_MWHEELDOWN, false);
            }
            break;
        case BANANA_EV_FOCUS:
        case BANANA_EV_RESIZE:
            last_mx = -1;
            last_my = -1;
            break;
        case BANANA_EV_CLOSE:
            Quake_RestoreCursor();
            Host_Shutdown();
            exit(0);
            break;
        }
    }
}

static cvar_t m_filter = {"m_filter", "1"};

void IN_Init(void)
{
    Cvar_RegisterVariable(&m_filter);
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
}

void IN_ClearStates(void)
{
    mouse_x_accum = 0;
    mouse_y_accum = 0;
    last_mx = -1;
    last_my = -1;
    for (unsigned int i = 0; i < NUM_KEYMAP; i++) {
        if (g_prev_down[i]) {
            g_prev_down[i] = 0;
            Key_Event(g_keytable[i].qkey, false);
        }
    }
}

void IN_Move(usercmd_t *cmd)
{
    float mx = (float)mouse_x_accum;
    float my = (float)mouse_y_accum;
    mouse_x_accum = 0;
    mouse_y_accum = 0;

    static float old_mx = 0, old_my = 0;
    if (m_filter.value) {
        mx = (mx + old_mx) * 0.5f;
        my = (my + old_my) * 0.5f;
    }
    old_mx = mx;
    old_my = my;

    mx *= sensitivity.value;
    my *= sensitivity.value;

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
        cmd->sidemove += m_side.value * mx;
    else
        cl.viewangles[YAW] -= m_yaw.value * mx;

    if (in_mlook.state & 1)
        V_StopPitchDrift();

    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * my;
        if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
    } else {
        cmd->forwardmove -= m_forward.value * my;
    }
}

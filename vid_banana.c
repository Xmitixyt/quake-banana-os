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
#define WIN_W 640
#define WIN_H 480

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

    if (!win_opened) {
        if (bwin_open(&win, "Quake - Banana OS", WIN_W, WIN_H) == 0) {
            win_opened = 1;
            bwin_wheel(&win);
            bwin_resizable(&win, VID_W, VID_H);
            bwin_clear(&win, BANANA_BLACK);
            bwin_update(&win);
            bwin_capture_mouse(&win, 1);
        } else {
            Sys_Error("Failed to open Banana OS window for Quake");
        }
    }
}

void VID_Shutdown(void)
{
    if (win_opened) {
        bwin_capture_mouse(&win, 0);
        bwin_close(&win);
        win_opened = 0;
    }
    if (dyn_surfcache) {
        free(dyn_surfcache);
        dyn_surfcache = NULL;
    }
}

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
        // Arbitrary window size fixed-point nearest-neighbor scaling
        uint32_t step_x = (VID_W << 16) / (uint32_t)ww;
        uint32_t step_y = (VID_H << 16) / (uint32_t)wh;
        uint32_t sy = 0;
        for (int y = 0; y < wh; y++) {
            const byte *src = vid.buffer + (sy >> 16) * VID_W;
            unsigned int *dst = win.px + y * ww;
            uint32_t sx = 0;
            for (int x = 0; x < ww; x++) {
                dst[x] = palette32[src[sx >> 16]];
                sx += step_x;
            }
            sy += step_y;
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
            bwin_capture_mouse(&win, 1);
            if (ev.button == 1) Key_Event(K_MOUSE1, true);
            else if (ev.button == 2) Key_Event(K_MOUSE2, true);
            else if (ev.button == 3) Key_Event(K_MOUSE3, true);
            break;
        case BANANA_EV_MOUSE_UP:
            if (ev.button == 1) Key_Event(K_MOUSE1, false);
            else if (ev.button == 2) Key_Event(K_MOUSE2, false);
            else if (ev.button == 3) Key_Event(K_MOUSE3, false);
            break;
        case BANANA_EV_MOUSE_REL:
            mouse_x_accum += ev.x;
            mouse_y_accum += ev.y;
            break;
        case BANANA_EV_MOUSE_MOVE:
            // Fallback for kernels without relative mouse capture
            if (last_mx >= 0) {
                mouse_x_accum += (ev.x - last_mx);
                mouse_y_accum += (ev.y - last_my);
            }
            last_mx = ev.x;
            last_my = ev.y;
            break;
        case BANANA_EV_CAPTURE:
            last_mx = -1;
            last_my = -1;
            break;
        case BANANA_EV_FOCUS:
            if (ev.x) {
                bwin_capture_mouse(&win, 1);
            }
            last_mx = -1;
            last_my = -1;
            break;
        case BANANA_EV_WHEEL:
            if (ev.button < 0 || ev.y < 0) {
                Key_Event(K_MWHEELUP, true);
                Key_Event(K_MWHEELUP, false);
            } else if (ev.button > 0 || ev.y > 0) {
                Key_Event(K_MWHEELDOWN, true);
                Key_Event(K_MWHEELDOWN, false);
            }
            break;
        case BANANA_EV_RESIZE:
            last_mx = -1;
            last_my = -1;
            break;
        case BANANA_EV_CLOSE:
            bwin_capture_mouse(&win, 0);
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

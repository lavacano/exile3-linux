#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

typedef int (*real_ioctl_t)(int fd, unsigned long request, ...);
typedef int (*real_msgbox_t)(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType);

static real_ioctl_t real_ioctl = NULL;
static real_msgbox_t real_msgbox = NULL;

static volatile uint8_t *cached_sf = NULL;
static void *cached_as = NULL;
static int sf_resolved = 0;

static inline void reset_sounds_fucked(void) {
    if (__builtin_expect(!sf_resolved, 0)) {
        cached_sf = (volatile uint8_t *)dlsym(RTLD_DEFAULT, "sounds_fucked");
        cached_as = dlsym(RTLD_DEFAULT, "always_asynch");
        sf_resolved = 1;
    }
    if (cached_sf) {
        *cached_sf = 0;
    }
    /* Force all 100 sound effects to play asynchronously so the game engine never freezes */
    if (cached_as && ((uint8_t *)cached_as)[0] != 1) {
        memset(cached_as, 1, 100);
    }
}

__attribute__((visibility("default"))) int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    if (request == SNDCTL_DSP_GETOSPACE) {
        struct audio_buf_info *info = (struct audio_buf_info *)arg;
        if (info) {
            /* Force available buffer space so Exile III never starves or triggers sounds_fucked */
            info->fragsize = 2048;
            info->fragstotal = 16;
            info->fragments = 16;
            info->bytes = 32768;
        }
        reset_sounds_fucked();
        return 0;
    }

    /* Prevent padsp from blocking the main game engine loop on sound playback */
    if (request == SNDCTL_DSP_NONBLOCK || request == SNDCTL_DSP_POST || request == SNDCTL_DSP_SYNC) {
        reset_sounds_fucked();
        return 0;
    }

    if (request == SNDCTL_DSP_RESET) {
        reset_sounds_fucked();
    }

    if (__builtin_expect(!real_ioctl, 0)) {
        real_ioctl = (real_ioctl_t)dlsym(RTLD_NEXT, "ioctl");
    }

    if (real_ioctl) {
        return real_ioctl(fd, request, arg);
    }

    return 0;
}

#include <sched.h>
#include <time.h>
#include <sys/prctl.h>

__attribute__((visibility("default"))) void Sleep(uint32_t dwMilliseconds) {
    if (dwMilliseconds == 0) {
        sched_yield();
        return;
    }
    struct timespec ts;
    ts.tv_sec = dwMilliseconds / 1000;
    ts.tv_nsec = (dwMilliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

__attribute__((visibility("default"))) void LogError(int err, void *ptr) {
}

__attribute__((visibility("default"))) void LogParamError(int err, void *ptr, ...) {
}

__attribute__((visibility("default"))) int logstr(int level, ...) {
    return 0;
}

#include <stdlib.h>
#include <emmintrin.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* =========================================================================
 * 2. FreeType 2 Dynamic Anti-Aliasing & Hinted Text Renderer
 * ========================================================================= */

typedef int (*real_xdrawstring_t)(Display *, Drawable, GC, int, int, const char *, int);
typedef int (*real_xdrawimagestring_t)(Display *, Drawable, GC, int, int, const char *, int);

static real_xdrawstring_t real_xdrawstring = NULL;
static real_xdrawimagestring_t real_xdrawimagestring = NULL;

/* FreeType 2 Minimal Type Definitions */
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef struct FT_GlyphSlotRec_ *FT_GlyphSlot;

typedef struct FT_Bitmap_ {
    unsigned int rows;
    unsigned int width;
    int pitch;
    unsigned char *buffer;
    unsigned short num_grays;
    unsigned char pixel_mode;
    unsigned char palette_mode;
    void *palette;
} FT_Bitmap;

typedef struct FT_Vector_ {
    long x;
    long y;
} FT_Vector;

typedef struct FT_Glyph_Metrics_ {
    long width;
    long height;
    long horiBearingX;
    long horiBearingY;
    long horiAdvance;
    long vertBearingX;
    long vertBearingY;
    long vertAdvance;
} FT_Glyph_Metrics;

typedef struct {
    void *data;
    void *finalizer;
} FT_Generic;

typedef struct {
    long xMin, yMin, xMax, yMax;
} FT_BBox;

struct FT_GlyphSlotRec_ {
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot next;
    unsigned int glyph_index;
    FT_Generic generic;
    FT_Glyph_Metrics metrics;
    long linearHoriAdvance;
    long linearVertAdvance;
    FT_Vector advance;
    int format;
    FT_Bitmap bitmap;
    int bitmap_left;
    int bitmap_top;
};

struct FT_FaceRec_ {
    long num_faces;
    long face_index;
    long face_flags;
    long style_flags;
    long num_glyphs;
    char *family_name;
    char *style_name;
    int num_fixed_sizes;
    void *available_sizes;
    int num_charmaps;
    void *charmaps;
    FT_Generic generic;
    FT_BBox bbox;
    unsigned short units_per_EM;
    short ascender;
    short descender;
    short height;
    short max_advance_width;
    short max_advance_height;
    short underline_position;
    short underline_thickness;
    FT_GlyphSlot glyph;
    void *size;
    void *charmap;
};

#define FT_LOAD_DEFAULT        0x0
#define FT_LOAD_RENDER         0x4
#define FT_LOAD_TARGET_NORMAL  0x0
#define FT_LOAD_TARGET_LCD     0x30000
#define FT_PIXEL_MODE_GRAY     2
#define FT_PIXEL_MODE_LCD      5

typedef int (*FT_Init_FreeType_t)(FT_Library *);
typedef int (*FT_Done_FreeType_t)(FT_Library);
typedef int (*FT_New_Face_t)(FT_Library, const char *, long, FT_Face *);
typedef int (*FT_Done_Face_t)(FT_Face);
typedef int (*FT_Set_Pixel_Sizes_t)(FT_Face, unsigned int, unsigned int);
typedef int (*FT_Set_Char_Size_t)(FT_Face, long, long, unsigned int, unsigned int);
typedef int (*FT_Load_Char_t)(FT_Face, unsigned long, int);

static FT_Init_FreeType_t p_FT_Init_FreeType = NULL;
static FT_Done_FreeType_t p_FT_Done_FreeType = NULL;
static FT_New_Face_t p_FT_New_Face = NULL;
static FT_Done_Face_t p_FT_Done_Face = NULL;
static FT_Set_Pixel_Sizes_t p_FT_Set_Pixel_Sizes = NULL;
static FT_Set_Char_Size_t p_FT_Set_Char_Size = NULL;
static FT_Load_Char_t p_FT_Load_Char = NULL;

static FT_Library ft_lib = NULL;
static FT_Face ft_face_regular = NULL;
static FT_Face ft_face_bold = NULL;
static int ft_initialized = 0;
static int ft_failed = 0;

static const char *candidate_fonts_regular[] = {
    "/game/fonts/LiberationSans-Regular.ttf",
    "./fonts/LiberationSans-Regular.ttf",
    "/home/lavac/exile3-linux-1/fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    NULL
};

static const char *candidate_fonts_bold[] = {
    "/game/fonts/LiberationSans-Bold.ttf",
    "./fonts/LiberationSans-Bold.ttf",
    "/home/lavac/exile3-linux-1/fonts/LiberationSans-Bold.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    NULL
};

static void init_freetype(void) {
    if (ft_initialized || ft_failed) return;

    void *ft_handle = dlopen("libfreetype.so.6", RTLD_LAZY);
    if (!ft_handle) {
        ft_handle = dlopen("/usr/lib/i386-linux-gnu/libfreetype.so.6", RTLD_LAZY);
    }
    if (!ft_handle) {
        ft_handle = dlopen("/usr/lib32/libfreetype.so.6", RTLD_LAZY);
    }
    if (!ft_handle) {
        ft_failed = 1;
        return;
    }

    p_FT_Init_FreeType = (FT_Init_FreeType_t)dlsym(ft_handle, "FT_Init_FreeType");
    p_FT_Done_FreeType = (FT_Done_FreeType_t)dlsym(ft_handle, "FT_Done_FreeType");
    p_FT_New_Face = (FT_New_Face_t)dlsym(ft_handle, "FT_New_Face");
    p_FT_Done_Face = (FT_Done_Face_t)dlsym(ft_handle, "FT_Done_Face");
    p_FT_Set_Pixel_Sizes = (FT_Set_Pixel_Sizes_t)dlsym(ft_handle, "FT_Set_Pixel_Sizes");
    p_FT_Set_Char_Size = (FT_Set_Char_Size_t)dlsym(ft_handle, "FT_Set_Char_Size");
    p_FT_Load_Char = (FT_Load_Char_t)dlsym(ft_handle, "FT_Load_Char");

    if (!p_FT_Init_FreeType || !p_FT_New_Face || !p_FT_Set_Pixel_Sizes || !p_FT_Load_Char) {
        ft_failed = 1;
        return;
    }

    if (p_FT_Init_FreeType(&ft_lib) != 0) {
        ft_failed = 1;
        return;
    }

    for (int i = 0; candidate_fonts_regular[i]; i++) {
        if (access(candidate_fonts_regular[i], R_OK) == 0) {
            if (p_FT_New_Face(ft_lib, candidate_fonts_regular[i], 0, &ft_face_regular) == 0) {
                break;
            }
        }
    }

    for (int i = 0; candidate_fonts_bold[i]; i++) {
        if (access(candidate_fonts_bold[i], R_OK) == 0) {
            if (p_FT_New_Face(ft_lib, candidate_fonts_bold[i], 0, &ft_face_bold) == 0) {
                break;
            }
        }
    }

    if (!ft_face_regular) ft_face_regular = ft_face_bold;
    if (!ft_face_bold) ft_face_bold = ft_face_regular;

    if (!ft_face_regular) {
        ft_failed = 1;
        return;
    }

    ft_initialized = 1;
}

static inline void fast_fill_pixel(uint32_t *data, uint32_t pixel, int count) {
    if (count <= 0) return;
    if (pixel == 0) {
        memset(data, 0, count * sizeof(uint32_t));
        return;
    }
    __m128i val128 = _mm_set1_epi32((int)pixel);
    int i = 0;
    for (; i <= count - 16; i += 16) {
        _mm_storeu_si128((__m128i *)(data + i), val128);
        _mm_storeu_si128((__m128i *)(data + i + 4), val128);
        _mm_storeu_si128((__m128i *)(data + i + 8), val128);
        _mm_storeu_si128((__m128i *)(data + i + 12), val128);
    }
    for (; i <= count - 4; i += 4) {
        _mm_storeu_si128((__m128i *)(data + i), val128);
    }
    for (; i < count; i++) {
        data[i] = pixel;
    }
}

#define NUM_SIZE_SLOTS 32

typedef struct {
    int valid;
    int width;
    int rows;
    int pitch;
    int bitmap_left;
    int bitmap_top;
    int advance_x;
    unsigned char *buffer;
} CachedGlyph;

typedef struct {
    int in_use;
    int is_bold;
    int font_height;
    CachedGlyph glyphs[256];
} FontSizeCache;

static FontSizeCache size_caches[NUM_SIZE_SLOTS];
static int size_caches_count = 0;
static int8_t size_to_slot[2][48];
static int size_map_initialized = 0;

static inline void init_size_map(void) {
    if (__builtin_expect(!size_map_initialized, 0)) {
        memset(size_to_slot, -1, sizeof(size_to_slot));
        size_map_initialized = 1;
    }
}

static CachedGlyph *get_cached_glyph(int is_bold, int font_height, unsigned char c) {
    if (font_height <= 0 || font_height > 120) font_height = 12;
    int b_idx = is_bold ? 1 : 0;

    int slot_idx = -1;
    if (__builtin_expect(font_height < 48, 1)) {
        if (__builtin_expect(!size_map_initialized, 0)) {
            init_size_map();
        }
        slot_idx = size_to_slot[b_idx][font_height];
    }

    if (slot_idx < 0) {
        for (int i = 0; i < size_caches_count; i++) {
            if (size_caches[i].in_use &&
                size_caches[i].is_bold == is_bold &&
                size_caches[i].font_height == font_height) {
                slot_idx = i;
                break;
            }
        }
        if (slot_idx < 0) {
            if (size_caches_count < NUM_SIZE_SLOTS) {
                slot_idx = size_caches_count++;
            } else {
                slot_idx = 0;
                for (int ch = 0; ch < 256; ch++) {
                    if (size_caches[0].glyphs[ch].buffer) {
                        free(size_caches[0].glyphs[ch].buffer);
                        size_caches[0].glyphs[ch].buffer = NULL;
                    }
                }
                if (size_caches[0].font_height < 48) {
                    size_to_slot[size_caches[0].is_bold ? 1 : 0][size_caches[0].font_height] = -1;
                }
            }
            memset(&size_caches[slot_idx], 0, sizeof(FontSizeCache));
            size_caches[slot_idx].in_use = 1;
            size_caches[slot_idx].is_bold = is_bold;
            size_caches[slot_idx].font_height = font_height;
        }
        if (font_height < 48) {
            size_to_slot[b_idx][font_height] = (int8_t)slot_idx;
        }
    }

    CachedGlyph *cg = &size_caches[slot_idx].glyphs[c];
    if (__builtin_expect(!cg->valid, 0)) {
        FT_Face face = is_bold ? ft_face_bold : ft_face_regular;
        if (!face) face = ft_face_regular;
        if (!face) return NULL;

        static float font_width_scale = -1.0f;
        if (font_width_scale < 0.0f) {
            const char *env_scale = getenv("EXILE_FONT_WIDTH_SCALE");
            if (env_scale && *env_scale) {
                font_width_scale = (float)atof(env_scale);
                if (font_width_scale < 0.5f || font_width_scale > 1.5f) {
                    font_width_scale = 0.88f;
                }
            } else {
                font_width_scale = 0.88f; // Standard condensed MS Sans Serif proportion
            }
        }

        if (p_FT_Set_Char_Size) {
            long char_w = (long)(font_width_scale * font_height * 64.0f + 0.5f);
            long char_h = font_height * 64;
            p_FT_Set_Char_Size(face, char_w, char_h, 72, 72);
        } else if (p_FT_Set_Pixel_Sizes) {
            unsigned int pw = (unsigned int)(font_width_scale * font_height + 0.5f);
            p_FT_Set_Pixel_Sizes(face, pw, font_height);
        }

        if (p_FT_Load_Char(face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
            cg->valid = 1;
            return cg;
        }

        FT_GlyphSlot gslot = face->glyph;
        cg->width = gslot->bitmap.width;
        cg->rows = gslot->bitmap.rows;
        cg->pitch = gslot->bitmap.pitch;
        cg->bitmap_left = gslot->bitmap_left;
        cg->bitmap_top = gslot->bitmap_top;
        cg->advance_x = gslot->advance.x >> 6;

        if (cg->rows > 0 && cg->width > 0 && gslot->bitmap.buffer) {
            int sz = cg->rows * cg->width;
            cg->buffer = (unsigned char *)malloc(sz);
            if (cg->buffer) {
                for (int r = 0; r < cg->rows; r++) {
                    memcpy(cg->buffer + r * cg->width,
                           gslot->bitmap.buffer + r * cg->pitch,
                           cg->width);
                }
                cg->pitch = cg->width;
            }
        }
        cg->valid = 1;
    }
    return cg;
}

static void prewarm_font_cache(void) {
    static int prewarmed = 0;
    if (prewarmed || ft_failed || !ft_face_regular) return;
    prewarmed = 1;
    init_size_map();
    static const int sizes[] = { 9, 10, 11, 12, 13, 14, 16, 18, 20, 24 };
    for (int s = 0; s < 10; s++) {
        for (int bold = 0; bold < 2; bold++) {
            for (unsigned char c = 32; c <= 126; c++) {
                get_cached_glyph(bold, sizes[s], c);
            }
        }
    }
}

#define FONT_CACHE_SIZE 64
typedef struct {
    Font font_id;
    int size;
    int is_bold;
    int ascent;
} FontCacheEntry;

static FontCacheEntry font_cache[FONT_CACHE_SIZE];
static int font_cache_count = 0;

static void get_font_info(Display *dpy, Font font_id, int *size, int *is_bold, int *ascent) {
    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].font_id == font_id) {
            *size = font_cache[i].size;
            *is_bold = font_cache[i].is_bold;
            *ascent = font_cache[i].ascent;
            return;
        }
    }

    int f_size = 12;
    int f_bold = 0;
    int f_ascent = 10;

    XFontStruct *fs = XQueryFont(dpy, font_id);
    if (fs) {
        int total_height = fs->ascent + fs->descent;
        f_ascent = fs->ascent;
        if (total_height > 0) {
            f_size = total_height;
        }
        if (f_size >= 14) {
            f_bold = 1;
        }
        XFreeFontInfo(NULL, fs, 1);
    }

    if (font_cache_count < FONT_CACHE_SIZE) {
        font_cache[font_cache_count].font_id = font_id;
        font_cache[font_cache_count].size = f_size;
        font_cache[font_cache_count].is_bold = f_bold;
        font_cache[font_cache_count].ascent = f_ascent;
        font_cache_count++;
    } else {
        font_cache[0].font_id = font_id;
        font_cache[0].size = f_size;
        font_cache[0].is_bold = f_bold;
        font_cache[0].ascent = f_ascent;
    }

    *size = f_size;
    *is_bold = f_bold;
    *ascent = f_ascent;
}

static int x_error_trap = 0;
static XErrorHandler twin_saved_error_handler = NULL;

static int trap_x_errors(Display *d, XErrorEvent *e) {
    x_error_trap = 1;
    return 0;
}

static int quiet_x_error_handler(Display *d, XErrorEvent *e) {
    if (e && e->error_code == BadMatch) {
        // Filter non-fatal BadMatch warnings from Willows Twin / Xephyr to keep console clean
        return 0;
    }
    if (twin_saved_error_handler) {
        return twin_saved_error_handler(d, e);
    }
    return 0;
}

__attribute__((visibility("default"))) XErrorHandler XSetErrorHandler(XErrorHandler handler) {
    static XErrorHandler (*real_xseterrorhandler)(XErrorHandler) = NULL;
    if (!real_xseterrorhandler) {
        real_xseterrorhandler = (XErrorHandler (*)(XErrorHandler))dlsym(RTLD_NEXT, "XSetErrorHandler");
    }
    if (handler == trap_x_errors) {
        return real_xseterrorhandler(trap_x_errors);
    }
    twin_saved_error_handler = handler;
    return real_xseterrorhandler(quiet_x_error_handler);
}

typedef struct {
    Drawable d;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
} WinGeoCache;

#define WIN_GEO_CACHE_SIZE 32
static WinGeoCache win_geo_cache[WIN_GEO_CACHE_SIZE];
static int win_geo_count = 0;

static inline int get_cached_geometry(Display *dpy, Drawable d, unsigned int *w, unsigned int *h, unsigned int *depth) {
    for (int i = 0; i < win_geo_count; i++) {
        if (win_geo_cache[i].d == d) {
            *w = win_geo_cache[i].width;
            *h = win_geo_cache[i].height;
            *depth = win_geo_cache[i].depth;
            return 1;
        }
    }
    Window root;
    int rx = 0, ry = 0;
    unsigned int win_w = 0, win_h = 0, border_w = 0, d_depth = 0;
    x_error_trap = 0;
    XErrorHandler prev_handler = XSetErrorHandler(trap_x_errors);
    Status s = XGetGeometry(dpy, d, &root, &rx, &ry, &win_w, &win_h, &border_w, &d_depth);
    XSetErrorHandler(prev_handler);
    if (!s || x_error_trap) {
        return 0;
    }
    if (win_geo_count < WIN_GEO_CACHE_SIZE) {
        win_geo_cache[win_geo_count].d = d;
        win_geo_cache[win_geo_count].width = win_w;
        win_geo_cache[win_geo_count].height = win_h;
        win_geo_cache[win_geo_count].depth = d_depth;
        win_geo_count++;
    } else {
        win_geo_cache[0].d = d;
        win_geo_cache[0].width = win_w;
        win_geo_cache[0].height = win_h;
        win_geo_cache[0].depth = d_depth;
    }
    *w = win_w;
    *h = win_h;
    *depth = d_depth;
    return 1;
}

#define SCRATCH_BUF_MAX_W 640
#define SCRATCH_BUF_MAX_H 256
#define SCRATCH_BUF_PIXELS (SCRATCH_BUF_MAX_W * SCRATCH_BUF_MAX_H)

static uint32_t scratch_text_buf[SCRATCH_BUF_PIXELS];
static XImage *scratch_ximage = NULL;
static Display *scratch_ximage_dpy = NULL;

static XImage *get_scratch_ximage(Display *dpy, int w, int h) {
    if (w <= 0 || h <= 0 || w > SCRATCH_BUF_MAX_W || (w * h) > SCRATCH_BUF_PIXELS) {
        return NULL;
    }
    if (!scratch_ximage || scratch_ximage_dpy != dpy) {
        if (scratch_ximage) {
            scratch_ximage->data = NULL;
            XDestroyImage(scratch_ximage);
            scratch_ximage = NULL;
        }
        Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
        scratch_ximage = XCreateImage(dpy, vis, 24, ZPixmap, 0, (char *)scratch_text_buf, SCRATCH_BUF_MAX_W, SCRATCH_BUF_MAX_H, 32, SCRATCH_BUF_MAX_W * 4);
        scratch_ximage_dpy = dpy;
    }
    if (scratch_ximage) {
        scratch_ximage->width = w;
        scratch_ximage->height = h;
        scratch_ximage->bytes_per_line = w * 4;
        scratch_ximage->data = (char *)scratch_text_buf;
    }
    return scratch_ximage;
}

#define GC_CACHE_SIZE 8
typedef struct {
    Display *dpy;
    Drawable win;
    GC gc;
} GCCacheEntry;
static GCCacheEntry gc_cache[GC_CACHE_SIZE];
static int gc_cache_count = 0;

static inline void reset_scratch_and_geo(void) {
    if (scratch_ximage) {
        scratch_ximage->data = NULL;
        XDestroyImage(scratch_ximage);
        scratch_ximage = NULL;
        scratch_ximage_dpy = NULL;
    }
    for (int g = 0; g < gc_cache_count; g++) {
        if (gc_cache[g].gc && gc_cache[g].dpy) {
            XFreeGC(gc_cache[g].dpy, gc_cache[g].gc);
        }
    }
    gc_cache_count = 0;
    win_geo_count = 0;
}

static int draw_text_aa(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length, int is_image_string) {
    if (!ft_initialized) {
        init_freetype();
    }
    if (ft_failed || !ft_face_regular || length <= 0 || !string) {
        return -1;
    }

    XGCValues values;
    if (!XGetGCValues(dpy, gc, GCForeground | GCBackground | GCFont, &values)) {
        return -1;
    }

    int font_size = 12, is_bold = 0, font_ascent = 10;
    get_font_info(dpy, values.font, &font_size, &is_bold, &font_ascent);

    CachedGlyph *glyphs_buf[256];
    CachedGlyph **glyphs = glyphs_buf;
    if (length > 256) {
        glyphs = (CachedGlyph **)malloc(sizeof(CachedGlyph *) * length);
        if (!glyphs) return -1;
    }

    int pen_x = x;
    int min_x = x, max_x = x;
    int min_y = y - font_ascent, max_y = y + (font_size - font_ascent);

    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, font_size, c);
        glyphs[i] = cg;
        if (!cg) continue;

        int gx = pen_x + cg->bitmap_left;
        int gy = y - cg->bitmap_top;
        int gw = cg->width;
        int gh = cg->rows;

        if (gx < min_x) min_x = gx;
        if (gy < min_y) min_y = gy;
        if (gx + gw > max_x) max_x = gx + gw;
        if (gy + gh > max_y) max_y = gy + gh;

        pen_x += cg->advance_x;
    }

    if (pen_x > max_x) max_x = pen_x;

    int is_pure_opaque = 0;
    if (is_image_string) {
        int bg_left = x;
        int bg_right = pen_x;
        int bg_top = y - font_ascent;
        int bg_bottom = y + (font_size - font_ascent);

        if (min_x >= bg_left && max_x <= bg_right && min_y >= bg_top && max_y <= bg_bottom) {
            is_pure_opaque = 1;
            min_x = bg_left;
            max_x = bg_right;
            min_y = bg_top;
            max_y = bg_bottom;
        } else {
            min_x -= 1;
            min_y -= 1;
            max_x += 1;
            max_y += 1;
        }
    } else {
        min_x -= 1;
        min_y -= 1;
        max_x += 1;
        max_y += 1;
    }

    unsigned int win_w = 0, win_h = 0, depth = 0;
    if (!get_cached_geometry(dpy, d, &win_w, &win_h, &depth) || depth < 24) {
        if (glyphs != glyphs_buf) free(glyphs);
        return -1;
    }

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > (int)win_w) max_x = (int)win_w;
    if (max_y > (int)win_h) max_y = (int)win_h;

    int bbox_w = max_x - min_x;
    int bbox_h = max_y - min_y;
    if (bbox_w <= 0 || bbox_h <= 0) {
        if (glyphs != glyphs_buf) free(glyphs);
        return 0;
    }

    XImage *img = NULL;
    int is_scratch = 0;
    if (is_pure_opaque) {
        img = get_scratch_ximage(dpy, bbox_w, bbox_h);
        if (img) {
            is_scratch = 1;
            fast_fill_pixel((uint32_t *)img->data, (uint32_t)values.background, bbox_w * bbox_h);
        } else {
            uint32_t *data = (uint32_t *)malloc(bbox_w * bbox_h * sizeof(uint32_t));
            if (data) {
                fast_fill_pixel(data, (uint32_t)values.background, bbox_w * bbox_h);
                Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
                img = XCreateImage(dpy, vis, 24, ZPixmap, 0, (char *)data, bbox_w, bbox_h, 32, bbox_w * 4);
            }
        }
    }
    if (!img) {
        x_error_trap = 0;
        XErrorHandler prev_handler = XSetErrorHandler(trap_x_errors);
        img = XGetImage(dpy, d, min_x, min_y, bbox_w, bbox_h, AllPlanes, ZPixmap);
        XSetErrorHandler(prev_handler);
    }
    if (!img) {
        if (glyphs != glyphs_buf) free(glyphs);
        return -1;
    }

    if (img->bits_per_pixel != 24 && img->bits_per_pixel != 32) {
        if (!is_scratch) XDestroyImage(img);
        if (glyphs != glyphs_buf) free(glyphs);
        return -1;
    }

    int is_fast_32 = (img->bits_per_pixel == 32 &&
                      img->red_mask == 0x00FF0000 &&
                      img->green_mask == 0x0000FF00 &&
                      img->blue_mask == 0x000000FF);

    unsigned long fg = values.foreground;
    unsigned int fg_r = (fg >> 16) & 0xFF;
    unsigned int fg_g = (fg >> 8) & 0xFF;
    unsigned int fg_b = fg & 0xFF;

    uint32_t fg_val = (fg_r << 16) | (fg_g << 8) | fg_b;
    uint32_t rb_fg = fg_val & 0x00FF00FF;
    uint32_t g_fg  = (fg_val >> 8) & 0x000000FF;

    if (is_fast_32) {
        if (!is_pure_opaque && is_image_string) {
            uint32_t bg_val = (uint32_t)values.background;
            for (int py = 0; py < bbox_h; py++) {
                uint32_t *dst_row = (uint32_t *)(img->data + py * img->bytes_per_line);
                fast_fill_pixel(dst_row, bg_val, bbox_w);
            }
        }

        pen_x = x;
        for (int i = 0; i < length; i++) {
            CachedGlyph *cg = glyphs[i];
            if (!cg) continue;

            if (cg->buffer && cg->rows > 0 && cg->width > 0) {
                int gx0 = pen_x + cg->bitmap_left - min_x;
                int gy0 = y - cg->bitmap_top - min_y;

                int r_start = (gy0 < 0) ? -gy0 : 0;
                int r_end = (gy0 + cg->rows > bbox_h) ? (bbox_h - gy0) : cg->rows;
                int c_start = (gx0 < 0) ? -gx0 : 0;
                int c_end = (gx0 + cg->width > bbox_w) ? (bbox_w - gx0) : cg->width;

                if (r_start < r_end && c_start < c_end) {
                    int col_count = c_end - c_start;
                    for (int row = r_start; row < r_end; row++) {
                        int py = gy0 + row;
                        uint32_t *dst = (uint32_t *)(img->data + py * img->bytes_per_line) + gx0 + c_start;
                        const unsigned char *src_ptr = cg->buffer + row * cg->pitch + c_start;

                        for (int col = 0; col < col_count; col++) {
                            unsigned int alpha = src_ptr[col];
                            if (alpha < 24) continue;

                            if (alpha >= 224) {
                                dst[col] = fg_val;
                            } else {
                                uint32_t bg = dst[col];
                                uint32_t rb_bg = bg & 0x00FF00FF;
                                uint32_t g_bg  = (bg >> 8) & 0x000000FF;
                                unsigned int inv_a = 255 - alpha;
                                uint32_t rb = ((rb_fg * alpha + rb_bg * inv_a + 0x00800080) >> 8) & 0x00FF00FF;
                                uint32_t g  = ((g_fg * alpha + g_bg * inv_a + 0x00000080) >> 8) & 0x000000FF;
                                dst[col] = rb | (g << 8);
                            }
                        }
                    }
                }
            }
            pen_x += cg->advance_x;
        }
    } else {
        if (is_image_string) {
            for (int py = 0; py < bbox_h; py++) {
                for (int px = 0; px < bbox_w; px++) {
                    XPutPixel(img, px, py, values.background);
                }
            }
        }

        pen_x = x;
        for (int i = 0; i < length; i++) {
            CachedGlyph *cg = glyphs[i];
            if (!cg) continue;

            if (cg->buffer && cg->rows > 0 && cg->width > 0) {
                int gx0 = pen_x + cg->bitmap_left - min_x;
                int gy0 = y - cg->bitmap_top - min_y;

                for (int row = 0; row < cg->rows; row++) {
                    int py = gy0 + row;
                    if (py < 0 || py >= bbox_h) continue;
                    unsigned char *src = cg->buffer + row * cg->pitch;

                    for (int col = 0; col < cg->width; col++) {
                        unsigned int alpha = src[col];
                        if (alpha < 24) continue;

                        int px = gx0 + col;
                        if (px < 0 || px >= bbox_w) continue;

                        if (alpha >= 224) {
                            XPutPixel(img, px, py, fg);
                        } else {
                            unsigned long bg = XGetPixel(img, px, py);
                            unsigned int bg_r = (bg >> 16) & 0xFF;
                            unsigned int bg_g = (bg >> 8) & 0xFF;
                            unsigned int bg_b = bg & 0xFF;

                            unsigned int inv_a = 255 - alpha;
                            unsigned int out_r = (fg_r * alpha + bg_r * inv_a + 128) >> 8;
                            unsigned int out_g = (fg_g * alpha + bg_g * inv_a + 128) >> 8;
                            unsigned int out_b = (fg_b * alpha + bg_b * inv_a + 128) >> 8;

                            XPutPixel(img, px, py, (out_r << 16) | (out_g << 8) | out_b);
                        }
                    }
                }
            }
            pen_x += cg->advance_x;
        }
    }

    XPutImage(dpy, d, gc, img, 0, 0, min_x, min_y, bbox_w, bbox_h);
    if (!is_scratch) {
        XDestroyImage(img);
    }
    XFlush(dpy);
    if (glyphs != glyphs_buf) free(glyphs);
    return 0;
}

typedef uint32_t (*real_getpixel_t)(void *hdc, int x, int y);
typedef uint32_t (*real_setpixel_t)(void *hdc, int x, int y, uint32_t crColor);
typedef uint32_t (*real_gettextcolor_t)(void *hdc);
typedef uint32_t (*real_getbkcolor_t)(void *hdc);
typedef int (*real_getbkmode_t)(void *hdc);
typedef int (*real_gettextalign_t)(void *hdc);
typedef int (*real_gettextmetrics_t)(void *hdc, void *tm);
typedef int (*real_gettextface_t)(void *hdc, int nCount, char *lpFaceName);
typedef void *(*real_twin_getdisplaydata_t)(void);
typedef void *(*real_twin_getwindowdata_t)(void *hwnd);
typedef void *(*real_windowfromdc_t)(void *hdc);
typedef int (*real_getdcorgex_t)(void *hdc, void *pt);
typedef void *HGDIOBJ;
typedef struct { int left, top, right, bottom; } RECT;
typedef HGDIOBJ (*real_createsolidbrush_t)(uint32_t crColor);
typedef int (*real_fillrect_t)(void *hdc, const RECT *lprc, HGDIOBJ hbr);
typedef int (*real_deleteobject_t)(HGDIOBJ hObject);

static real_getpixel_t real_getpixel = NULL;
static real_setpixel_t real_setpixel = NULL;
static real_gettextcolor_t real_gettextcolor = NULL;
static real_getbkcolor_t real_getbkcolor = NULL;
static real_getbkmode_t real_getbkmode = NULL;
static real_gettextalign_t real_gettextalign = NULL;
static real_gettextmetrics_t real_gettextmetrics = NULL;
static real_gettextface_t real_gettextface = NULL;
static real_twin_getdisplaydata_t p_getdisplaydata = NULL;
static real_twin_getwindowdata_t p_getwindowdata = NULL;
static real_windowfromdc_t real_windowfromdc = NULL;
static real_getdcorgex_t p_getdcorgex = NULL;
static real_createsolidbrush_t real_createsolidbrush = NULL;
static real_fillrect_t real_fillrect = NULL;
static real_deleteobject_t real_deleteobject = NULL;

typedef struct {
    int32_t tmHeight;
    int32_t tmAscent;
    int32_t tmDescent;
    int32_t tmInternalLeading;
    int32_t tmExternalLeading;
    int32_t tmAveCharWidth;
    int32_t tmMaxCharWidth;
    int32_t tmWeight;
    int32_t tmOverhang;
    int32_t tmDigitizedAspectX;
    int32_t tmDigitizedAspectY;
    uint8_t tmFirstChar;
    uint8_t tmLastChar;
    uint8_t tmDefaultChar;
    uint8_t tmBreakChar;
    uint8_t tmItalic;
    uint8_t tmUnderlined;
    uint8_t tmStruckOut;
    uint8_t tmPitchAndFamily;
    uint8_t tmCharSet;
} MY_TEXTMETRIC;

typedef struct {
    uint8_t peRed;
    uint8_t peGreen;
    uint8_t peBlue;
    uint8_t peFlags;
} MY_PALETTEENTRY;

typedef unsigned int (*real_getpaletteentries_t)(void *hpal, unsigned int iStart, unsigned int cEntries, MY_PALETTEENTRY *pEntries);
static real_getpaletteentries_t real_getpaletteentries = NULL;

static MY_PALETTEENTRY cached_palette[256];
static uint32_t cached_rgb_palette[256];
static int palette_cached = 0;
static void **cached_hpal_sym = NULL;
static int hpal_resolved = 0;

static void update_palette_cache(void) {
    if (!real_getpaletteentries) {
        real_getpaletteentries = (real_getpaletteentries_t)dlsym(RTLD_NEXT, "GetPaletteEntries");
    }
    if (!hpal_resolved) {
        cached_hpal_sym = (void **)dlsym(RTLD_DEFAULT, "hpal");
        hpal_resolved = 1;
    }
    if (cached_hpal_sym && real_getpaletteentries) {
        void *hpal = *cached_hpal_sym;
        if (hpal) {
            unsigned int num = real_getpaletteentries(hpal, 0, 256, cached_palette);
            if (num > 0) {
                for (unsigned int i = 0; i < num; i++) {
                    cached_rgb_palette[i] = ((uint32_t)cached_palette[i].peRed) |
                                            ((uint32_t)cached_palette[i].peGreen << 8) |
                                            ((uint32_t)cached_palette[i].peBlue << 16);
                }
                palette_cached = 1;
            }
        }
    }
}

static inline uint32_t colorref_to_rgb(uint32_t col) {
    if ((col & 0xFF000000) == 0x01000000) {
        if (__builtin_expect(!palette_cached, 0)) {
            update_palette_cache();
        }
        if (palette_cached) {
            return cached_rgb_palette[col & 0xFF];
        }
        unsigned int idx = col & 0xFF;
        if (idx == 0 || idx == 252) return 0x00000000;
        if (idx == 255) return 0x00FFFFFF;
        return (idx) | (idx << 8) | (idx << 16);
    }
    return col & 0x00FFFFFF;
}

static int gdi_resolved = 0;
static void resolve_gdi_symbols(void) {
    if (__builtin_expect(!gdi_resolved, 0)) {
        real_gettextmetrics = (real_gettextmetrics_t)dlsym(RTLD_NEXT, "GetTextMetrics");
        real_gettextcolor = (real_gettextcolor_t)dlsym(RTLD_NEXT, "GetTextColor");
        real_getbkcolor = (real_getbkcolor_t)dlsym(RTLD_NEXT, "GetBkColor");
        real_getbkmode = (real_getbkmode_t)dlsym(RTLD_NEXT, "GetBkMode");
        real_gettextalign = (real_gettextalign_t)dlsym(RTLD_NEXT, "GetTextAlign");
        real_getpixel = (real_getpixel_t)dlsym(RTLD_NEXT, "GetPixel");
        real_setpixel = (real_setpixel_t)dlsym(RTLD_NEXT, "SetPixel");
        real_gettextface = (real_gettextface_t)dlsym(RTLD_NEXT, "GetTextFace");
        p_getdisplaydata = (real_twin_getdisplaydata_t)dlsym(RTLD_DEFAULT, "TWIN_GetDisplayData");
        p_getwindowdata = (real_twin_getwindowdata_t)dlsym(RTLD_DEFAULT, "TWIN_GetWindowData");
        real_windowfromdc = (real_windowfromdc_t)dlsym(RTLD_DEFAULT, "WindowFromDC");
        p_getdcorgex = (real_getdcorgex_t)dlsym(RTLD_DEFAULT, "GetDCOrgEx");
        real_createsolidbrush = (real_createsolidbrush_t)dlsym(RTLD_NEXT, "CreateSolidBrush");
        real_fillrect = (real_fillrect_t)dlsym(RTLD_NEXT, "FillRect");
        real_deleteobject = (real_deleteobject_t)dlsym(RTLD_NEXT, "DeleteObject");
        gdi_resolved = 1;
    }
}

static int cached_no_aa = -1;
static inline int is_no_aa(void) {
    if (__builtin_expect(cached_no_aa < 0, 0)) {
        const char *env = getenv("EXILE_NO_AA");
        cached_no_aa = (env && strcmp(env, "1") == 0) ? 1 : 0;
    }
    return cached_no_aa;
}

typedef int (*real_drawtext_t)(void *hdc, const char *lpString, int nCount, void *lpRect, unsigned int uFormat);
typedef int (*real_exttextout_t)(void *hdc, int x, int y, unsigned int fuOptions, const void *lprc, const char *lpString, unsigned int cbCount, const int *lpDx);
typedef int (*real_textout_t)(void *hdc, int x, int y, const char *lpString, int nCount);

static real_drawtext_t real_drawtext = NULL;
static real_drawtext_t real_drawtexta = NULL;
static real_exttextout_t real_exttextout = NULL;
static real_exttextout_t real_exttextouta = NULL;
static real_textout_t real_textout = NULL;
static real_textout_t real_textouta = NULL;

__attribute__((visibility("default"))) int MessageBox(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType) {
    if (lpText && strstr(lpText, "/dev/dsp")) {
        return 1;
    }
    if (!real_msgbox) real_msgbox = (real_msgbox_t)dlsym(RTLD_NEXT, "MessageBox");
    return real_msgbox ? real_msgbox(hWnd, lpText, lpCaption, uType) : 1;
}

__attribute__((visibility("default"))) int MessageBoxA(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType) {
    return MessageBox(hWnd, lpText, lpCaption, uType);
}

static int draw_text_hdc_aa(void *hdc, int x, int y, const char *string, int length) {
    if (!ft_initialized) {
        init_freetype();
    }
    if (ft_failed || !ft_face_regular || length <= 0 || !string) {
        return -1;
    }

    resolve_gdi_symbols();
    if (!real_setpixel) return -1;

    MY_TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    int font_height = 12;
    int font_ascent = 10;
    int is_bold = 0;
    if (real_gettextmetrics && real_gettextmetrics(hdc, &tm)) {
        if (tm.tmHeight > 0) font_height = tm.tmHeight;
        if (tm.tmAscent > 0) font_ascent = tm.tmAscent;
        if (tm.tmWeight >= 700) is_bold = 1;
    }

    if (!is_bold && real_gettextface) {
        char facename[64] = {0};
        real_gettextface(hdc, sizeof(facename) - 1, facename);
        if (strstr(facename, "Bold") || strstr(facename, "bold")) {
            is_bold = 1;
        }
    }

    if (font_ascent <= 0) {
        font_ascent = (font_height * 4) / 5;
    }

    int render_height = font_height;
    if (tm.tmInternalLeading > 0 && render_height > 10) {
        render_height -= tm.tmInternalLeading;
    } else if (render_height >= 13) {
        render_height -= 2;
    } else if (render_height == 12) {
        render_height = 11;
    }

    static int user_height_adjust = 999;
    if (user_height_adjust == 999) {
        const char *adj_env = getenv("EXILE_FONT_HEIGHT_ADJUST");
        if (adj_env && *adj_env) {
            user_height_adjust = atoi(adj_env);
        } else {
            user_height_adjust = 0;
        }
    }
    render_height += user_height_adjust;
    if (render_height < 8) render_height = 8;

    uint32_t fg_raw = real_gettextcolor ? real_gettextcolor(hdc) : 0x00000000;
    uint32_t bk_raw = real_getbkcolor ? real_getbkcolor(hdc) : 0x00FFFFFF;
    int bk_mode = real_getbkmode ? real_getbkmode(hdc) : 1;

    uint32_t fg_color = colorref_to_rgb(fg_raw);
    uint32_t bk_color = colorref_to_rgb(bk_raw);

    unsigned int fg_r = fg_color & 0xFF;
    unsigned int fg_g = (fg_color >> 8) & 0xFF;
    unsigned int fg_b = (fg_color >> 16) & 0xFF;

    unsigned int bk_r = bk_color & 0xFF;
    unsigned int bk_g = (bk_color >> 8) & 0xFF;
    unsigned int bk_b = (bk_color >> 16) & 0xFF;

    int align = real_gettextalign ? real_gettextalign(hdc) : 0;
    int org[2] = {0, 0};
    if (p_getdcorgex) p_getdcorgex(hdc, org);

    CachedGlyph *glyphs_buf[256];
    CachedGlyph **glyphs = glyphs_buf;
    if (length > 256) {
        glyphs = (CachedGlyph **)malloc(sizeof(CachedGlyph *) * length);
        if (!glyphs) return -1;
    }

    int total_width = 0;
    int rel_min_x = 0x7FFFFFFF, rel_min_y = 0x7FFFFFFF;
    int rel_max_x = -0x7FFFFFFF, rel_max_y = -0x7FFFFFFF;
    int has_pixels = 0;

    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, render_height, c);
        glyphs[i] = cg;
        if (cg) {
            if (cg->rows > 0 && cg->width > 0) {
                int gx0 = total_width + cg->bitmap_left;
                int gy0 = -cg->bitmap_top;
                int gx1 = gx0 + cg->width;
                int gy1 = gy0 + cg->rows;

                if (gx0 < rel_min_x) rel_min_x = gx0;
                if (gy0 < rel_min_y) rel_min_y = gy0;
                if (gx1 > rel_max_x) rel_max_x = gx1;
                if (gy1 > rel_max_y) rel_max_y = gy1;
                has_pixels = 1;
            }
            total_width += cg->advance_x;
        }
    }

    int pen_x = x;
    if ((align & 0x06) == 0x06) {
        pen_x = x - (total_width / 2);
    } else if ((align & 0x02) == 0x02) {
        pen_x = x - total_width;
    }

    int pen_y = y;
    if ((align & 0x18) == 0x18) {
        pen_y = y;
    } else if (align & 0x08) {
        pen_y = y - (font_height - font_ascent);
    } else {
        pen_y = y + font_ascent;
    }

    // Attempt direct X11 block blit (instantaneous in-memory compositing)
    static Display *cached_twin_dpy = NULL;
    if (__builtin_expect(!cached_twin_dpy, 0)) {
        if (p_getdisplaydata) cached_twin_dpy = (Display *)p_getdisplaydata();
    }
    Display *dpy = cached_twin_dpy;
    void *hwnd = real_windowfromdc ? real_windowfromdc(hdc) : NULL;
    Drawable win_d = (p_getwindowdata && hwnd) ? (Drawable)p_getwindowdata(hwnd) : 0;

    if (dpy && win_d) {
        int dev_pen_x = pen_x + org[0];
        int dev_pen_y = pen_y + org[1];
        int dev_y = y + org[1];

        int min_x = has_pixels ? (dev_pen_x + rel_min_x) : dev_pen_x;
        int max_x = has_pixels ? (dev_pen_x + rel_max_x) : dev_pen_x;
        int min_y = has_pixels ? (dev_pen_y + rel_min_y) : dev_pen_y;
        int max_y = has_pixels ? (dev_pen_y + rel_max_y) : dev_pen_y;

        int is_pure_opaque = 0;
        int bg_left = dev_pen_x;
        int bg_right = dev_pen_x + total_width;
        int bg_top = dev_y;
        int bg_bottom = dev_y + font_height;

        if (bk_mode == 2) {
            if (!has_pixels || (min_x >= bg_left && max_x <= bg_right && min_y >= bg_top && max_y <= bg_bottom)) {
                is_pure_opaque = 1;
                min_x = bg_left;
                max_x = bg_right;
                min_y = bg_top;
                max_y = bg_bottom;
            } else {
                if (bg_left < min_x) min_x = bg_left;
                if (bg_top < min_y) min_y = bg_top;
                if (bg_right > max_x) max_x = bg_right;
                if (bg_bottom > max_y) max_y = bg_bottom;
                min_x -= 1;
                min_y -= 1;
                max_x += 1;
                max_y += 1;
            }
        } else {
            min_x -= 1;
            min_y -= 1;
            max_x += 1;
            max_y += 1;
        }

        if (min_x >= max_x || min_y >= max_y) {
            if (glyphs != glyphs_buf) free(glyphs);
            return 0; // Nothing visible to draw
        }

        unsigned int win_w = 0, win_h = 0, depth = 0;
        if (get_cached_geometry(dpy, win_d, &win_w, &win_h, &depth) && depth >= 24) {
            if (min_x < 0) min_x = 0;
            if (min_y < 0) min_y = 0;
            if (max_x > (int)win_w) max_x = (int)win_w;
            if (max_y > (int)win_h) max_y = (int)win_h;
            int bbox_w = max_x - min_x;
            int bbox_h = max_y - min_y;

            if (bbox_w > 0 && bbox_h > 0) {
                GC local_gc = 0;
                for (int g = 0; g < gc_cache_count; g++) {
                    if (gc_cache[g].dpy == dpy && gc_cache[g].win == win_d) {
                        local_gc = gc_cache[g].gc;
                        break;
                    }
                }
                if (!local_gc) {
                    local_gc = XCreateGC(dpy, win_d, 0, NULL);
                    if (local_gc) {
                        if (gc_cache_count < GC_CACHE_SIZE) {
                            gc_cache[gc_cache_count].dpy = dpy;
                            gc_cache[gc_cache_count].win = win_d;
                            gc_cache[gc_cache_count].gc = local_gc;
                            gc_cache_count++;
                        } else {
                            if (gc_cache[0].gc && gc_cache[0].dpy) {
                                XFreeGC(gc_cache[0].dpy, gc_cache[0].gc);
                            }
                            gc_cache[0].dpy = dpy;
                            gc_cache[0].win = win_d;
                            gc_cache[0].gc = local_gc;
                        }
                    }
                }

                if (local_gc) {
                    XImage *img = NULL;
                    int is_scratch = 0;
                    uint32_t bk_pixel = (bk_r << 16) | (bk_g << 8) | bk_b;

                    if (is_pure_opaque) {
                        img = get_scratch_ximage(dpy, bbox_w, bbox_h);
                        if (img) {
                            is_scratch = 1;
                            fast_fill_pixel((uint32_t *)img->data, bk_pixel, bbox_w * bbox_h);
                        } else {
                            uint32_t *data = (uint32_t *)malloc(bbox_w * bbox_h * sizeof(uint32_t));
                            if (data) {
                                fast_fill_pixel(data, bk_pixel, bbox_w * bbox_h);
                                Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
                                img = XCreateImage(dpy, vis, 24, ZPixmap, 0, (char *)data, bbox_w, bbox_h, 32, bbox_w * 4);
                            }
                        }
                    }
                    if (!img) {
                        x_error_trap = 0;
                        XErrorHandler prev_handler = XSetErrorHandler(trap_x_errors);
                        img = XGetImage(dpy, win_d, min_x, min_y, bbox_w, bbox_h, AllPlanes, ZPixmap);
                        XSetErrorHandler(prev_handler);
                    }

                    if (img && !x_error_trap && (img->bits_per_pixel == 24 || img->bits_per_pixel == 32)) {
                        int is_fast_32 = (img->bits_per_pixel == 32 &&
                                          img->red_mask == 0x00FF0000 &&
                                          img->green_mask == 0x0000FF00 &&
                                          img->blue_mask == 0x000000FF);

                        int bx0 = dev_pen_x - min_x;
                        int by0 = dev_y - min_y;
                        int bx1 = bx0 + total_width;
                        int by1 = by0 + font_height;

                        if (bx0 < 0) bx0 = 0;
                        if (by0 < 0) by0 = 0;
                        if (bx1 > bbox_w) bx1 = bbox_w;
                        if (by1 > bbox_h) by1 = bbox_h;

                        if (is_fast_32) {
                            if (!is_pure_opaque && bk_mode == 2 && bx1 > bx0 && by1 > by0) {
                                for (int py = by0; py < by1; py++) {
                                    uint32_t *dst_row = (uint32_t *)(img->data + py * img->bytes_per_line);
                                    fast_fill_pixel(dst_row + bx0, bk_pixel, bx1 - bx0);
                                }
                            }

                            uint32_t fg_pixel = (fg_r << 16) | (fg_g << 8) | fg_b;
                            uint32_t rb_fg = fg_pixel & 0x00FF00FF;
                            uint32_t g_fg  = (fg_pixel >> 8) & 0x000000FF;
                            int cur_x = dev_pen_x;

                            for (int i = 0; i < length; i++) {
                                CachedGlyph *cg = glyphs[i];
                                if (!cg) continue;

                                if (cg->buffer && cg->rows > 0 && cg->width > 0) {
                                    int gx0 = cur_x + cg->bitmap_left - min_x;
                                    int gy0 = dev_pen_y - cg->bitmap_top - min_y;

                                    int r_start = (gy0 < 0) ? -gy0 : 0;
                                    int r_end = (gy0 + cg->rows > bbox_h) ? (bbox_h - gy0) : cg->rows;
                                    int c_start = (gx0 < 0) ? -gx0 : 0;
                                    int c_end = (gx0 + cg->width > bbox_w) ? (bbox_w - gx0) : cg->width;

                                    if (r_start < r_end && c_start < c_end) {
                                        int col_count = c_end - c_start;
                                        for (int row = r_start; row < r_end; row++) {
                                            int py = gy0 + row;
                                            uint32_t *dst = (uint32_t *)(img->data + py * img->bytes_per_line) + gx0 + c_start;
                                            const unsigned char *src_ptr = cg->buffer + row * cg->pitch + c_start;

                                            for (int col = 0; col < col_count; col++) {
                                                unsigned int alpha = src_ptr[col];
                                                if (alpha < 24) continue;

                                                if (alpha >= 224) {
                                                    dst[col] = fg_pixel;
                                                } else {
                                                    uint32_t bg = dst[col];
                                                    uint32_t rb_bg = bg & 0x00FF00FF;
                                                    uint32_t g_bg  = (bg >> 8) & 0x000000FF;
                                                    unsigned int inv_a = 255 - alpha;
                                                    uint32_t rb = ((rb_fg * alpha + rb_bg * inv_a + 0x00800080) >> 8) & 0x00FF00FF;
                                                    uint32_t g  = ((g_fg * alpha + g_bg * inv_a + 0x00000080) >> 8) & 0x000000FF;
                                                    dst[col] = rb | (g << 8);
                                                }
                                            }
                                        }
                                    }
                                }
                                cur_x += cg->advance_x;
                            }
                        } else {
                            if (bk_mode == 2 && bx1 > bx0 && by1 > by0) {
                                unsigned long bk_pixel_ul = (bk_r << 16) | (bk_g << 8) | bk_b;
                                for (int py = by0; py < by1; py++) {
                                    for (int px = bx0; px < bx1; px++) {
                                        XPutPixel(img, px, py, bk_pixel_ul);
                                    }
                                }
                            }

                            unsigned long fg_pixel_ul = (fg_r << 16) | (fg_g << 8) | fg_b;
                            int cur_x = dev_pen_x;

                            for (int i = 0; i < length; i++) {
                                CachedGlyph *cg = glyphs[i];
                                if (!cg) continue;

                                if (cg->buffer && cg->rows > 0 && cg->width > 0) {
                                    int gx0 = cur_x + cg->bitmap_left - min_x;
                                    int gy0 = dev_pen_y - cg->bitmap_top - min_y;

                                    for (int row = 0; row < cg->rows; row++) {
                                        int py = gy0 + row;
                                        if (py < 0 || py >= bbox_h) continue;
                                        unsigned char *src = cg->buffer + row * cg->pitch;

                                        for (int col = 0; col < cg->width; col++) {
                                            unsigned int alpha = src[col];
                                            if (alpha < 24) continue;

                                            int px = gx0 + col;
                                            if (px < 0 || px >= bbox_w) continue;

                                            if (alpha >= 224) {
                                                XPutPixel(img, px, py, fg_pixel_ul);
                                            } else {
                                                unsigned long bg = XGetPixel(img, px, py);
                                                unsigned int cur_bg_r = (bg >> 16) & 0xFF;
                                                unsigned int cur_bg_g = (bg >> 8) & 0xFF;
                                                unsigned int cur_bg_b = bg & 0xFF;

                                                unsigned int inv_a = 255 - alpha;
                                                unsigned int out_r = (fg_r * alpha + cur_bg_r * inv_a + 128) >> 8;
                                                unsigned int out_g = (fg_g * alpha + cur_bg_g * inv_a + 128) >> 8;
                                                unsigned int out_b = (fg_b * alpha + cur_bg_b * inv_a + 128) >> 8;

                                                XPutPixel(img, px, py, (out_r << 16) | (out_g << 8) | out_b);
                                            }
                                        }
                                    }
                                }
                                cur_x += cg->advance_x;
                            }
                        }

                        XPutImage(dpy, win_d, local_gc, img, 0, 0, min_x, min_y, bbox_w, bbox_h);
                        if (!is_scratch) {
                            XDestroyImage(img);
                        }
                        XFlush(dpy);
                        if (glyphs != glyphs_buf) free(glyphs);
                        return 0; // Ultra-fast single block blit succeeded!
                    }
                    if (img && !is_scratch) XDestroyImage(img);
                }
                if (x_error_trap) {
                    reset_scratch_and_geo();
                }
            }
        }
    }

    if (bk_mode == 2) {
        if (real_createsolidbrush && real_fillrect && real_deleteobject) {
            RECT rc = { pen_x, y, pen_x + total_width, y + font_height };
            HGDIOBJ hbr = real_createsolidbrush(bk_color);
            if (hbr) {
                real_fillrect(hdc, &rc, hbr);
                real_deleteobject(hbr);
            }
        }
    }

    for (int i = 0; i < length; i++) {
        CachedGlyph *cg = glyphs[i];
        if (!cg) continue;

        if (cg->buffer && cg->rows > 0 && cg->width > 0) {
            int gx0 = pen_x + cg->bitmap_left;
            int gy0 = pen_y - cg->bitmap_top;

            for (int row = 0; row < cg->rows; row++) {
                int py = gy0 + row;
                unsigned char *src = cg->buffer + row * cg->pitch;

                for (int col = 0; col < cg->width; col++) {
                    unsigned int alpha = src[col];
                    if (alpha < 24) continue;

                    int px = gx0 + col;
                    if (alpha >= 224) {
                        real_setpixel(hdc, px, py, fg_color);
                    } else if (bk_mode == 2) {
                        unsigned int inv_a = 255 - alpha;
                        unsigned int out_r = (fg_r * alpha + bk_r * inv_a + 128) >> 8;
                        unsigned int out_g = (fg_g * alpha + bk_g * inv_a + 128) >> 8;
                        unsigned int out_b = (fg_b * alpha + bk_b * inv_a + 128) >> 8;
                        uint32_t blended = (out_r) | (out_g << 8) | (out_b << 16);
                        real_setpixel(hdc, px, py, blended);
                    } else {
                        uint32_t bg = real_getpixel ? real_getpixel(hdc, px, py) : 0xFFFFFFFF;
                        if (bg == 0xFFFFFFFF) {
                            bg = (fg_color == 0) ? 0x00C0C0C0 : 0x00000000;
                        } else {
                            bg = colorref_to_rgb(bg);
                        }
                        unsigned int cur_bg_r = bg & 0xFF;
                        unsigned int cur_bg_g = (bg >> 8) & 0xFF;
                        unsigned int cur_bg_b = (bg >> 16) & 0xFF;

                        unsigned int inv_a = 255 - alpha;
                        unsigned int out_r = (fg_r * alpha + cur_bg_r * inv_a + 128) >> 8;
                        unsigned int out_g = (fg_g * alpha + cur_bg_g * inv_a + 128) >> 8;
                        unsigned int out_b = (fg_b * alpha + cur_bg_b * inv_a + 128) >> 8;

                        uint32_t blended = (out_r) | (out_g << 8) | (out_b << 16);
                        real_setpixel(hdc, px, py, blended);
                    }
                }
            }
        }
        pen_x += cg->advance_x;
    }

    if (glyphs != glyphs_buf) free(glyphs);
    return 0;
}

__attribute__((visibility("default"))) int DrawText(void *hdc, const char *lpString, int nCount, void *lpRect, unsigned int uFormat) {
    if (!real_drawtext) real_drawtext = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawText");
    return real_drawtext ? real_drawtext(hdc, lpString, nCount, lpRect, uFormat) : 0;
}

__attribute__((visibility("default"))) int DrawTextA(void *hdc, const char *lpString, int nCount, void *lpRect, unsigned int uFormat) {
    if (!real_drawtexta) real_drawtexta = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawTextA");
    return real_drawtexta ? real_drawtexta(hdc, lpString, nCount, lpRect, uFormat) : 0;
}

__attribute__((visibility("default"))) int ExtTextOut(void *hdc, int x, int y, unsigned int fuOptions, const void *lprc, const char *lpString, unsigned int cbCount, const int *lpDx) {
    if (!real_exttextout) real_exttextout = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOut");

    if (!is_no_aa()) {
        if (draw_text_hdc_aa(hdc, x, y, lpString, (int)cbCount) == 0) {
            return 1;
        }
    }

    return real_exttextout ? real_exttextout(hdc, x, y, fuOptions, lprc, lpString, cbCount, lpDx) : 0;
}

__attribute__((visibility("default"))) int ExtTextOutA(void *hdc, int x, int y, unsigned int fuOptions, const void *lprc, const char *lpString, unsigned int cbCount, const int *lpDx) {
    if (!real_exttextouta) real_exttextouta = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOutA");

    if (!is_no_aa()) {
        if (draw_text_hdc_aa(hdc, x, y, lpString, (int)cbCount) == 0) {
            return 1;
        }
    }

    return real_exttextouta ? real_exttextouta(hdc, x, y, fuOptions, lprc, lpString, cbCount, lpDx) : 0;
}

__attribute__((visibility("default"))) int TextOut(void *hdc, int x, int y, const char *lpString, int nCount) {
    if (!real_textout) real_textout = (real_textout_t)dlsym(RTLD_NEXT, "TextOut");

    if (!is_no_aa()) {
        int len = nCount >= 0 ? nCount : (lpString ? (int)strlen(lpString) : 0);
        if (draw_text_hdc_aa(hdc, x, y, lpString, len) == 0) {
            return 1;
        }
    }

    return real_textout ? real_textout(hdc, x, y, lpString, nCount) : 0;
}

__attribute__((visibility("default"))) int TextOutA(void *hdc, int x, int y, const char *lpString, int nCount) {
    if (!real_textouta) real_textouta = (real_textout_t)dlsym(RTLD_NEXT, "TextOutA");

    if (!is_no_aa()) {
        int len = nCount >= 0 ? nCount : (lpString ? (int)strlen(lpString) : 0);
        if (draw_text_hdc_aa(hdc, x, y, lpString, len) == 0) {
            return 1;
        }
    }

    return real_textouta ? real_textouta(hdc, x, y, lpString, nCount) : 0;
}

__attribute__((visibility("default"))) int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    if (!real_xdrawstring) {
        real_xdrawstring = (real_xdrawstring_t)dlsym(RTLD_NEXT, "XDrawString");
    }

    if (!is_no_aa()) {
        if (draw_text_aa(dpy, d, gc, x, y, string, length, 0) == 0) {
            return 0;
        }
    }

    if (real_xdrawstring) {
        return real_xdrawstring(dpy, d, gc, x, y, string, length);
    }
    return 0;
}

__attribute__((visibility("default"))) int XDrawImageString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    if (!real_xdrawimagestring) {
        real_xdrawimagestring = (real_xdrawimagestring_t)dlsym(RTLD_NEXT, "XDrawImageString");
    }

    if (!is_no_aa()) {
        if (draw_text_aa(dpy, d, gc, x, y, string, length, 1) == 0) {
            return 0;
        }
    }

    if (real_xdrawimagestring) {
        return real_xdrawimagestring(dpy, d, gc, x, y, string, length);
    }
    return 0;
}

__attribute__((constructor))
static void init_exile3_shim(void) {
    real_ioctl = (real_ioctl_t)dlsym(RTLD_NEXT, "ioctl");
    real_msgbox = (real_msgbox_t)dlsym(RTLD_NEXT, "MessageBox");
    real_xdrawstring = (real_xdrawstring_t)dlsym(RTLD_NEXT, "XDrawString");
    real_xdrawimagestring = (real_xdrawimagestring_t)dlsym(RTLD_NEXT, "XDrawImageString");
    real_drawtext = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawText");
    real_drawtexta = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawTextA");
    real_exttextout = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOut");
    real_exttextouta = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOutA");
    real_textout = (real_textout_t)dlsym(RTLD_NEXT, "TextOut");
    real_textouta = (real_textout_t)dlsym(RTLD_NEXT, "TextOutA");
    resolve_gdi_symbols();
    reset_sounds_fucked();
    prctl(PR_SET_TIMERSLACK, 1, 0, 0, 0);
    init_freetype();
    prewarm_font_cache();
}


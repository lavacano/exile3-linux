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

static void reset_sounds_fucked(void) {
    void *sf = dlsym(RTLD_DEFAULT, "sounds_fucked");
    if (sf) {
        *(volatile uint8_t *)sf = 0;
    }
    /* Force all 100 sound effects to play asynchronously so the game engine never freezes */
    void *as = dlsym(RTLD_DEFAULT, "always_asynch");
    if (as) {
        memset(as, 1, 100);
    }
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    if (!real_ioctl) {
        real_ioctl = (real_ioctl_t)dlsym(RTLD_NEXT, "ioctl");
    }

    if (request == SNDCTL_DSP_GETOSPACE) {
        int ret = 0;
        if (real_ioctl) {
            ret = real_ioctl(fd, request, arg);
        }
        struct audio_buf_info *info = (struct audio_buf_info *)arg;
        if (info) {
            /* Force available buffer space so Exile III never starves or triggers sounds_fucked */
            info->fragsize = 4096;
            info->fragstotal = 16;
            info->fragments = 16;
            info->bytes = 65536;
        }
        reset_sounds_fucked();
        return ret;
    }

    /* Prevent padsp from blocking the main game engine loop on sound playback */
    if (request == SNDCTL_DSP_POST || request == SNDCTL_DSP_SYNC) {
        reset_sounds_fucked();
        return 0;
    }

    if (request == SNDCTL_DSP_RESET) {
        reset_sounds_fucked();
    }

    if (real_ioctl) {
        return real_ioctl(fd, request, arg);
    }

    return 0;
}

#include <stdlib.h>
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

#define NUM_SIZE_SLOTS 8

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

static CachedGlyph *get_cached_glyph(int is_bold, int font_height, unsigned char c) {
    if (font_height <= 0 || font_height > 120) font_height = 12;

    int slot_idx = -1;
    for (int i = 0; i < NUM_SIZE_SLOTS; i++) {
        if (size_caches[i].in_use &&
            size_caches[i].is_bold == is_bold &&
            size_caches[i].font_height == font_height) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx < 0) {
        for (int i = 0; i < NUM_SIZE_SLOTS; i++) {
            if (!size_caches[i].in_use) {
                slot_idx = i;
                break;
            }
        }
        if (slot_idx < 0) {
            slot_idx = 0;
            for (int ch = 0; ch < 256; ch++) {
                if (size_caches[0].glyphs[ch].buffer) {
                    free(size_caches[0].glyphs[ch].buffer);
                    size_caches[0].glyphs[ch].buffer = NULL;
                }
            }
        }
        memset(&size_caches[slot_idx], 0, sizeof(FontSizeCache));
        size_caches[slot_idx].in_use = 1;
        size_caches[slot_idx].is_bold = is_bold;
        size_caches[slot_idx].font_height = font_height;
    }

    CachedGlyph *cg = &size_caches[slot_idx].glyphs[c];
    if (!cg->valid) {
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

#define FONT_CACHE_SIZE 16
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
    }

    *size = f_size;
    *is_bold = f_bold;
    *ascent = f_ascent;
}

static int x_error_trap = 0;
static int (*orig_x_error_handler)(Display *, XErrorEvent *) = NULL;
static int trap_x_errors(Display *d, XErrorEvent *e) {
    x_error_trap = 1;
    return 0;
}

static int draw_text_aa(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length, int is_image_string) {
    FILE *plog = fopen("/home/lavac/exile3-linux-1/probe.log", "a");
    if (plog) {
        fprintf(plog, "draw_text_aa: dpy=%p d=%lu x=%d y=%d str='%.*s'\n", dpy, (unsigned long)d, x, y, length, string);
        fclose(plog);
    }
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

    int pen_x = x;
    int min_x = x, max_x = x;
    int min_y = y - font_ascent, max_y = y + (font_size - font_ascent);

    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, font_size, c);
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

    min_x -= 1;
    min_y -= 1;
    max_x += 1;
    max_y += 1;

    Window root;
    int rx = 0, ry = 0;
    unsigned int win_w = 0, win_h = 0, border_w = 0, depth = 0;
    x_error_trap = 0;
    orig_x_error_handler = XSetErrorHandler(trap_x_errors);
    Status s = XGetGeometry(dpy, d, &root, &rx, &ry, &win_w, &win_h, &border_w, &depth);
    XSync(dpy, False);
    XSetErrorHandler(orig_x_error_handler);
    if (!s || x_error_trap) {
        return -1;
    }

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > (int)win_w) max_x = (int)win_w;
    if (max_y > (int)win_h) max_y = (int)win_h;

    int bbox_w = max_x - min_x;
    int bbox_h = max_y - min_y;
    if (bbox_w <= 0 || bbox_h <= 0) return 0;

    x_error_trap = 0;
    orig_x_error_handler = XSetErrorHandler(trap_x_errors);

    XImage *img = XGetImage(dpy, d, min_x, min_y, bbox_w, bbox_h, AllPlanes, ZPixmap);
    XSync(dpy, False);
    XSetErrorHandler(orig_x_error_handler);

    if (x_error_trap || !img) {
        if (img) XDestroyImage(img);
        return -1;
    }

    if (img->bits_per_pixel != 24 && img->bits_per_pixel != 32) {
        XDestroyImage(img);
        return -1;
    }

    if (is_image_string) {
        for (int py = 0; py < bbox_h; py++) {
            for (int px = 0; px < bbox_w; px++) {
                XPutPixel(img, px, py, values.background);
            }
        }
    }

    unsigned long fg = values.foreground;
    unsigned int fg_r = (fg >> 16) & 0xFF;
    unsigned int fg_g = (fg >> 8) & 0xFF;
    unsigned int fg_b = fg & 0xFF;

    pen_x = x;
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, font_size, c);
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

                        unsigned int out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
                        unsigned int out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
                        unsigned int out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

                        XPutPixel(img, px, py, (out_r << 16) | (out_g << 8) | out_b);
                    }
                }
            }
        }
        pen_x += cg->advance_x;
    }

    XPutImage(dpy, d, gc, img, 0, 0, min_x, min_y, bbox_w, bbox_h);
    XDestroyImage(img);
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

static real_getpixel_t real_getpixel = NULL;
static real_setpixel_t real_setpixel = NULL;
static real_gettextcolor_t real_gettextcolor = NULL;
static real_getbkcolor_t real_getbkcolor = NULL;
static real_getbkmode_t real_getbkmode = NULL;
static real_gettextalign_t real_gettextalign = NULL;
static real_gettextmetrics_t real_gettextmetrics = NULL;
static real_gettextface_t real_gettextface = NULL;

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
static int palette_cached = 0;

static void update_palette_cache(void) {
    if (!real_getpaletteentries) {
        real_getpaletteentries = (real_getpaletteentries_t)dlsym(RTLD_NEXT, "GetPaletteEntries");
    }
    void *hpal_sym = dlsym(RTLD_DEFAULT, "hpal");
    if (hpal_sym && real_getpaletteentries) {
        void *hpal = *(void **)hpal_sym;
        if (hpal) {
            unsigned int num = real_getpaletteentries(hpal, 0, 256, cached_palette);
            if (num > 0) {
                palette_cached = 1;
            }
        }
    }
}

static uint32_t colorref_to_rgb(uint32_t col) {
    if ((col & 0xFF000000) == 0x01000000) {
        unsigned int idx = col & 0xFF;
        if (!palette_cached) {
            update_palette_cache();
        }
        if (palette_cached) {
            return ((uint32_t)cached_palette[idx].peRed) |
                   ((uint32_t)cached_palette[idx].peGreen << 8) |
                   ((uint32_t)cached_palette[idx].peBlue << 16);
        }
        if (idx == 0 || idx == 252) return 0x00000000;
        if (idx == 255) return 0x00FFFFFF;
        return (idx) | (idx << 8) | (idx << 16);
    }
    return col & 0x00FFFFFF;
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

int MessageBox(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType) {
    if (lpText && strstr(lpText, "/dev/dsp")) {
        return 1;
    }
    if (!real_msgbox) real_msgbox = (real_msgbox_t)dlsym(RTLD_NEXT, "MessageBox");
    return real_msgbox ? real_msgbox(hWnd, lpText, lpCaption, uType) : 1;
}

int MessageBoxA(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType) {
    return MessageBox(hWnd, lpText, lpCaption, uType);
}

static int draw_text_hdc_aa(void *hdc, int x, int y, const char *string, int length) {
    if (!ft_initialized) {
        init_freetype();
    }
    if (ft_failed || !ft_face_regular || length <= 0 || !string) {
        return -1;
    }

    if (!real_gettextmetrics) real_gettextmetrics = (real_gettextmetrics_t)dlsym(RTLD_NEXT, "GetTextMetrics");
    if (!real_gettextcolor) real_gettextcolor = (real_gettextcolor_t)dlsym(RTLD_NEXT, "GetTextColor");
    if (!real_getbkcolor) real_getbkcolor = (real_getbkcolor_t)dlsym(RTLD_NEXT, "GetBkColor");
    if (!real_getbkmode) real_getbkmode = (real_getbkmode_t)dlsym(RTLD_NEXT, "GetBkMode");
    if (!real_gettextalign) real_gettextalign = (real_gettextalign_t)dlsym(RTLD_NEXT, "GetTextAlign");
    if (!real_getpixel) real_getpixel = (real_getpixel_t)dlsym(RTLD_NEXT, "GetPixel");
    if (!real_setpixel) real_setpixel = (real_setpixel_t)dlsym(RTLD_NEXT, "SetPixel");
    if (!real_gettextface) real_gettextface = (real_gettextface_t)dlsym(RTLD_NEXT, "GetTextFace");

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

    char facename[64] = {0};
    if (real_gettextface) {
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

    typedef void *(*real_twin_getdisplaydata_t)(void);
    typedef void *(*real_twin_getwindowdata_t)(void *hwnd);
    typedef void *(*real_windowfromdc_t)(void *hdc);
    typedef int (*real_getdcorgex_t)(void *hdc, void *pt);

    static real_twin_getdisplaydata_t p_getdisplaydata = NULL;
    static real_twin_getwindowdata_t p_getwindowdata = NULL;
    static real_windowfromdc_t real_windowfromdc = NULL;
    static real_getdcorgex_t p_getdcorgex = NULL;

    if (!p_getdisplaydata) p_getdisplaydata = (real_twin_getdisplaydata_t)dlsym(RTLD_DEFAULT, "TWIN_GetDisplayData");
    if (!p_getwindowdata) p_getwindowdata = (real_twin_getwindowdata_t)dlsym(RTLD_DEFAULT, "TWIN_GetWindowData");
    if (!real_windowfromdc) real_windowfromdc = (real_windowfromdc_t)dlsym(RTLD_DEFAULT, "WindowFromDC");
    if (!p_getdcorgex) p_getdcorgex = (real_getdcorgex_t)dlsym(RTLD_DEFAULT, "GetDCOrgEx");

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

    int total_width = 0;
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, render_height, c);
        if (cg) total_width += cg->advance_x;
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
    Display *dpy = p_getdisplaydata ? (Display *)p_getdisplaydata() : NULL;
    void *hwnd = real_windowfromdc ? real_windowfromdc(hdc) : NULL;
    Drawable win_d = (p_getwindowdata && hwnd) ? (Drawable)p_getwindowdata(hwnd) : 0;

    if (dpy && win_d) {
        int dev_pen_x = pen_x + org[0];
        int dev_pen_y = pen_y + org[1];
        int dev_y = y + org[1];

        int min_x = 0x7FFFFFFF, min_y = 0x7FFFFFFF;
        int max_x = -0x7FFFFFFF, max_y = -0x7FFFFFFF;
        int cur_x = dev_pen_x;

        for (int i = 0; i < length; i++) {
            unsigned char c = (unsigned char)string[i];
            CachedGlyph *cg = get_cached_glyph(is_bold, render_height, c);
            if (!cg || cg->rows == 0 || cg->width == 0) {
                if (cg) cur_x += cg->advance_x;
                continue;
            }

            int gx0 = cur_x + cg->bitmap_left;
            int gy0 = dev_pen_y - cg->bitmap_top;
            int gx1 = gx0 + cg->width;
            int gy1 = gy0 + cg->rows;

            if (gx0 < min_x) min_x = gx0;
            if (gy0 < min_y) min_y = gy0;
            if (gx1 > max_x) max_x = gx1;
            if (gy1 > max_y) max_y = gy1;

            cur_x += cg->advance_x;
        }

        if (bk_mode == 2) {
            int bg_left = dev_pen_x;
            int bg_right = dev_pen_x + total_width;
            int bg_top = dev_y;
            int bg_bottom = dev_y + font_height;

            if (bg_left < min_x) min_x = bg_left;
            if (bg_top < min_y) min_y = bg_top;
            if (bg_right > max_x) max_x = bg_right;
            if (bg_bottom > max_y) max_y = bg_bottom;
        }

        if (min_x > max_x || min_y > max_y) {
            return 0; // Nothing visible to draw
        }

        min_x -= 1;
        min_y -= 1;
        max_x += 1;
        max_y += 1;

        Window root;
        int rx = 0, ry = 0;
        unsigned int win_w = 0, win_h = 0, border_w = 0, depth = 0;

        x_error_trap = 0;
        orig_x_error_handler = XSetErrorHandler(trap_x_errors);
        Status s = XGetGeometry(dpy, win_d, &root, &rx, &ry, &win_w, &win_h, &border_w, &depth);
        XSync(dpy, False);
        XSetErrorHandler(orig_x_error_handler);

        if (s && !x_error_trap && depth >= 24) {
            if (min_x < 0) min_x = 0;
            if (min_y < 0) min_y = 0;
            if (max_x > (int)win_w) max_x = (int)win_w;
            if (max_y > (int)win_h) max_y = (int)win_h;
            int bbox_w = max_x - min_x;
            int bbox_h = max_y - min_y;

            if (bbox_w > 0 && bbox_h > 0) {
                struct timeval t0, t1;
                gettimeofday(&t0, NULL);

                x_error_trap = 0;
                orig_x_error_handler = XSetErrorHandler(trap_x_errors);
                XImage *img = XGetImage(dpy, win_d, min_x, min_y, bbox_w, bbox_h, AllPlanes, ZPixmap);
                XSync(dpy, False);
                XSetErrorHandler(orig_x_error_handler);

                if (img && !x_error_trap && (img->bits_per_pixel == 24 || img->bits_per_pixel == 32)) {
                    if (bk_mode == 2) {
                        unsigned long bk_pixel = (bk_r << 16) | (bk_g << 8) | bk_b;
                        int bx0 = dev_pen_x - min_x;
                        int by0 = dev_y - min_y;
                        int bx1 = bx0 + total_width;
                        int by1 = by0 + font_height;

                        if (bx0 < 0) bx0 = 0;
                        if (by0 < 0) by0 = 0;
                        if (bx1 > bbox_w) bx1 = bbox_w;
                        if (by1 > bbox_h) by1 = bbox_h;

                        for (int py = by0; py < by1; py++) {
                            for (int px = bx0; px < bx1; px++) {
                                XPutPixel(img, px, py, bk_pixel);
                            }
                        }
                    }

                    unsigned long fg_pixel = (fg_r << 16) | (fg_g << 8) | fg_b;
                    cur_x = dev_pen_x;

                    for (int i = 0; i < length; i++) {
                        unsigned char c = (unsigned char)string[i];
                        CachedGlyph *cg = get_cached_glyph(is_bold, render_height, c);
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
                                        XPutPixel(img, px, py, fg_pixel);
                                    } else {
                                        unsigned long bg = XGetPixel(img, px, py);
                                        unsigned int cur_bg_r = (bg >> 16) & 0xFF;
                                        unsigned int cur_bg_g = (bg >> 8) & 0xFF;
                                        unsigned int cur_bg_b = bg & 0xFF;

                                        unsigned int out_r = (fg_r * alpha + cur_bg_r * (255 - alpha)) / 255;
                                        unsigned int out_g = (fg_g * alpha + cur_bg_g * (255 - alpha)) / 255;
                                        unsigned int out_b = (fg_b * alpha + cur_bg_b * (255 - alpha)) / 255;

                                        XPutPixel(img, px, py, (out_r << 16) | (out_g << 8) | out_b);
                                    }
                                }
                            }
                        }
                        cur_x += cg->advance_x;
                    }

                    GC local_gc = XCreateGC(dpy, win_d, 0, NULL);
                    if (local_gc) {
                        x_error_trap = 0;
                        orig_x_error_handler = XSetErrorHandler(trap_x_errors);
                        XPutImage(dpy, win_d, local_gc, img, 0, 0, min_x, min_y, bbox_w, bbox_h);
                        XSync(dpy, False);
                        XSetErrorHandler(orig_x_error_handler);
                        XFreeGC(dpy, local_gc);
                    }
                    XDestroyImage(img);

                    static int debug_timing = -1;
                    if (debug_timing == -1) {
                        debug_timing = (getenv("EXILE_DEBUG_TIMING") != NULL) ? 1 : 0;
                    }
                    if (debug_timing) {
                        gettimeofday(&t1, NULL);
                        long micros = (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_usec - t0.tv_usec);
                        FILE *tf = fopen("/home/lavac/exile3-linux-1/timing.log", "a");
                        if (tf) {
                            fprintf(tf, "FAST_BLIT: %ld us (w=%d h=%d len=%d '%.*s')\n",
                                    micros, bbox_w, bbox_h, length, length, string);
                            fclose(tf);
                        }
                    }

                    return 0; // Ultra-fast single block blit succeeded!
                }
                if (img) XDestroyImage(img);
            }
        }
    }

    if (bk_mode == 2) {
        typedef void *HGDIOBJ;
        typedef struct { int left, top, right, bottom; } RECT;
        typedef HGDIOBJ (*real_createsolidbrush_t)(uint32_t crColor);
        typedef int (*real_fillrect_t)(void *hdc, const RECT *lprc, HGDIOBJ hbr);
        typedef int (*real_deleteobject_t)(HGDIOBJ hObject);

        static real_createsolidbrush_t real_createsolidbrush = NULL;
        static real_fillrect_t real_fillrect = NULL;
        static real_deleteobject_t real_deleteobject = NULL;

        if (!real_createsolidbrush) real_createsolidbrush = (real_createsolidbrush_t)dlsym(RTLD_NEXT, "CreateSolidBrush");
        if (!real_fillrect) real_fillrect = (real_fillrect_t)dlsym(RTLD_NEXT, "FillRect");
        if (!real_deleteobject) real_deleteobject = (real_deleteobject_t)dlsym(RTLD_NEXT, "DeleteObject");

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
        unsigned char c = (unsigned char)string[i];
        CachedGlyph *cg = get_cached_glyph(is_bold, render_height, c);
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
                        unsigned int out_r = (fg_r * alpha + bk_r * (255 - alpha)) / 255;
                        unsigned int out_g = (fg_g * alpha + bk_g * (255 - alpha)) / 255;
                        unsigned int out_b = (fg_b * alpha + bk_b * (255 - alpha)) / 255;
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

                        unsigned int out_r = (fg_r * alpha + cur_bg_r * (255 - alpha)) / 255;
                        unsigned int out_g = (fg_g * alpha + cur_bg_g * (255 - alpha)) / 255;
                        unsigned int out_b = (fg_b * alpha + cur_bg_b * (255 - alpha)) / 255;

                        uint32_t blended = (out_r) | (out_g << 8) | (out_b << 16);
                        real_setpixel(hdc, px, py, blended);
                    }
                }
            }
        }
        pen_x += cg->advance_x;
    }

    return 0;
}

int DrawText(void *hdc, const char *lpString, int nCount, void *lpRect, unsigned int uFormat) {
    if (!real_drawtext) real_drawtext = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawText");
    return real_drawtext ? real_drawtext(hdc, lpString, nCount, lpRect, uFormat) : 0;
}

int DrawTextA(void *hdc, const char *lpString, int nCount, void *lpRect, unsigned int uFormat) {
    if (!real_drawtexta) real_drawtexta = (real_drawtext_t)dlsym(RTLD_NEXT, "DrawTextA");
    return real_drawtexta ? real_drawtexta(hdc, lpString, nCount, lpRect, uFormat) : 0;
}

int ExtTextOut(void *hdc, int x, int y, unsigned int fuOptions, const void *lprc, const char *lpString, unsigned int cbCount, const int *lpDx) {
    if (!real_exttextout) real_exttextout = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOut");

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
        if (draw_text_hdc_aa(hdc, x, y, lpString, (int)cbCount) == 0) {
            return 1;
        }
    }

    return real_exttextout ? real_exttextout(hdc, x, y, fuOptions, lprc, lpString, cbCount, lpDx) : 0;
}

int ExtTextOutA(void *hdc, int x, int y, unsigned int fuOptions, const void *lprc, const char *lpString, unsigned int cbCount, const int *lpDx) {
    if (!real_exttextouta) real_exttextouta = (real_exttextout_t)dlsym(RTLD_NEXT, "ExtTextOutA");

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
        if (draw_text_hdc_aa(hdc, x, y, lpString, (int)cbCount) == 0) {
            return 1;
        }
    }

    return real_exttextouta ? real_exttextouta(hdc, x, y, fuOptions, lprc, lpString, cbCount, lpDx) : 0;
}

int TextOut(void *hdc, int x, int y, const char *lpString, int nCount) {
    if (!real_textout) real_textout = (real_textout_t)dlsym(RTLD_NEXT, "TextOut");

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
        int len = nCount >= 0 ? nCount : (lpString ? (int)strlen(lpString) : 0);
        if (draw_text_hdc_aa(hdc, x, y, lpString, len) == 0) {
            return 1;
        }
    }

    return real_textout ? real_textout(hdc, x, y, lpString, nCount) : 0;
}

int TextOutA(void *hdc, int x, int y, const char *lpString, int nCount) {
    if (!real_textouta) real_textouta = (real_textout_t)dlsym(RTLD_NEXT, "TextOutA");

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
        int len = nCount >= 0 ? nCount : (lpString ? (int)strlen(lpString) : 0);
        if (draw_text_hdc_aa(hdc, x, y, lpString, len) == 0) {
            return 1;
        }
    }

    return real_textouta ? real_textouta(hdc, x, y, lpString, nCount) : 0;
}

int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    if (!real_xdrawstring) {
        real_xdrawstring = (real_xdrawstring_t)dlsym(RTLD_NEXT, "XDrawString");
    }

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
        if (draw_text_aa(dpy, d, gc, x, y, string, length, 0) == 0) {
            return 0;
        }
    }

    if (real_xdrawstring) {
        return real_xdrawstring(dpy, d, gc, x, y, string, length);
    }
    return 0;
}

int XDrawImageString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    if (!real_xdrawimagestring) {
        real_xdrawimagestring = (real_xdrawimagestring_t)dlsym(RTLD_NEXT, "XDrawImageString");
    }

    const char *no_aa = getenv("EXILE_NO_AA");
    if (!no_aa || strcmp(no_aa, "1") != 0) {
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
    FILE *plog = fopen("/home/lavac/exile3-linux-1/probe.log", "a");
    if (plog) {
        fprintf(plog, "init_exile3_shim called!\n");
        fclose(plog);
    }
    real_ioctl = (real_ioctl_t)dlsym(RTLD_NEXT, "ioctl");
    real_msgbox = (real_msgbox_t)dlsym(RTLD_NEXT, "MessageBox");
    real_xdrawstring = (real_xdrawstring_t)dlsym(RTLD_NEXT, "XDrawString");
    real_xdrawimagestring = (real_xdrawimagestring_t)dlsym(RTLD_NEXT, "XDrawImageString");
    reset_sounds_fucked();
    init_freetype();
}


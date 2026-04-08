#include "traffic_game.h"

int text_len(const char *s) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

const uint8_t *glyph_for_char(char ch) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t dash[7]  = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t plus[7]  = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t equal[7] = {0, 31, 0, 31, 0, 0, 0};
    static const uint8_t slash[7] = {1, 2, 4, 8, 16, 0, 0};
    static const uint8_t amp[7]   = {12, 18, 20, 8, 21, 18, 13};
    static const uint8_t colon[7] = {0, 4, 0, 0, 4, 0, 0};
    static const uint8_t zero[7]  = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t one[7]   = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t two[7]   = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t three[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t four[7]  = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t five[7]  = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t six[7]   = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t seven[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t eight[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t nine[7]  = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t a[7]     = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t b[7]     = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t c[7]     = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t d[7]     = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t e[7]     = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t f[7]     = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t g[7]     = {14, 17, 16, 23, 17, 17, 14};
    static const uint8_t h[7]     = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t i[7]     = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t k[7]     = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t l[7]     = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t m[7]     = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t n[7]     = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t o[7]     = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t p[7]     = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t q[7]     = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t r[7]     = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t s[7]     = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t t[7]     = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t u[7]     = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t v[7]     = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t w[7]     = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t y[7]     = {17, 17, 10, 4, 4, 4, 4};
    static const uint8_t x[7]     = {17, 10, 4, 4, 4, 10, 17};
    static const uint8_t z[7]     = {31, 1, 2, 4, 8, 16, 31};

    switch (ch) {
        case 'A': return a; case 'B': return b; case 'C': return c; case 'D': return d;
        case 'E': return e; case 'F': return f; case 'G': return g; case 'H': return h;
        case 'I': return i; case 'K': return k; case 'L': return l; case 'M': return m;
        case 'N': return n; case 'O': return o; case 'P': return p; case 'Q': return q;
        case 'R': return r; case 'S': return s; case 'T': return t; case 'U': return u;
        case 'V': return v; case 'W': return w; case 'X': return x; case 'Y': return y;
        case 'Z': return z;
        case '0': return zero; case '1': return one; case '2': return two; case '3': return three;
        case '4': return four; case '5': return five; case '6': return six; case '7': return seven;
        case '8': return eight; case '9': return nine;
        case '-': return dash; case '+': return plus; case '=': return equal;
        case '&': return amp;
        case '/': return slash; case ':': return colon; case ' ': return blank;
        default:  return blank;
    }
}

void draw_char(int x, int y, char ch, short color, int scale) {
    const uint8_t *rows = glyph_for_char(ch);
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 5; c++) {
            if ((rows[r] & (1 << (4 - c))) != 0) {
                draw_box(x + c * scale, y + r * scale,
                         x + c * scale + scale - 1, y + r * scale + scale - 1, color);
            }
        }
    }
}

void draw_text(int x, int y, const char *text, short color, int scale) {
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(x + i * scale * 6, y, text[i], color, scale);
    }
}

void draw_text_centered(int y, const char *text, short color, int scale) {
    int width = text_len(text) * scale * 6 - scale;
    int x = (SCREEN_W - width) / 2;
    draw_text(x, y, text, color, scale);
}

void draw_text_in_box(int x1, int x2, int y, const char *text, short color, int scale) {
    int width = text_len(text) * scale * 6 - scale;
    int x = x1 + ((x2 - x1 + 1) - width) / 2;
    draw_text(x, y, text, color, scale);
}

void format_int_text(int value, char *buf) {
    int idx = 0;
    if (value == 0) {
        buf[idx++] = '0';
    } else {
        int v = value;
        if (v < 0) v = -v;
        while (v > 0 && idx < 10) {
            buf[idx++] = (char)('0' + (v % 10));
            v /= 10;
        }
        if (value < 0 && idx < 11) {
            buf[idx++] = '-';
        }
    }
    for (int i = 0; i < idx / 2; i++) {
        char t = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = t;
    }
    buf[idx] = '\0';
}

void draw_int(int x, int y, int value, short color, int scale) {
    char buf[12];
    format_int_text(value, buf);
    draw_text(x, y, buf, color, scale);
}

void draw_int_centered(int y, int value, short color, int scale) {
    char buf[12];
    format_int_text(value, buf);
    draw_text_centered(y, buf, color, scale);
}

void draw_int_in_box(int x1, int x2, int y, int value, short color, int scale) {
    char buf[12];
    format_int_text(value, buf);
    draw_text_in_box(x1, x2, y, buf, color, scale);
}

int text_width(const char *text, int scale) {
    return text_len(text) * scale * 6 - scale;
}

void draw_text_right(int x_right, int y, const char *text, short color, int scale) {
    int width = text_width(text, scale);
    draw_text(x_right - width + 1, y, text, color, scale);
}

void draw_int_right(int x_right, int y, int value, short color, int scale) {
    char buf[12];
    format_int_text(value, buf);
    draw_text_right(x_right, y, buf, color, scale);
}

void copy_text(char *dst, const char *src) {
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
}

void append_text(char *dst, const char *src) {
    while (*dst != '\0') dst++;
    copy_text(dst, src);
}

void format_status_count_text(const char *status, int count, char *buf) {
    char num[12];
    buf[0] = '\0';
    copy_text(buf, status);
    append_text(buf, " ");
    format_int_text(count, num);
    append_text(buf, num);
}


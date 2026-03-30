#include <stdbool.h>
#include <stdint.h>

#define PS2_BASE            0xFF200100
#define TIMER1_BASE         0xFF202000
#define HEX3_HEX0_BASE      0xFF200020
#define HEX5_HEX4_BASE      0xFF200030
#define PIXEL_CTRL_BASE     0xFF203020
#define PIXEL_BUF_BASE      0x08000000

// Screen constants
#define SCREEN_W 320
#define SCREEN_H 240

// Colors
#define BLACK      0x0000
#define WHITE      0xFFFF
#define RED        0xF800
#define GREEN      0x07E0
#define YELLOW     0xFFE0
#define CYAN       0x07FF
#define MAGENTA    0xF81F
#define ORANGE     0xFD20
#define ROAD       0x39E7
#define GRASS      0x0600
#define DARKGRAY   0x4208
#define DARKRED    0x8000
#define DARKGREEN  0x0400
#define DARKYELLOW 0x8400
#define CITY_BG    0x2104
#define SIDEWALK   0xBDF7
#define ROAD_EDGE  0x6B4D
#define BUILDING   0x52AA

#define MAX_CARS 32
#define CAR_LONG 12
#define CAR_SHORT 8
#define STOP_N 72
#define STOP_S 166
#define STOP_W 112
#define STOP_E 206
#define CONFLICT_X1 134
#define CONFLICT_X2 186
#define CONFLICT_Y1 94
#define CONFLICT_Y2 146

#define TICK_COUNTS 5000000U
#define NS_GREEN_TICKS 22
#define NS_YELLOW_TICKS 20
#define EW_GREEN_TICKS 22
#define EW_YELLOW_TICKS 20
#define ALL_RED_TICKS 4
#define MAX_GREEN_TICKS 40
#define ROUND_TICKS 1200
#define PASS_SCORE 25
#define WAIT_DISPLAY_DIVISOR 20
#define WAIT_PENALTY_DIVISOR 40

typedef enum {
    NS_GREEN = 0,
    NS_YELLOW,
    ALL_RED,
    EW_GREEN,
    EW_YELLOW
} LightState;

typedef enum {
    AUTO_MODE = 0,
    MANUAL_MODE
} ControlMode;

typedef enum {
    SCENE_TITLE = 0,
    SCENE_INSTRUCTIONS,
    SCENE_PLAYING,
    SCENE_PAUSED,
    SCENE_GAME_OVER
} Scene;

typedef enum {
    END_TIME = 0,
    END_CRASH
} EndReason;

typedef enum {
    DIR_NORTH = 0,
    DIR_SOUTH,
    DIR_WEST,
    DIR_EAST
} Direction;

typedef struct {
    bool active;
    Direction dir;
    int x;
    int y;
    bool scored;
    short color;
} Car;

typedef void (*SceneRenderer)(void);

volatile int *ps2_ptr   = (int *)PS2_BASE;
volatile int *timer_ptr = (int *)TIMER1_BASE;
volatile int *hex30_ptr = (int *)HEX3_HEX0_BASE;
volatile int *hex54_ptr = (int *)HEX5_HEX4_BASE;
volatile int *pixel_ctrl_ptr = (int *)PIXEL_CTRL_BASE;
volatile short *pixel_buffer = (short *)PIXEL_BUF_BASE;
#define BACK_BUF_BASE 0x02000000

static LightState light_state = NS_GREEN;
static ControlMode mode = AUTO_MODE;
static Scene scene = SCENE_TITLE;
static int phase_ticks = 0;
static uint32_t rng_state = 0x2432026u;
static Car cars[MAX_CARS];
static int score = 0;
static int best_score = 0;
static int passed = 0;
static int wait_ticks_total = 0;
static int elapsed_ticks = 0;
static EndReason end_reason = END_TIME;
static int crash_x = SCREEN_W / 2;
static int crash_y = SCREEN_H / 2;
static int queue_n = 0;
static int queue_s = 0;
static int queue_w = 0;
static int queue_e = 0;
static LightState next_green_state = NS_GREEN;

void clear_screen(short color);
void update_hex_timer(void);
void draw_static_scene(SceneRenderer renderer);
void draw_title_scene(void);
void draw_instructions_scene(void);
void draw_paused_scene(void);
void draw_game_over_scene(void);

void wait_for_vsync(void) {
    *pixel_ctrl_ptr = 1;
    while ((*(pixel_ctrl_ptr + 3) & 0x1) != 0) {
    }
}

void present_frame(void) {
    wait_for_vsync();
    pixel_buffer = (volatile short *)(uintptr_t)(*(pixel_ctrl_ptr + 1));
}

void video_init(void) {
    *(pixel_ctrl_ptr + 1) = BACK_BUF_BASE;
    pixel_buffer = (volatile short *)(uintptr_t)BACK_BUF_BASE;
    clear_screen(BLACK);
    present_frame();
    clear_screen(BLACK);
}

// VGA helpers
void plot_pixel(int x, int y, short color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    volatile short *addr = (volatile short *)((uintptr_t)pixel_buffer + (y << 10) + (x << 1));
    *addr = color;
}

void draw_box(int x1, int y1, int x2, int y2, short color) {
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= SCREEN_W) x2 = SCREEN_W - 1;
    if (y2 >= SCREEN_H) y2 = SCREEN_H - 1;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            plot_pixel(x, y, color);
        }
    }
}

void clear_screen(short color) {
    for (int y = 0; y < SCREEN_H; y++) {
        volatile short *row_ptr = (volatile short *)((uintptr_t)pixel_buffer + (y << 10));
        for (int x = 0; x < SCREEN_W; x++) {
            row_ptr[x] = color;
        }
    }
}

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

void draw_int(int x, int y, int value, short color, int scale) {
    char buf[12];
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
    draw_text(x, y, buf, color, scale);
}

void draw_int_centered(int y, int value, short color, int scale) {
    char buf[12];
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
    draw_text_centered(y, buf, color, scale);
}

// Timer helpers w/ 100 MHz clock
void timer_init(uint32_t period_counts) {
    *(timer_ptr + 1) = 0x8;
    *(timer_ptr + 2) = (int)(period_counts & 0xFFFFu);
    *(timer_ptr + 3) = (int)((period_counts >> 16) & 0xFFFFu);
    *(timer_ptr + 0) = 0;
    *(timer_ptr + 1) = 0x6;
}

bool timer_expired(void) {
    int status = *(timer_ptr + 0);
    if ((status & 0x1) != 0) {
        *(timer_ptr + 0) = 0;
        return true;
    }
    return false;
}

// PS/2 polling
int ps2_get_make_code(void) {
    static bool break_pending = false;
    static bool ext_pending = false;

    int data = *ps2_ptr;
    int rvalid = data & 0x8000;
    if (rvalid == 0) return -1;

    uint8_t byte = (uint8_t)(data & 0xFF);

    if (byte == 0xF0) {
        break_pending = true;
        return -1;
    }

    if (byte == 0xE0) {
        ext_pending = true;
        return -1;
    }

    if (break_pending) {
        break_pending = false;
        ext_pending = false;
        return -1;
    }

    int code;
    if (ext_pending) {
        code = 0xE000 | byte;
        ext_pending = false;
    } else {
        code = byte;
    }

    return code;
}

uint32_t next_rand(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

short random_car_color(void) {
    static const short palette[6] = {
        0xF800, /* red */
        0x07E0, /* green */
        0x001F, /* blue */
        0xFD20, /* orange */
        0xF81F, /* magenta */
        0x07FF  /* cyan */
    };
    return palette[next_rand() % 6u];
}

bool is_green_for_dir(Direction dir) {
    if (dir == DIR_NORTH || dir == DIR_SOUTH) {
        return light_state == NS_GREEN;
    }
    return light_state == EW_GREEN;
}

bool passed_stop_line(const Car *car) {
    if (car->dir == DIR_NORTH) return car->y >= STOP_N;
    if (car->dir == DIR_SOUTH) return car->y <= STOP_S;
    if (car->dir == DIR_WEST) return car->x >= STOP_W;
    return car->x <= STOP_E;
}

int car_width(const Car *car) {
    if (car->dir == DIR_NORTH || car->dir == DIR_SOUTH) {
        return CAR_SHORT;
    }
    return CAR_LONG;
}

int car_height(const Car *car) {
    if (car->dir == DIR_NORTH || car->dir == DIR_SOUTH) {
        return CAR_LONG;
    }
    return CAR_SHORT;
}

bool rect_overlap(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2) {
    if (ax2 < bx1 || bx2 < ax1) return false;
    if (ay2 < by1 || by2 < ay1) return false;
    return true;
}

bool cars_overlap(const Car *a, const Car *b) {
    int ax1 = a->x;
    int ay1 = a->y;
    int ax2 = a->x + car_width(a) - 1;
    int ay2 = a->y + car_height(a) - 1;
    int bx1 = b->x;
    int by1 = b->y;
    int bx2 = b->x + car_width(b) - 1;
    int by2 = b->y + car_height(b) - 1;
    return rect_overlap(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
}

void reset_round(void) {
    light_state = NS_GREEN;
    next_green_state = NS_GREEN;
    mode = AUTO_MODE;
    phase_ticks = 0;
    score = 0;
    passed = 0;
    wait_ticks_total = 0;
    elapsed_ticks = 0;
    end_reason = END_TIME;
    crash_x = SCREEN_W / 2;
    crash_y = SCREEN_H / 2;
    queue_n = 0;
    queue_s = 0;
    queue_w = 0;
    queue_e = 0;
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].active = false;
        cars[i].scored = false;
    }
    update_hex_timer();
}

int spawn_chance_percent(void) {
    int level = elapsed_ticks / 260;
    int chance = 14 + level * 2;
    if (chance > 28) {
        chance = 28;
    }
    return chance;
}

void maybe_spawn_car(void) {
    if ((int)(next_rand() % 100u) >= spawn_chance_percent()) {
        return;
    }

    Direction dir = (Direction)(next_rand() % 4u);
    int sx = 0;
    int sy = 0;
    if (dir == DIR_NORTH) { sx = 146; sy = -CAR_LONG; }
    if (dir == DIR_SOUTH) { sx = 166; sy = SCREEN_H + CAR_LONG; }
    if (dir == DIR_WEST)  { sx = -CAR_LONG; sy = 126; }
    if (dir == DIR_EAST)  { sx = SCREEN_W + CAR_LONG; sy = 106; }

    for (int j = 0; j < MAX_CARS; j++) {
        if (!cars[j].active || cars[j].dir != dir) continue;
        if (dir == DIR_NORTH && cars[j].y < 22) return;
        if (dir == DIR_SOUTH && cars[j].y > SCREEN_H - 22) return;
        if (dir == DIR_WEST && cars[j].x < 22) return;
        if (dir == DIR_EAST && cars[j].x > SCREEN_W - 22) return;
    }

    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) {
            cars[i].active = true;
            cars[i].dir = dir;
            cars[i].x = sx;
            cars[i].y = sy;
            cars[i].scored = false;
            cars[i].color = random_car_color();
            return;
        }
    }
}

void update_queue_lengths(void) {
    queue_n = 0;
    queue_s = 0;
    queue_w = 0;
    queue_e = 0;

    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active || passed_stop_line(&cars[i])) {
            continue;
        }

        if (cars[i].dir == DIR_NORTH) queue_n++;
        if (cars[i].dir == DIR_SOUTH) queue_s++;
        if (cars[i].dir == DIR_WEST)  queue_w++;
        if (cars[i].dir == DIR_EAST)  queue_e++;
    }
}

bool detect_crash(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) continue;
        for (int j = i + 1; j < MAX_CARS; j++) {
            if (!cars[j].active) continue;
            if (!cars_overlap(&cars[i], &cars[j])) continue;
            crash_x = (cars[i].x + cars[j].x) / 2;
            crash_y = (cars[i].y + cars[j].y) / 2;
            return true;
        }
    }
    return false;
}

int axis_for_dir(Direction dir) {
    if (dir == DIR_NORTH || dir == DIR_SOUTH) {
        return 0;
    }
    return 1;
}

bool car_hits_conflict_zone_at(const Car *car, int x, int y) {
    int x2 = x + car_width(car) - 1;
    int y2 = y + car_height(car) - 1;
    return rect_overlap(x, y, x2, y2, CONFLICT_X1, CONFLICT_Y1, CONFLICT_X2, CONFLICT_Y2);
}

bool conflict_zone_blocked(const Car *car, int nx, int ny) {
    if (!car_hits_conflict_zone_at(car, nx, ny)) {
        return false;
    }

    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active || &cars[i] == car) {
            continue;
        }
        if (!car_hits_conflict_zone_at(&cars[i], cars[i].x, cars[i].y)) {
            continue;
        }
        if (axis_for_dir(cars[i].dir) != axis_for_dir(car->dir)) {
            return true;
        }
    }
    return false;
}

void update_score(void) {
    score = passed * PASS_SCORE - (wait_ticks_total / WAIT_PENALTY_DIVISOR);
    if (score < 0) {
        score = 0;
    }
    if (score > best_score) {
        best_score = score;
    }
}

int wait_seconds_total(void) {
    return wait_ticks_total / WAIT_DISPLAY_DIVISOR;
}

int remaining_round_seconds(void) {
    int remaining_ticks = ROUND_TICKS - elapsed_ticks;
    if (remaining_ticks < 0) {
        remaining_ticks = 0;
    }
    return (remaining_ticks + 19) / 20;
}

int hex_digit_pattern(int digit) {
    static const int patterns[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    if (digit < 0 || digit > 9) {
        return 0x00;
    }
    return patterns[digit];
}

void update_hex_timer(void) {
    int total_seconds = remaining_round_seconds();
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    int hex0 = hex_digit_pattern(seconds % 10);
    int hex1 = hex_digit_pattern((seconds / 10) % 10);
    int hex2 = hex_digit_pattern(minutes % 10);
    int hex3 = hex_digit_pattern((minutes / 10) % 10);

    *hex30_ptr = hex0 | (hex1 << 8) | (hex2 << 16) | (hex3 << 24);
    *hex54_ptr = 0;
}

bool blocked_by_leader(const Car *car, int nx, int ny) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active || &cars[i] == car || cars[i].dir != car->dir) {
            continue;
        }

        if (car->dir == DIR_NORTH && cars[i].y > car->y && cars[i].y - ny < 16) return true;
        if (car->dir == DIR_SOUTH && cars[i].y < car->y && ny - cars[i].y < 16) return true;
        if (car->dir == DIR_WEST  && cars[i].x > car->x && cars[i].x - nx < 16) return true;
        if (car->dir == DIR_EAST  && cars[i].x < car->x && nx - cars[i].x < 16) return true;
    }
    return false;
}

const char *light_state_label(void) {
    switch (light_state) {
        case NS_GREEN:  return "NS G";
        case NS_YELLOW: return "NS Y";
        case ALL_RED:   return "ALL R";
        case EW_GREEN:  return "EW G";
        case EW_YELLOW: return "EW Y";
        default:        return "ALL R";
    }
}

int phase_countdown_ticks(void) {
    int remaining = 0;
    if (mode == MANUAL_MODE && (light_state == NS_GREEN || light_state == EW_GREEN ||
        (light_state == ALL_RED && next_green_state == ALL_RED))) {
        return 0;
    }
    switch (light_state) {
        case NS_GREEN:
            remaining = NS_GREEN_TICKS - phase_ticks;
            break;
        case NS_YELLOW:
            remaining = NS_YELLOW_TICKS - phase_ticks;
            break;
        case ALL_RED:
            remaining = ALL_RED_TICKS - phase_ticks;
            break;
        case EW_GREEN:
            remaining = EW_GREEN_TICKS - phase_ticks;
            break;
        case EW_YELLOW:
            remaining = EW_YELLOW_TICKS - phase_ticks;
            break;
        default:
            remaining = 0;
            break;
    }
    if (remaining < 0) {
        remaining = 0;
    }
    return remaining;
}

int phase_countdown_tenths(void) {
    return phase_countdown_ticks() / 2;
}

void update_cars(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) continue;

        int nx = cars[i].x;
        int ny = cars[i].y;
        if (cars[i].dir == DIR_NORTH) ny += 2;
        if (cars[i].dir == DIR_SOUTH) ny -= 2;
        if (cars[i].dir == DIR_WEST)  nx += 2;
        if (cars[i].dir == DIR_EAST)  nx -= 2;

        bool stopped_by_light = false;
        if (!is_green_for_dir(cars[i].dir) && !passed_stop_line(&cars[i])) {
            if (cars[i].dir == DIR_NORTH && ny >= STOP_N) stopped_by_light = true;
            if (cars[i].dir == DIR_SOUTH && ny <= STOP_S) stopped_by_light = true;
            if (cars[i].dir == DIR_WEST  && nx >= STOP_W) stopped_by_light = true;
            if (cars[i].dir == DIR_EAST  && nx <= STOP_E) stopped_by_light = true;
        }

        if (stopped_by_light || blocked_by_leader(&cars[i], nx, ny) ||
            (mode == AUTO_MODE && conflict_zone_blocked(&cars[i], nx, ny))) {
            wait_ticks_total++;
            continue;
        }

        cars[i].x = nx;
        cars[i].y = ny;

        if (!cars[i].scored && passed_stop_line(&cars[i])) {
            cars[i].scored = true;
            passed++;
        }

        if (cars[i].x < -24 || cars[i].x > SCREEN_W + 24 || cars[i].y < -24 || cars[i].y > SCREEN_H + 24) {
            cars[i].active = false;
        }
    }
    update_queue_lengths();
    update_score();
}

void update_lights_auto(void) {
    phase_ticks++;
    if (light_state == NS_GREEN) {
        int ns_load = queue_n + queue_s;
        int ew_load = queue_w + queue_e;
        bool reached_max = phase_ticks >= MAX_GREEN_TICKS;
        bool should_yield = (phase_ticks >= NS_GREEN_TICKS) &&
                            ((ew_load > ns_load) || (ew_load > 0 && ns_load == 0) || reached_max);
        if (should_yield) {
            light_state = NS_YELLOW;
            next_green_state = EW_GREEN;
            phase_ticks = 0;
        }
    } else if (light_state == EW_GREEN) {
        int ns_load = queue_n + queue_s;
        int ew_load = queue_w + queue_e;
        bool reached_max = phase_ticks >= MAX_GREEN_TICKS;
        bool should_yield = (phase_ticks >= EW_GREEN_TICKS) &&
                            ((ns_load > ew_load) || (ns_load > 0 && ew_load == 0) || reached_max);
        if (should_yield) {
            light_state = EW_YELLOW;
            next_green_state = NS_GREEN;
            phase_ticks = 0;
        }
    }
}

void update_light_transition(void) {
    phase_ticks++;
    if (light_state == NS_YELLOW && phase_ticks >= NS_YELLOW_TICKS) {
        if (mode == MANUAL_MODE && next_green_state != ALL_RED) {
            light_state = next_green_state;
        } else {
            light_state = ALL_RED;
        }
        phase_ticks = 0;
    } else if (light_state == EW_YELLOW && phase_ticks >= EW_YELLOW_TICKS) {
        if (mode == MANUAL_MODE && next_green_state != ALL_RED) {
            light_state = next_green_state;
        } else {
            light_state = ALL_RED;
        }
        phase_ticks = 0;
    } else if (light_state == ALL_RED && next_green_state != ALL_RED && phase_ticks >= ALL_RED_TICKS) {
        light_state = next_green_state;
        phase_ticks = 0;
    }
}

void request_light_state(LightState target) {
    if (target == ALL_RED) {
        next_green_state = ALL_RED;
        if (light_state == NS_GREEN) {
            light_state = NS_YELLOW;
            phase_ticks = 0;
        } else if (light_state == EW_GREEN) {
            light_state = EW_YELLOW;
            phase_ticks = 0;
        } else if (light_state == ALL_RED) {
            phase_ticks = 0;
        }
        return;
    }

    next_green_state = target;
    if (light_state == target) {
        return;
    }

    if (light_state == NS_GREEN && target == EW_GREEN) {
        light_state = NS_YELLOW;
        phase_ticks = 0;
    } else if (light_state == EW_GREEN && target == NS_GREEN) {
        light_state = EW_YELLOW;
        phase_ticks = 0;
    } else if (light_state == ALL_RED) {
        light_state = target;
        phase_ticks = 0;
    }
}

// Drawing
void draw_light_vertical(int x, int y, short c_top, short c_mid, short c_bot) {
    draw_box(x, y, x + 11, y + 35, DARKGRAY);
    draw_box(x + 2, y + 2,  x + 9, y + 9,  c_top);
    draw_box(x + 2, y + 13, x + 9, y + 20, c_mid);
    draw_box(x + 2, y + 24, x + 9, y + 31, c_bot);
}

void draw_intersection_base(void) {
    clear_screen(CITY_BG);

    draw_box(0, 0, SCREEN_W - 1, 22, BLACK);
    draw_box(0, SCREEN_H - 18, SCREEN_W - 1, SCREEN_H - 1, BLACK);

    // Building blocks for city-like background.
    draw_box(6, 26, 92, 78, BUILDING);
    draw_box(228, 26, 314, 78, BUILDING);
    draw_box(6, 162, 92, 214, BUILDING);
    draw_box(228, 162, 314, 214, BUILDING);
    draw_box(10, 30, 88, 74, DARKGRAY);
    draw_box(232, 30, 310, 74, DARKGRAY);
    draw_box(10, 166, 88, 210, DARKGRAY);
    draw_box(232, 166, 310, 210, DARKGRAY);

    // horizontal road
    draw_box(0, 90, SCREEN_W - 1, 150, ROAD);

    // vertical road
    draw_box(130, 0, 190, SCREEN_H - 1, ROAD);

    // center region
    draw_box(130, 90, 190, 150, DARKGRAY);
    draw_box(132, 92, 188, 148, 0x3186);

    // Sidewalk around the intersection
    draw_box(0, 84, SCREEN_W - 1, 89, SIDEWALK);
    draw_box(0, 151, SCREEN_W - 1, 156, SIDEWALK);
    draw_box(124, 0, 129, SCREEN_H - 1, SIDEWALK);
    draw_box(191, 0, 196, SCREEN_H - 1, SIDEWALK);

    // Road edge
    draw_box(0, 90, SCREEN_W - 1, 90, ROAD_EDGE);
    draw_box(0, 150, SCREEN_W - 1, 150, ROAD_EDGE);
    draw_box(130, 0, 130, SCREEN_H - 1, ROAD_EDGE);
    draw_box(190, 0, 190, SCREEN_H - 1, ROAD_EDGE);

    // lane markers
    for (int x = 0; x < SCREEN_W; x += 20) {
        draw_box(x, 119, x + 8, 121, WHITE);
    }
    for (int y = 0; y < SCREEN_H; y += 20) {
        draw_box(159, y, 161, y + 8, WHITE);
    }
}

void draw_lights(void) {
    short ns_red    = DARKRED;
    short ns_yellow = DARKYELLOW;
    short ns_green  = DARKGREEN;

    short ew_red    = DARKRED;
    short ew_yellow = DARKYELLOW;
    short ew_green  = DARKGREEN;

    switch (light_state) {
        case NS_GREEN:
            ns_green = GREEN;
            ew_red   = RED;
            break;
        case NS_YELLOW:
            ns_yellow = YELLOW;
            ew_red    = RED;
            break;
        case ALL_RED:
            ns_red = RED;
            ew_red = RED;
            break;
        case EW_GREEN:
            ew_green = GREEN;
            ns_red   = RED;
            break;
        case EW_YELLOW:
            ew_yellow = YELLOW;
            ns_red    = RED;
            break;
    }

    // Signal poles anchored on sidewalks, fully outside the traffic envelope.
    draw_box(204, 64, 206, 90, DARKGRAY);
    draw_box(114, 150, 116, 176, DARKGRAY);
    draw_box(88, 82, 114, 84, DARKGRAY);
    draw_box(206, 156, 236, 158, DARKGRAY);

    // Heads sit on the sidewalk corners, not in the lane.
    draw_light_vertical(198, 28, ns_red, ns_yellow, ns_green);
    draw_light_vertical(102, 160, ns_red, ns_yellow, ns_green);
    draw_light_vertical(72, 46, ew_red, ew_yellow, ew_green);
    draw_light_vertical(240, 160, ew_red, ew_yellow, ew_green);
}

void draw_vehicle_sprite(const Car *car) {
    int x = car->x;
    int y = car->y;
    int w = car_width(car);
    int h = car_height(car);
    short body = car->color;
    short glass = 0xBEFF;
    short roof = WHITE;
    short head = YELLOW;
    short tail = RED;

    draw_box(x + 1, y + 1, x + w, y + h, DARKGRAY);
    draw_box(x, y, x + w - 1, y + h - 1, body);
    draw_box(x, y, x + w - 1, y, BLACK);
    draw_box(x, y + h - 1, x + w - 1, y + h - 1, BLACK);
    draw_box(x, y, x, y + h - 1, BLACK);
    draw_box(x + w - 1, y, x + w - 1, y + h - 1, BLACK);

    if (car->dir == DIR_NORTH) {
        draw_box(x + 2, y + 1, x + w - 3, y + 2, head);
        draw_box(x + 1, y + 3, x + w - 2, y + 5, roof);
        draw_box(x + 1, y + 6, x + w - 2, y + h - 5, glass);
        draw_box(x + 1, y + h - 4, x + w - 2, y + h - 4, BLACK);
        draw_box(x + 1, y + h - 3, x + 2, y + h - 2, tail);
        draw_box(x + w - 3, y + h - 3, x + w - 2, y + h - 2, tail);
    } else if (car->dir == DIR_SOUTH) {
        draw_box(x + 2, y + h - 3, x + w - 3, y + h - 2, head);
        draw_box(x + 1, y + h - 6, x + w - 2, y + h - 4, roof);
        draw_box(x + 1, y + 3, x + w - 2, y + h - 7, glass);
        draw_box(x + 1, y + 3, x + w - 2, y + 3, BLACK);
        draw_box(x + 1, y + 1, x + 2, y + 2, tail);
        draw_box(x + w - 3, y + 1, x + w - 2, y + 2, tail);
    } else if (car->dir == DIR_WEST) {
        draw_box(x + w - 2, y + 2, x + w - 1, y + h - 3, head);
        draw_box(x + w - 5, y + 1, x + w - 3, y + h - 2, roof);
        draw_box(x + 3, y + 1, x + w - 6, y + h - 2, glass);
        draw_box(x + 3, y + 1, x + 3, y + h - 2, BLACK);
        draw_box(x + 1, y + 1, x + 2, y + 2, tail);
        draw_box(x + 1, y + h - 3, x + 2, y + h - 2, tail);
    } else {
        draw_box(x, y + 2, x + 1, y + h - 3, head);
        draw_box(x + 2, y + 1, x + 4, y + h - 2, roof);
        draw_box(x + 5, y + 1, x + w - 4, y + h - 2, glass);
        draw_box(x + w - 4, y + 1, x + w - 4, y + h - 2, BLACK);
        draw_box(x + w - 3, y + 1, x + w - 2, y + 2, tail);
        draw_box(x + w - 3, y + h - 3, x + w - 2, y + h - 2, tail);
    }
}

void draw_cars(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) continue;
        draw_vehicle_sprite(&cars[i]);
    }
}

void draw_hud(void) {
    int time_left = (ROUND_TICKS - elapsed_ticks) / 10;
    int wait_seconds = wait_seconds_total();
    int phase_cd = phase_countdown_tenths();
    if (time_left < 0) time_left = 0;
    draw_text(6, 6, "SCORE", WHITE, 1);
    draw_int(40, 6, score, YELLOW, 1);
    draw_text(92, 6, "PASS", WHITE, 1);
    draw_int(120, 6, passed, GREEN, 1);
    draw_text(154, 6, "WAIT", WHITE, 1);
    draw_int(182, 6, wait_seconds, ORANGE, 1);
    draw_text(214, 6, "TIME", WHITE, 1);
    draw_int(244, 6, time_left, CYAN, 1);
    draw_text(274, 6, "CD", WHITE, 1);
    draw_int(296, 6, phase_cd, MAGENTA, 1);

    draw_text(6, SCREEN_H - 13, "MODE", WHITE, 1);
    draw_text(36, SCREEN_H - 13, (mode == AUTO_MODE) ? "AUTO" : "MANUAL", CYAN, 1);
    draw_text(92, SCREEN_H - 13, "PH", WHITE, 1);
    draw_text(110, SCREEN_H - 13, light_state_label(), YELLOW, 1);
    draw_text(152, SCREEN_H - 13, "BEST", WHITE, 1);
    draw_int(180, SCREEN_H - 13, best_score, MAGENTA, 1);
    draw_text(214, SCREEN_H - 13, "N", WHITE, 1);
    draw_int(222, SCREEN_H - 13, queue_n, CYAN, 1);
    draw_text(238, SCREEN_H - 13, "S", WHITE, 1);
    draw_int(246, SCREEN_H - 13, queue_s, CYAN, 1);
    draw_text(262, SCREEN_H - 13, "W", WHITE, 1);
    draw_int(270, SCREEN_H - 13, queue_w, CYAN, 1);
    draw_text(286, SCREEN_H - 13, "E", WHITE, 1);
    draw_int(294, SCREEN_H - 13, queue_e, CYAN, 1);
}

void redraw_all(void) {
    draw_intersection_base();
    draw_lights();
    draw_cars();
    draw_hud();
    present_frame();
}

void draw_static_scene(SceneRenderer renderer) {
    renderer();
    present_frame();
    renderer();
}

void draw_title_scene(void) {
    clear_screen(0x18C3);

    draw_box(10, 10, 309, 229, BLACK);
    draw_box(14, 14, 305, 225, DARKGRAY);
    draw_box(18, 18, 301, 221, CITY_BG);

    draw_box(18, 18, 301, 24, ROAD_EDGE);
    draw_box(18, 215, 301, 221, ROAD_EDGE);

    draw_box(34, 116, 285, 140, ROAD);
    draw_box(34, 116, 285, 120, SIDEWALK);
    draw_box(34, 136, 285, 140, SIDEWALK);
    for (int x = 52; x <= 250; x += 28) {
        draw_box(x, 126, x + 11, 129, WHITE);
    }

    draw_box(52, 40, 267, 102, BLACK);
    draw_box(56, 44, 263, 98, DARKGRAY);
    draw_box(56, 44, 263, 48, ROAD_EDGE);
    draw_box(56, 94, 263, 98, ROAD_EDGE);
    draw_text_centered(58, "TRAFFIC CONTROL", YELLOW, 2);

    draw_light_vertical(28, 52, RED, DARKYELLOW, DARKGREEN);
    draw_light_vertical(280, 52, DARKRED, YELLOW, DARKGREEN);
    draw_box(33, 88, 35, 116, BLACK);
    draw_box(285, 88, 287, 116, BLACK);

    draw_box(56, 156, 263, 202, BLACK);
    draw_box(60, 160, 259, 198, DARKGRAY);
    draw_box(60, 160, 259, 164, DARKGREEN);
    draw_text_centered(168, "PRESS SPACE", GREEN, 2);
    draw_text_centered(184, "TO START", WHITE, 2);

    draw_box(88, 206, 231, 222, BLACK);
    draw_box(92, 210, 227, 218, DARKGRAY);
    draw_text_centered(211, "I - INFO PAGE", CYAN, 1);
}

void draw_title(void) {
    draw_static_scene(draw_title_scene);
}

void draw_instructions_scene(void) {
    clear_screen(CITY_BG);
    draw_box(18, 14, 301, 225, BLACK);
    draw_box(24, 20, 295, 219, DARKGRAY);

    draw_text_centered(30, "HOW TO PLAY", YELLOW, 2);

    draw_text(34, 62, "GOAL", CYAN, 1);
    draw_text(88, 62, "MOVE CARS WITHOUT A CRASH", WHITE, 1);

    draw_text(34, 84, "SCORE", CYAN, 1);
    draw_text(88, 84, "PASS +25", WHITE, 1);
    draw_text(88, 98, "WAIT SHOWS TOTAL SECONDS", WHITE, 1);
    draw_text(88, 112, "SCORE PENALTY = WAIT SEC / 2", WHITE, 1);
    draw_text(88, 126, "AUTO MODE FOLLOWS QUEUE LOAD", WHITE, 1);

    draw_text(34, 144, "KEYS", CYAN, 1);
    draw_text(88, 144, "SPACE START OR PAUSE", WHITE, 1);
    draw_text(88, 158, "A TOGGLE AUTO MANUAL", WHITE, 1);
    draw_text(88, 172, "1 NS GREEN   2 EW GREEN", WHITE, 1);
    draw_text(88, 186, "3 ALL RED    R RESTART", WHITE, 1);
    draw_text(88, 200, "S TITLE", WHITE, 1);

    draw_text(34, 214, "END", CYAN, 1);
    draw_text(88, 214, "CRASH OUT OR ROUND CLEAR", WHITE, 1);

    draw_text_centered(226, "SPACE PLAY   S BACK", MAGENTA, 1);
}

void draw_instructions(void) {
    draw_static_scene(draw_instructions_scene);
}

void draw_paused_scene(void) {
    draw_intersection_base();
    draw_lights();
    draw_cars();
    draw_hud();
    draw_box(64, 88, 255, 154, BLACK);
    draw_box(68, 92, 251, 150, DARKGRAY);
    draw_text_centered(104, "PAUSED", YELLOW, 2);
    draw_text_centered(126, "SPACE OR P RESUME", WHITE, 1);
    draw_text_centered(140, "S TITLE", WHITE, 1);
}

void draw_paused(void) {
    draw_static_scene(draw_paused_scene);
}

void draw_game_over_scene(void) {
    clear_screen(CITY_BG);
    draw_box(36, 28, 283, 212, BLACK);
    draw_box(42, 34, 277, 206, DARKGRAY);
    if (end_reason == END_CRASH) {
        draw_text_centered(52, "CRASH OUT", RED, 3);
        draw_text_centered(84, "YOU LOST BY COLLISION", ORANGE, 1);
    } else {
        draw_text_centered(52, "ROUND CLEAR", GREEN, 2);
        draw_text_centered(84, "TIME LIMIT REACHED", CYAN, 1);
    }
    draw_box(58, 102, 154, 158, ROAD);
    draw_box(166, 102, 262, 158, ROAD);
    draw_box(60, 104, 152, 156, CITY_BG);
    draw_box(168, 104, 260, 156, CITY_BG);
    draw_text(84, 112, "SCORE", WHITE, 1);
    draw_int(88, 130, score, YELLOW, 2);
    draw_text(198, 112, "PASS", WHITE, 1);
    draw_int(202, 130, passed, GREEN, 2);
    draw_text_centered(166, "PASS +25", WHITE, 1);
    draw_text_centered(178, "WAIT SEC / 2", WHITE, 1);
    draw_text_centered(190, "SPACE RETRY", MAGENTA, 1);
    draw_text_centered(200, "S TITLE", MAGENTA, 1);
}

void draw_game_over(void) {
    draw_static_scene(draw_game_over_scene);
}

// Main controls:
// TITLE: SPACE start, I info
// INSTRUCTIONS: SPACE start, S back
// PLAYING: SPACE/P pause, A auto/manual, 1 force NS, 2 force EW, 3 all red, R restart, S title
// PAUSED: SPACE resume, S title
// GAME OVER: SPACE retry, S title
int main(void) {
    video_init();
    timer_init(TICK_COUNTS); // 0.05 second tick

    reset_round();
    scene = SCENE_TITLE;
    update_hex_timer();

    draw_title();

    while (1) {
        if (timer_expired()) {
            update_hex_timer();

            /* Static scenes are drawn only when the scene changes. Repainting title /
             * pause / game-over every tick causes repeated clear+swap cycles that
             * show up as shimmer on text pixels. */
            if (scene == SCENE_PLAYING) {
                elapsed_ticks++;
                maybe_spawn_car();
                update_cars();
                if (detect_crash()) {
                    end_reason = END_CRASH;
                    scene = SCENE_GAME_OVER;
                    draw_game_over();
                    continue;
                }
                if (light_state == NS_YELLOW || light_state == EW_YELLOW || light_state == ALL_RED) {
                    update_light_transition();
                } else if (mode == AUTO_MODE) {
                    update_lights_auto();
                }
                if (elapsed_ticks >= ROUND_TICKS) {
                    end_reason = END_TIME;
                    scene = SCENE_GAME_OVER;
                    draw_game_over();
                } else {
                    redraw_all();
                }
            }
        }

      int key = ps2_get_make_code();
        
        if (scene == SCENE_TITLE) {
            rng_state += *(timer_ptr + 4); // Change the seed while waiting
        }

        if (key != -1) {
            if (scene == SCENE_TITLE) {
                if (key == 0x29) {      // SPACE
                    scene = SCENE_PLAYING;
                    reset_round();
                    redraw_all();
                } else if (key == 0x43) { // I
                    scene = SCENE_INSTRUCTIONS;
                    draw_instructions();
                }
            } else if (scene == SCENE_INSTRUCTIONS) {
                if (key == 0x29) {      // SPACE
                    scene = SCENE_PLAYING;
                    reset_round();
                    redraw_all();
                } else if (key == 0x1B) { // S
                    scene = SCENE_TITLE;
                    draw_title();
                }
            } else if (scene == SCENE_PLAYING) {
                if (key == 0x29 || key == 0x4D) { // SPACE / P
                    scene = SCENE_PAUSED;
                    draw_paused();
                } else if (key == 0x16) {   // '1'
                    mode = MANUAL_MODE;
                    request_light_state(NS_GREEN);
                    redraw_all();
                } else if (key == 0x1E) {   // '2'
                    mode = MANUAL_MODE;
                    request_light_state(EW_GREEN);
                    redraw_all();
                } else if (key == 0x26) {   // '3'
                    mode = MANUAL_MODE;
                    request_light_state(ALL_RED);
                    redraw_all();
                } else if (key == 0x1C) {   // 'A'
                    mode = (mode == AUTO_MODE) ? MANUAL_MODE : AUTO_MODE;
                    redraw_all();
                } else if (key == 0x2D) {   // 'R'
                    reset_round();
                    redraw_all();
                } else if (key == 0x1B) {   // S -> back to title
                    scene = SCENE_TITLE;
                    draw_title();
                }
            } else if (scene == SCENE_PAUSED) {
                if (key == 0x29 || key == 0x4D) { // SPACE / P resume
                    scene = SCENE_PLAYING;
                    redraw_all();
                } else if (key == 0x1B) {   // S -> title
                    scene = SCENE_TITLE;
                    draw_title();
                }
            } else if (scene == SCENE_GAME_OVER) {
                if (key == 0x29) {          // SPACE retry
                    reset_round();
                    scene = SCENE_PLAYING;
                    redraw_all();
                } else if (key == 0x1B) {   // S -> title
                    scene = SCENE_TITLE;
                    draw_title();
                }
            }
        }
    }
}

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
#define MENU_BG    0x18C3
#define SIDEWALK   0xBDF7
#define ROAD_EDGE  0x6B4D
#define BUILDING   0x52AA

#define MAX_CARS 32
#define MAX_PEDS 8
#define CAR_LONG 12
#define CAR_SHORT 8
#define PED_SIZE 7
#define PED_WALK_TICKS 56
#define PED_REQUEST_CHANCE_PERCENT 3
#define MAX_PED_WAITING 9
#define MAX_PEDS_PER_PHASE 6
#define MANUAL_PED_REQUEST_COUNT 2
#define PED_TOP_Y 81
#define PED_BOTTOM_Y 154
#define PED_LEFT_X 119
#define PED_RIGHT_X 196
#define PED_H_START_L 116
#define PED_H_START_R 198
#define PED_V_START_T 92
#define PED_V_START_B 144
#define STOP_N 68
#define STOP_S 162
#define STOP_W 106
#define STOP_E 204
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
    EW_YELLOW,
    PED_HORIZONTAL_WALK,
    PED_VERTICAL_WALK,
    PED_ALL_WALK
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

typedef enum {
    RUSH_NS = 0,
    RUSH_EW
} RushAxis;

typedef struct {
    bool active;
    Direction dir;
    int x;
    int y;
    bool scored;
    short color;
} Car;

typedef struct {
    bool active;
    bool horizontal;
    int x;
    int y;
    int dx;
    int dy;
    short color;
} Pedestrian;

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
static Pedestrian peds[MAX_PEDS];
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
static LightState resume_green_state = NS_GREEN;
static int ped_waiting_horizontal = 0;
static int ped_waiting_vertical = 0;
static int ped_wait_ticks_horizontal = 0;
static int ped_wait_ticks_vertical = 0;
static int ped_groups_served = 0;
static bool ped_priority_horizontal = true;
static RushAxis rush_axis = RUSH_NS;
static int rush_ticks_left = 0;
static int bonus_score = 0;
static int flow_streak = 0;

void clear_screen(short color);
void update_hex_timer(void);
void draw_page_frame(short fill);
void draw_panel(int x1, int y1, int x2, int y2, short fill, short accent);
void draw_compact_button(int x1, int y1, int x2, int y2, short accent);
void draw_label_strip(int x1, int y1, int x2, int y2, short fill);
void draw_static_scene(SceneRenderer renderer);
void draw_title_scene(void);
void draw_instructions_scene(void);
void draw_paused_scene(void);
void draw_game_over_scene(void);

bool is_ped_walk_state(LightState state);
bool is_vehicle_green_state(LightState state);
bool any_ped_request_pending(void);
LightState choose_pending_ped_state(void);
LightState choose_resume_green_state(void);
void clear_pedestrians(void);
void maybe_queue_ped_request(void);
void start_ped_phase(LightState walk_state);
void update_pedestrians(void);
void draw_pedestrians(void);
void draw_waiting_pedestrians(void);
void draw_crosswalk_guides(void);
bool ped_area_clear(void);
const char *ped_status_horizontal_label(void);
const char *ped_status_vertical_label(void);
short ped_status_horizontal_color(void);
short ped_status_vertical_color(void);
bool ped_flow_allowed_horizontal(void);
bool ped_flow_allowed_vertical(void);
int count_active_pedestrians(bool horizontal);
void release_waiting_pedestrians(bool horizontal, int count);
void maybe_spawn_pedestrians_for_current_state(void);

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

const char *rush_axis_label(void) {
    return (rush_axis == RUSH_NS) ? "NS" : "EW";
}

const char *rush_notice_label(void) {
    return (rush_axis == RUSH_NS) ? "PRIORITY NORTH SOUTH" : "PRIORITY EAST WEST";
}

short rush_axis_color(void) {
    return (rush_axis == RUSH_NS) ? GREEN : ORANGE;
}

bool is_rush_dir(Direction dir) {
    if (rush_axis == RUSH_NS) {
        return dir == DIR_NORTH || dir == DIR_SOUTH;
    }
    return dir == DIR_WEST || dir == DIR_EAST;
}

void choose_new_rush_axis(void) {
    rush_axis = (next_rand() & 1u) ? RUSH_NS : RUSH_EW;
    rush_ticks_left = 180 + (int)(next_rand() % 120u);
}

void update_rush_cycle(void) {
    if (rush_ticks_left > 0) {
        rush_ticks_left--;
    }
    if (rush_ticks_left <= 0) {
        choose_new_rush_axis();
    }
}


bool is_ped_walk_state(LightState state) {
    return state == PED_HORIZONTAL_WALK || state == PED_VERTICAL_WALK || state == PED_ALL_WALK;
}

bool is_vehicle_green_state(LightState state) {
    return state == NS_GREEN || state == EW_GREEN;
}

bool any_ped_request_pending(void) {
    return ped_waiting_horizontal > 0 || ped_waiting_vertical > 0;
}

LightState choose_pending_ped_state(void) {
    if (ped_waiting_horizontal > 0 && ped_waiting_vertical > 0) {
        return PED_ALL_WALK;
    }
    return ALL_RED;
}

LightState choose_resume_green_state(void) {
    int ns_load = queue_n + queue_s;
    int ew_load = queue_w + queue_e;

    if (ns_load > ew_load) return NS_GREEN;
    if (ew_load > ns_load) return EW_GREEN;
    if (rush_axis == RUSH_NS) return NS_GREEN;
    return EW_GREEN;
}

void clear_pedestrians(void) {
    for (int i = 0; i < MAX_PEDS; i++) {
        peds[i].active = false;
    }
}

bool ped_flow_allowed_horizontal(void) {
    return light_state == EW_GREEN || light_state == PED_ALL_WALK;
}

bool ped_flow_allowed_vertical(void) {
    return light_state == NS_GREEN || light_state == PED_ALL_WALK;
}

int count_active_pedestrians(bool horizontal) {
    int count = 0;
    for (int i = 0; i < MAX_PEDS; i++) {
        if (!peds[i].active) continue;
        if (peds[i].horizontal == horizontal) count++;
    }
    return count;
}

void spawn_pedestrian(bool horizontal, int x, int y, int dx, int dy, short color) {
    for (int i = 0; i < MAX_PEDS; i++) {
        if (!peds[i].active) {
            peds[i].active = true;
            peds[i].horizontal = horizontal;
            peds[i].x = x;
            peds[i].y = y;
            peds[i].dx = dx;
            peds[i].dy = dy;
            peds[i].color = color;
            return;
        }
    }
}

void add_ped_waiters(bool horizontal, int count) {
    for (int i = 0; i < count; i++) {
        if (horizontal) {
            if (ped_waiting_horizontal < MAX_PED_WAITING) ped_waiting_horizontal++;
        } else {
            if (ped_waiting_vertical < MAX_PED_WAITING) ped_waiting_vertical++;
        }
    }
}

void maybe_queue_ped_request(void) {
    if (light_state == PED_ALL_WALK) {
        return;
    }

    if ((int)(next_rand() % 100u) >= PED_REQUEST_CHANCE_PERCENT) {
        return;
    }

    if ((next_rand() & 1u) == 0u) {
        add_ped_waiters(true, 1);
    } else {
        add_ped_waiters(false, 1);
    }
}

void release_waiting_pedestrians(bool horizontal, int count) {
    if (count <= 0) return;

    int active_total = count_active_pedestrians(true) + count_active_pedestrians(false);
    if (count > MAX_PEDS - active_total) {
        count = MAX_PEDS - active_total;
    }
    if (count <= 0) return;

    for (int i = 0; i < count; i++) {
        int offset = (i / 2) * 10;
        short color = ((ped_groups_served + i) & 1) == 0 ? CYAN : MAGENTA;

        if (horizontal) {
            if (ped_waiting_horizontal <= 0) break;
            ped_waiting_horizontal--;
            if ((i & 1) == 0) {
                spawn_pedestrian(true, PED_H_START_L - 4 - offset, PED_TOP_Y + 1, 2, 0, color);
            } else {
                spawn_pedestrian(true, PED_H_START_R + 4 + offset, PED_BOTTOM_Y + 1, -2, 0, color);
            }
        } else {
            if (ped_waiting_vertical <= 0) break;
            ped_waiting_vertical--;
            if ((i & 1) == 0) {
                spawn_pedestrian(false, PED_LEFT_X + 1, PED_V_START_T - 4 - offset, 0, 2, color);
            } else {
                spawn_pedestrian(false, PED_RIGHT_X + 1, PED_V_START_B + 4 + offset, 0, -2, color);
            }
        }
        ped_groups_served++;
    }
}

void start_ped_phase(LightState walk_state) {
    phase_ticks = 0;
    light_state = walk_state;

    if (walk_state == PED_HORIZONTAL_WALK) {
        ped_wait_ticks_horizontal = 0;
        release_waiting_pedestrians(true, MAX_PEDS_PER_PHASE);
    } else if (walk_state == PED_VERTICAL_WALK) {
        ped_wait_ticks_vertical = 0;
        release_waiting_pedestrians(false, MAX_PEDS_PER_PHASE);
    } else if (walk_state == PED_ALL_WALK) {
        ped_wait_ticks_horizontal = 0;
        ped_wait_ticks_vertical = 0;
        release_waiting_pedestrians(true, MAX_PEDS);
        release_waiting_pedestrians(false, MAX_PEDS);
    }
}

void maybe_spawn_pedestrians_for_current_state(void) {
    bool all_walk = (light_state == PED_ALL_WALK);
    bool exclusive_walk = is_ped_walk_state(light_state);
    int spawn_tick = all_walk ? 4 : (exclusive_walk ? 6 : 8);

    if (ped_flow_allowed_horizontal() && ped_waiting_horizontal > 0) {
        int active_h = count_active_pedestrians(true);
        if (active_h == 0 || (phase_ticks % spawn_tick) == 0) {
            int count = ped_waiting_horizontal;
            if (!all_walk && count > 2) count = 2;
            release_waiting_pedestrians(true, count);
        }
    }

    if (ped_flow_allowed_vertical() && ped_waiting_vertical > 0) {
        int active_v = count_active_pedestrians(false);
        if (active_v == 0 || (phase_ticks % spawn_tick) == 0) {
            int count = ped_waiting_vertical;
            if (!all_walk && count > 2) count = 2;
            release_waiting_pedestrians(false, count);
        }
    }
}

void update_pedestrians(void) {
    bool exclusive_walk = is_ped_walk_state(light_state);

    if (exclusive_walk) {
        phase_ticks++;
        if (light_state == PED_ALL_WALK || phase_ticks < PED_WALK_TICKS) {
            maybe_spawn_pedestrians_for_current_state();
        }
    } else if (ped_flow_allowed_horizontal() || ped_flow_allowed_vertical()) {
        maybe_spawn_pedestrians_for_current_state();
    }

    bool any_active = false;

    for (int i = 0; i < MAX_PEDS; i++) {
        if (!peds[i].active) continue;

        peds[i].x += peds[i].dx;
        peds[i].y += peds[i].dy;

        if (peds[i].horizontal) {
            if (peds[i].x < PED_H_START_L - 14 || peds[i].x > PED_H_START_R + 14) {
                peds[i].active = false;
                continue;
            }
        } else {
            if (peds[i].y < PED_V_START_T - 14 || peds[i].y > PED_V_START_B + 14) {
                peds[i].active = false;
                continue;
            }
        }
        any_active = true;
    }

    if (exclusive_walk) {
        bool queue_cleared = false;

        if (light_state == PED_ALL_WALK) {
            queue_cleared = (ped_waiting_horizontal == 0 && ped_waiting_vertical == 0);
        } else if (light_state == PED_HORIZONTAL_WALK) {
            queue_cleared = (ped_waiting_horizontal == 0);
        } else {
            queue_cleared = (ped_waiting_vertical == 0);
        }

        if (phase_ticks >= PED_WALK_TICKS && queue_cleared && !any_active) {
            light_state = ALL_RED;
            phase_ticks = 0;
            next_green_state = (mode == AUTO_MODE) ? choose_resume_green_state() : ALL_RED;
        }
    }
}

const char *ped_status_horizontal_label(void) {
    if (ped_flow_allowed_horizontal() && count_active_pedestrians(true) > 0) return "GO";
    if (ped_waiting_horizontal > 0) return "WAIT";
    return "CLEAR";
}

const char *ped_status_vertical_label(void) {
    if (ped_flow_allowed_vertical() && count_active_pedestrians(false) > 0) return "GO";
    if (ped_waiting_vertical > 0) return "WAIT";
    return "CLEAR";
}

short ped_status_horizontal_color(void) {
    if (ped_flow_allowed_horizontal() && count_active_pedestrians(true) > 0) return GREEN;
    if (ped_waiting_horizontal > 0) return ORANGE;
    return CYAN;
}

short ped_status_vertical_color(void) {
    if (ped_flow_allowed_vertical() && count_active_pedestrians(false) > 0) return GREEN;
    if (ped_waiting_vertical > 0) return ORANGE;
    return CYAN;
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
    resume_green_state = NS_GREEN;
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
    ped_waiting_horizontal = 0;
    ped_waiting_vertical = 0;
    ped_wait_ticks_horizontal = 0;
    ped_wait_ticks_vertical = 0;
    ped_groups_served = 0;
    ped_priority_horizontal = true;
    bonus_score = 0;
    flow_streak = 0;
    choose_new_rush_axis();
    clear_pedestrians();
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

    Direction dir;
    int roll = (int)(next_rand() % 100u);
    if (rush_axis == RUSH_NS) {
        if (roll < 38) dir = DIR_NORTH;
        else if (roll < 76) dir = DIR_SOUTH;
        else if (roll < 88) dir = DIR_WEST;
        else dir = DIR_EAST;
    } else {
        if (roll < 38) dir = DIR_WEST;
        else if (roll < 76) dir = DIR_EAST;
        else if (roll < 88) dir = DIR_NORTH;
        else dir = DIR_SOUTH;
    }
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
        case NS_GREEN:            return "N/S GO";
        case NS_YELLOW:           return "CHANGE";
        case ALL_RED:             return "STOP";
        case EW_GREEN:            return "E/W GO";
        case EW_YELLOW:           return "CHANGE";
        case PED_HORIZONTAL_WALK: return "SIDE WALK";
        case PED_VERTICAL_WALK:   return "UP DOWN";
        case PED_ALL_WALK:        return "ALL STOP";
        default:                  return "STOP";
    }
}

const char *light_state_long_label(void) {
    switch (light_state) {
        case NS_GREEN:            return "N/S GO";
        case NS_YELLOW:           return "CHANGE";
        case ALL_RED:             return "STOP";
        case EW_GREEN:            return "E/W GO";
        case EW_YELLOW:           return "CHANGE";
        case PED_HORIZONTAL_WALK: return "SIDE WALK";
        case PED_VERTICAL_WALK:   return "UP DOWN WALK";
        case PED_ALL_WALK:        return "ALL STOP";
        default:                  return "STOP";
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
        case PED_HORIZONTAL_WALK:
        case PED_VERTICAL_WALK:
        case PED_ALL_WALK:
            remaining = PED_WALK_TICKS - phase_ticks;
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

        if (cars[i].x < -24 || cars[i].x > SCREEN_W + 24 || cars[i].y < -24 || cars[i].y > SCREEN_H + 24) {
            if (!cars[i].scored && passed_stop_line(&cars[i])) {
                cars[i].scored = true;
                passed++;
            }
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
        bool scramble_due = (ped_waiting_horizontal > 0 && ped_waiting_vertical > 0 && phase_ticks >= NS_GREEN_TICKS);
        bool should_yield = scramble_due ||
                            ((phase_ticks >= NS_GREEN_TICKS) &&
                             ((ew_load > ns_load) || (ew_load > 0 && ns_load == 0) || reached_max));
        if (should_yield) {
            light_state = NS_YELLOW;
            next_green_state = scramble_due ? PED_ALL_WALK : EW_GREEN;
            phase_ticks = 0;
        }
    } else if (light_state == EW_GREEN) {
        int ns_load = queue_n + queue_s;
        int ew_load = queue_w + queue_e;
        bool reached_max = phase_ticks >= MAX_GREEN_TICKS;
        bool scramble_due = (ped_waiting_horizontal > 0 && ped_waiting_vertical > 0 && phase_ticks >= EW_GREEN_TICKS);
        bool should_yield = scramble_due ||
                            ((phase_ticks >= EW_GREEN_TICKS) &&
                             ((ns_load > ew_load) || (ns_load > 0 && ew_load == 0) || reached_max));
        if (should_yield) {
            light_state = EW_YELLOW;
            next_green_state = scramble_due ? PED_ALL_WALK : NS_GREEN;
            phase_ticks = 0;
        }
    }
}

void update_light_transition(void) {
    phase_ticks++;
    if (light_state == NS_YELLOW && phase_ticks >= NS_YELLOW_TICKS) {
        if (mode == MANUAL_MODE && is_vehicle_green_state(next_green_state)) {
            light_state = next_green_state;
        } else {
            light_state = ALL_RED;
        }
        phase_ticks = 0;
    } else if (light_state == EW_YELLOW && phase_ticks >= EW_YELLOW_TICKS) {
        if (mode == MANUAL_MODE && is_vehicle_green_state(next_green_state)) {
            light_state = next_green_state;
        } else {
            light_state = ALL_RED;
        }
        phase_ticks = 0;
    } else if (light_state == ALL_RED && next_green_state != ALL_RED && phase_ticks >= ALL_RED_TICKS) {
        if (is_ped_walk_state(next_green_state)) {
            if (ped_area_clear()) {
                start_ped_phase(next_green_state);
            }
        } else {
            light_state = next_green_state;
            phase_ticks = 0;
        }
    }
}

void request_light_state(LightState target) {
    if (target == PED_HORIZONTAL_WALK) {
        add_ped_waiters(true, MANUAL_PED_REQUEST_COUNT);
        return;
    }

    if (target == PED_VERTICAL_WALK) {
        add_ped_waiters(false, MANUAL_PED_REQUEST_COUNT);
        return;
    }

    if (target == PED_ALL_WALK) {
        resume_green_state = ALL_RED;
        next_green_state = PED_ALL_WALK;

        if (light_state == NS_GREEN) {
            light_state = NS_YELLOW;
            phase_ticks = 0;
        } else if (light_state == EW_GREEN) {
            light_state = EW_YELLOW;
            phase_ticks = 0;
        } else if (light_state == ALL_RED && ped_area_clear()) {
            start_ped_phase(PED_ALL_WALK);
        }
        return;
    }

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
    bool top_on = ((unsigned short)c_top == (unsigned short)RED);
    bool mid_on = ((unsigned short)c_mid == (unsigned short)YELLOW);
    bool bot_on = ((unsigned short)c_bot == (unsigned short)GREEN);

    draw_box(x, y, x + 13, y + 41, BLACK);
    draw_box(x + 1, y + 1, x + 12, y + 40, DARKGRAY);
    draw_box(x + 2, y + 2, x + 11, y + 39, 0x2945);

    if (top_on) {
        draw_box(x + 2, y + 4, x + 11, y + 13, DARKRED);
    }
    draw_box(x + 3, y + 5,  x + 10, y + 12, c_top);

    if (mid_on) {
        draw_box(x + 2, y + 16, x + 11, y + 25, DARKYELLOW);
    }
    draw_box(x + 3, y + 17, x + 10, y + 24, c_mid);

    if (bot_on) {
        draw_box(x + 2, y + 28, x + 11, y + 37, DARKGREEN);
    }
    draw_box(x + 3, y + 29, x + 10, y + 36, c_bot);
}

void draw_light_horizontal(int x, int y, short c_left, short c_mid, short c_right) {
    bool left_on = ((unsigned short)c_left == (unsigned short)RED);
    bool mid_on = ((unsigned short)c_mid == (unsigned short)YELLOW);
    bool right_on = ((unsigned short)c_right == (unsigned short)GREEN);

    draw_box(x, y, x + 41, y + 13, BLACK);
    draw_box(x + 1, y + 1, x + 40, y + 12, DARKGRAY);
    draw_box(x + 2, y + 2, x + 39, y + 11, 0x2945);

    if (left_on) {
        draw_box(x + 4, y + 2, x + 13, y + 11, DARKRED);
    }
    draw_box(x + 5, y + 3, x + 12, y + 10, c_left);

    if (mid_on) {
        draw_box(x + 16, y + 2, x + 25, y + 11, DARKYELLOW);
    }
    draw_box(x + 17, y + 3, x + 24, y + 10, c_mid);

    if (right_on) {
        draw_box(x + 28, y + 2, x + 37, y + 11, DARKGREEN);
    }
    draw_box(x + 29, y + 3, x + 36, y + 10, c_right);
}

void draw_intersection_base(void) {
    clear_screen(CITY_BG);

    draw_box(0, 0, SCREEN_W - 1, 35, BLACK);
    draw_box(0, SCREEN_H - 34, SCREEN_W - 1, SCREEN_H - 1, BLACK);

    // Building blocks for city-like background.
    draw_box(6, 42, 92, 78, BUILDING);
    draw_box(228, 42, 314, 78, BUILDING);
    draw_box(6, 158, 92, 198, BUILDING);
    draw_box(228, 158, 314, 198, BUILDING);
    draw_box(10, 46, 88, 74, DARKGRAY);
    draw_box(232, 46, 310, 74, DARKGRAY);
    draw_box(10, 162, 88, 194, DARKGRAY);
    draw_box(232, 162, 310, 194, DARKGRAY);
    for (int x = 18; x <= 72; x += 18) {
        draw_box(x, 42, x + 8, 50, SIDEWALK);
        draw_box(x, 56, x + 8, 64, SIDEWALK);
        draw_box(x, 174, x + 8, 182, SIDEWALK);
    }
    for (int x = 240; x <= 294; x += 18) {
        draw_box(x, 42, x + 8, 50, SIDEWALK);
        draw_box(x, 56, x + 8, 64, SIDEWALK);
        draw_box(x, 174, x + 8, 182, SIDEWALK);
    }

    // horizontal road
    draw_box(0, 88, SCREEN_W - 1, 152, ROAD);

    // vertical road
    draw_box(126, 36, 194, 205, ROAD);

    // center region
    draw_box(126, 88, 194, 152, DARKGRAY);
    draw_box(130, 92, 190, 148, 0x3186);

    // Sidewalk around the intersection
    draw_box(0, 80, SCREEN_W - 1, 87, SIDEWALK);
    draw_box(0, 153, SCREEN_W - 1, 160, SIDEWALK);
    draw_box(118, 36, 125, 205, SIDEWALK);
    draw_box(195, 36, 202, 205, SIDEWALK);

    // Road edge
    draw_box(0, 88, SCREEN_W - 1, 88, ROAD_EDGE);
    draw_box(0, 152, SCREEN_W - 1, 152, ROAD_EDGE);
    draw_box(126, 36, 126, 205, ROAD_EDGE);
    draw_box(194, 36, 194, 205, ROAD_EDGE);

    // lane markers
    for (int x = 0; x < SCREEN_W; x += 20) {
        draw_box(x, 119, x + 8, 121, WHITE);
    }
    for (int y = 40; y < 198; y += 20) {
        draw_box(159, y, 161, y + 8, WHITE);
    }

    // Crosswalks give the junction a cleaner city look.
    for (int x = 134; x <= 184; x += 8) {
        draw_box(x, 80, x + 4, 87, WHITE);
        draw_box(x, 153, x + 4, 160, WHITE);
    }
    for (int y = 94; y <= 146; y += 8) {
        draw_box(118, y, 125, y + 4, WHITE);
        draw_box(195, y, 202, y + 4, WHITE);
    }

    // Stop bars help the player read the traffic flow at a glance.
    draw_box(136, 78, 184, 79, WHITE);
    draw_box(136, 161, 184, 162, WHITE);
    draw_box(116, 96, 117, 144, WHITE);
    draw_box(203, 96, 204, 144, WHITE);
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
        case PED_HORIZONTAL_WALK:
        case PED_VERTICAL_WALK:
        case PED_ALL_WALK:
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

    short ns_bar = DARKRED;
    short ew_bar = DARKRED;
    if (light_state == NS_GREEN) ns_bar = GREEN;
    else if (light_state == NS_YELLOW) ns_bar = YELLOW;
    if (light_state == EW_GREEN) ew_bar = GREEN;
    else if (light_state == EW_YELLOW) ew_bar = YELLOW;

    draw_box(136, 78, 184, 80, ns_bar);
    draw_box(136, 160, 184, 162, ns_bar);
    draw_box(116, 96, 118, 144, ew_bar);
    draw_box(202, 96, 204, 144, ew_bar);

    draw_light_vertical(204, 46, ns_red, ns_yellow, ns_green);
    draw_light_vertical(92, 144, ns_red, ns_yellow, ns_green);
    draw_light_horizontal(44, 70, ew_red, ew_yellow, ew_green);
    draw_light_horizontal(222, 150, ew_red, ew_yellow, ew_green);
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


bool ped_area_clear(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) continue;

        int x1 = cars[i].x;
        int y1 = cars[i].y;
        int x2 = cars[i].x + car_width(&cars[i]) - 1;
        int y2 = cars[i].y + car_height(&cars[i]) - 1;

        if (rect_overlap(x1, y1, x2, y2, 116, 79, 204, 161)) {
            return false;
        }
    }
    return true;
}


void draw_pedestrian_sprite(const Pedestrian *ped) {
    int x = ped->x;
    int y = ped->y;
    short body = ped->color;

    draw_box(x + 2, y, x + 4, y + 2, WHITE);
    draw_box(x + 2, y + 1, x + 4, y + 2, body);
    draw_box(x + 3, y + 3, x + 3, y + 7, body);
    draw_box(x + 1, y + 4, x + 2, y + 4, body);
    draw_box(x + 4, y + 4, x + 5, y + 4, body);
    draw_box(x + 2, y + 8, x + 2, y + 9, body);
    draw_box(x + 4, y + 8, x + 4, y + 9, body);
}


void draw_crosswalk_guides(void) {
    short top_bot_color = 0;
    short left_right_color = 0;

    if ((ped_flow_allowed_horizontal() && (count_active_pedestrians(true) > 0 || ped_waiting_horizontal > 0)) ||
        light_state == PED_ALL_WALK) {
        top_bot_color = GREEN;
    } else if (ped_waiting_horizontal > 0) {
        top_bot_color = YELLOW;
    }

    if ((ped_flow_allowed_vertical() && (count_active_pedestrians(false) > 0 || ped_waiting_vertical > 0)) ||
        light_state == PED_ALL_WALK) {
        left_right_color = GREEN;
    } else if (ped_waiting_vertical > 0) {
        left_right_color = YELLOW;
    }

    if (top_bot_color != 0) {
        draw_box(134, 79, 184, 80, top_bot_color);
        draw_box(134, 87, 184, 88, top_bot_color);
        draw_box(134, 152, 184, 153, top_bot_color);
        draw_box(134, 160, 184, 161, top_bot_color);
    }

    if (left_right_color != 0) {
        draw_box(117, 94, 118, 144, left_right_color);
        draw_box(125, 94, 126, 144, left_right_color);
        draw_box(194, 94, 195, 144, left_right_color);
        draw_box(202, 94, 203, 144, left_right_color);
    }
}

void draw_pedestrians(void) {
    for (int i = 0; i < MAX_PEDS; i++) {
        if (!peds[i].active) continue;
        draw_pedestrian_sprite(&peds[i]);
    }
}

void draw_waiting_pedestrians(void) {
}

void draw_cars(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        if (!cars[i].active) continue;
        draw_vehicle_sprite(&cars[i]);
    }
}

void draw_hud(void) {
    draw_label_strip(4, 3, 102, 19, DARKGRAY);
    draw_text(10, 7, "SCORE", WHITE, 1);
    draw_int_right(96, 7, score, YELLOW, 1);

    draw_label_strip(108, 3, 206, 19, DARKGRAY);
    draw_text(114, 7, "PASS", WHITE, 1);
    draw_int_right(200, 7, passed, GREEN, 1);

    draw_label_strip(212, 3, 316, 19, DARKGRAY);
    draw_text(218, 7, "BEST", WHITE, 1);
    draw_int_right(310, 7, best_score, MAGENTA, 1);

    draw_label_strip(4, 22, 316, 36, DARKGRAY);
    draw_text(10, 26, "CARS", WHITE, 1);
    draw_text(46, 26, "N", WHITE, 1);
    draw_int(54, 26, queue_n, CYAN, 1);
    draw_text(80, 26, "S", WHITE, 1);
    draw_int(88, 26, queue_s, CYAN, 1);
    draw_text(114, 26, "W", WHITE, 1);
    draw_int(122, 26, queue_w, ORANGE, 1);
    draw_text(148, 26, "E", WHITE, 1);
    draw_int(156, 26, queue_e, ORANGE, 1);

    draw_panel(4, 205, 68, 237, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(8, 64, 213, "MODE", WHITE, 1);
    draw_text_in_box(8, 64, 225, (mode == AUTO_MODE) ? "AUTO" : "MANUAL", CYAN, 1);

    draw_panel(74, 205, 170, 237, DARKGRAY, YELLOW);
    draw_text_in_box(78, 166, 213, "LIGHT", WHITE, 1);
    draw_text_in_box(78, 166, 225, light_state_label(), YELLOW, 1);

    draw_panel(176, 194, 316, 237, DARKGRAY, CYAN);
    draw_text(184, 202, "PEOPLE", WHITE, 1);
    draw_text(184, 214, "UP/DN", ped_status_vertical_color(), 1);
    draw_int_right(306, 214, ped_waiting_vertical, ped_status_vertical_color(), 1);
    draw_text(184, 224, "SIDE", ped_status_horizontal_color(), 1);
    draw_int_right(306, 224, ped_waiting_horizontal, ped_status_horizontal_color(), 1);
}

void redraw_all(void) {
    draw_intersection_base();
    draw_lights();
    draw_crosswalk_guides();
    draw_waiting_pedestrians();
    draw_cars();
    draw_pedestrians();
    draw_hud();
    present_frame();
}

void draw_page_frame(short fill) {
    clear_screen(MENU_BG);
    draw_box(10, 10, 309, 229, BLACK);
    draw_box(14, 14, 305, 225, DARKGRAY);
    draw_box(18, 18, 301, 221, fill);
    draw_box(18, 18, 301, 24, ROAD_EDGE);
    draw_box(18, 215, 301, 221, ROAD_EDGE);
}

void draw_panel(int x1, int y1, int x2, int y2, short fill, short accent) {
    draw_box(x1, y1, x2, y2, BLACK);
    draw_box(x1 + 4, y1 + 4, x2 - 4, y2 - 4, fill);
    draw_box(x1 + 4, y1 + 4, x2 - 4, y1 + 6, accent);
}

void draw_compact_button(int x1, int y1, int x2, int y2, short accent) {
    draw_box(x1, y1, x2, y2, BLACK);
    draw_box(x1 + 3, y1 + 3, x2 - 3, y2 - 3, DARKGRAY);
    draw_box(x1 + 3, y1 + 3, x2 - 3, y1 + 5, accent);
}

void draw_label_strip(int x1, int y1, int x2, int y2, short fill) {
    draw_box(x1, y1, x2, y2, BLACK);
    draw_box(x1 + 3, y1 + 3, x2 - 3, y2 - 3, fill);
}

void draw_static_scene(SceneRenderer renderer) {
    renderer();
    present_frame();
    renderer();
}

void draw_title_scene(void) {
    draw_page_frame(CITY_BG);

    draw_panel(40, 28, 279, 92, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(44, 275, 40, "TRAFFIC CONTROL", YELLOW, 2);
    draw_text_in_box(44, 275, 64, "CARS AND CROSSWALKS", CYAN, 1);
    draw_text_in_box(44, 275, 76, "BY ALAN HE AND HARRY ZHANG", WHITE, 1);

    draw_box(34, 102, 286, 150, ROAD);
    draw_box(118, 88, 202, 164, ROAD);
    draw_box(34, 102, 286, 106, SIDEWALK);
    draw_box(34, 146, 286, 150, SIDEWALK);
    draw_box(114, 88, 118, 164, SIDEWALK);
    draw_box(202, 88, 206, 164, SIDEWALK);
    for (int x = 56; x <= 250; x += 26) {
        draw_box(x, 124, x + 10, 127, WHITE);
    }
    for (int x = 128; x <= 188; x += 10) {
        draw_box(x, 102, x + 4, 106, WHITE);
        draw_box(x, 146, x + 4, 150, WHITE);
    }
    for (int y = 100; y <= 150; y += 10) {
        draw_box(114, y, 118, y + 4, WHITE);
        draw_box(202, y, 206, y + 4, WHITE);
    }
    draw_light_vertical(24, 56, RED, DARKYELLOW, DARKGREEN);
    draw_light_vertical(282, 56, RED, DARKYELLOW, DARKGREEN);
    { Pedestrian demo1 = {true, true, 136, 112, 0, 0, CYAN}; draw_pedestrian_sprite(&demo1); }
    { Pedestrian demo2 = {true, true, 182, 128, 0, 0, MAGENTA}; draw_pedestrian_sprite(&demo2); }

    draw_panel(50, 166, 270, 198, DARKGRAY, DARKGREEN);
    draw_text_in_box(54, 266, 176, "PRESS SPACE TO START", GREEN, 1);

    draw_label_strip(82, 206, 238, 220, DARKGRAY);
    draw_text_in_box(86, 234, 210, "I HELP", WHITE, 1);
}

void draw_title(void) {
    draw_static_scene(draw_title_scene);
}

void draw_instructions_scene(void) {
    draw_page_frame(CITY_BG);

    draw_panel(46, 28, 273, 74, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(50, 269, 40, "HOW TO PLAY", YELLOW, 2);

    draw_panel(28, 84, 291, 116, DARKGRAY, CYAN);
    draw_text(40, 94, "GOAL", CYAN, 1);
    draw_text(82, 94, "MOVE CARS", WHITE, 1);
    draw_text(148, 94, "AVOID CRASHES", WHITE, 1);
    draw_text(82, 104, "GREEN MOVES SAME WAY WALKERS", WHITE, 1);

    draw_panel(28, 126, 155, 206, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(32, 151, 138, "MAIN KEYS", CYAN, 1);
    draw_text_in_box(32, 151, 150, "SPACE START", WHITE, 1);
    draw_text_in_box(32, 151, 160, "A AUTO MANUAL", WHITE, 1);
    draw_text_in_box(32, 151, 170, "1 NS GREEN", WHITE, 1);
    draw_text_in_box(32, 151, 180, "2 EW GREEN", WHITE, 1);
    draw_text_in_box(32, 151, 190, "3 ALL STOP", WHITE, 1);

    draw_panel(165, 126, 292, 206, DARKGRAY, CYAN);
    draw_text_in_box(169, 288, 138, "WALK KEYS", CYAN, 1);
    draw_text_in_box(169, 288, 150, "4 ADD UP DOWN", WHITE, 1);
    draw_text_in_box(169, 288, 160, "5 ADD SIDE", WHITE, 1);
    draw_text_in_box(169, 288, 170, "P PAUSE", WHITE, 1);
    draw_text_in_box(169, 288, 180, "R RESTART", WHITE, 1);
    draw_text_in_box(169, 288, 190, "S TITLE", WHITE, 1);

    draw_label_strip(44, 212, 160, 226, DARKGRAY);
    draw_label_strip(162, 212, 276, 226, DARKGRAY);
    draw_text_in_box(48, 156, 216, "SPACE PLAY", WHITE, 1);
    draw_text_in_box(166, 272, 216, "S BACK", WHITE, 1);
}

void draw_instructions(void) {
    draw_static_scene(draw_instructions_scene);
}

void draw_paused_scene(void) {
    draw_intersection_base();
    draw_lights();
    draw_crosswalk_guides();
    draw_waiting_pedestrians();
    draw_cars();
    draw_pedestrians();
    draw_hud();
    draw_box(54, 82, 265, 160, BLACK);
    draw_box(58, 86, 261, 156, DARKGRAY);
    draw_box(58, 86, 261, 92, ROAD_EDGE);
    draw_text_centered(100, "PAUSED", YELLOW, 2);
    draw_text_centered(122, "SPACE OR P RESUME", WHITE, 1);
    draw_text_centered(136, "R RESET", CYAN, 1);
    draw_text_centered(148, "S TITLE", WHITE, 1);
}

void draw_paused(void) {
    draw_static_scene(draw_paused_scene);
}

void draw_game_over_scene(void) {
    short accent = (end_reason == END_CRASH) ? RED : GREEN;
    short sub_color = (end_reason == END_CRASH) ? ORANGE : CYAN;

    draw_page_frame(CITY_BG);
    draw_panel(42, 30, 277, 88, DARKGRAY, accent);
    if (end_reason == END_CRASH) {
        draw_text_in_box(46, 273, 42, "CRASH OUT", RED, 3);
        draw_text_in_box(46, 273, 74, "YOU LOST BY COLLISION", sub_color, 1);
    } else {
        draw_text_in_box(46, 273, 46, "ROUND CLEAR", GREEN, 2);
        draw_text_in_box(46, 273, 74, "SAFE ROUND COMPLETE", sub_color, 1);
    }

    draw_panel(26, 102, 118, 176, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(30, 114, 114, "SCORE", WHITE, 1);
    draw_int_in_box(30, 114, 140, score, YELLOW, 2);

    draw_panel(126, 102, 218, 176, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(130, 214, 114, "PASS", WHITE, 1);
    draw_int_in_box(130, 214, 140, passed, GREEN, 2);

    draw_panel(226, 102, 294, 176, DARKGRAY, ROAD_EDGE);
    draw_text_in_box(230, 290, 114, "PEOPLE", WHITE, 1);
    draw_int_in_box(230, 290, 140, ped_groups_served, CYAN, 2);

    draw_label_strip(36, 192, 148, 214, DARKGRAY);
    draw_label_strip(170, 192, 284, 214, DARKGRAY);
    draw_text_in_box(40, 144, 199, "SPACE RETRY", WHITE, 1);
    draw_text_in_box(174, 280, 199, "S TITLE", WHITE, 1);
}

void draw_game_over(void) {
    draw_static_scene(draw_game_over_scene);
}

// Main controls:
// TITLE: SPACE start, I info
// INSTRUCTIONS: SPACE start, S back
// PLAYING: SPACE/P pause, A auto/manual, 1 force NS, 2 force EW, 3 all stop,
//          4 add up/down people, 5 add side people, R restart, S title
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
                update_rush_cycle();
                maybe_queue_ped_request();
                if (ped_waiting_horizontal > 0) ped_wait_ticks_horizontal++;
                if (ped_waiting_vertical > 0) ped_wait_ticks_vertical++;
                maybe_spawn_car();
                update_cars();
                bool ped_phase_before = is_ped_walk_state(light_state);
                update_pedestrians();
                if (detect_crash()) {
                    end_reason = END_CRASH;
                    scene = SCENE_GAME_OVER;
                    draw_game_over();
                    continue;
                }
                if (!ped_phase_before) {
                    if (light_state == NS_YELLOW || light_state == EW_YELLOW || light_state == ALL_RED) {
                        update_light_transition();
                    } else if (mode == AUTO_MODE) {
                        update_lights_auto();
                    }
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
                    request_light_state(PED_ALL_WALK);
                    redraw_all();
                } else if (key == 0x25) {   // '4'
                    request_light_state(PED_VERTICAL_WALK);
                    redraw_all();
                } else if (key == 0x2E) {   // '5'
                    request_light_state(PED_HORIZONTAL_WALK);
                    redraw_all();
                } else if (key == 0x1C) {   // 'A'
                    mode = (mode == AUTO_MODE) ? MANUAL_MODE : AUTO_MODE;
                    if (mode == AUTO_MODE && light_state == ALL_RED && next_green_state == ALL_RED) {
                        next_green_state = (ped_waiting_horizontal > 0 && ped_waiting_vertical > 0)
                                              ? PED_ALL_WALK : choose_resume_green_state();
                        phase_ticks = 0;
                    }
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
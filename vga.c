#include <stdbool.h>
#include <stdint.h>

#define PS2_BASE        0xFF200100
#define TIMER1_BASE     0xFF202000
#define PIXEL_CTRL_BASE 0xFF203020
#define PIXEL_BUF_BASE  0xC8000000

#define SCREEN_W 320
#define SCREEN_H 240

#define TICK_COUNTS         3333333U
#define MAX_VEHICLES        52
#define FPS                 30
#define RENDER_DIVIDER      2
#define ROUND_SECONDS       120
#define ROUND_TICKS         (FPS * ROUND_SECONDS)
#define YELLOW_TICKS        18
#define ALL_RED_TICKS       10
#define MIN_GREEN_TICKS     20
#define CAR_SPEED           2
#define CAR_LONG            14
#define CAR_SHORT           8
#define FOLLOW_SPACING      20
#define STOP_GAP            4
#define GRIDLOCK_QUEUE      8

#define ROAD_X1 112
#define ROAD_X2 208
#define ROAD_Y1 66
#define ROAD_Y2 174

#define N_TURN_X       134
#define N_STRAIGHT_X   150
#define S_STRAIGHT_X   170
#define S_TURN_X       186
#define W_STRAIGHT_Y   138
#define W_TURN_Y       154
#define E_STRAIGHT_Y   102
#define E_TURN_Y       86

#define TO_WEST_Y      102
#define TO_EAST_Y      138
#define TO_NORTH_X     186
#define TO_SOUTH_X     134

#define TURN_N_TRIGGER_Y  88
#define TURN_S_TRIGGER_Y  152
#define TURN_W_TRIGGER_X  136
#define TURN_E_TRIGGER_X  184

#define STOP_N (ROAD_Y1 - CAR_LONG - STOP_GAP)
#define STOP_S (ROAD_Y2 + STOP_GAP)
#define STOP_W (ROAD_X1 - CAR_LONG - STOP_GAP)
#define STOP_E (ROAD_X2 + STOP_GAP)

#define BLACK        0x0000
#define WHITE        0xFFFF
#define RED          0xF800
#define GREEN        0x07E0
#define BLUE         0x001F
#define YELLOW       0xFFE0
#define CYAN         0x07FF
#define MAGENTA      0xF81F
#define ORANGE       0xFD20
#define SILVER       0xC618
#define ROAD         0x3A2A
#define GRASS        0x0600
#define DARKGRAY     0x4208
#define LIGHTGRAY    0xA514
#define DARKRED      0x8000
#define DARKGREEN    0x0400
#define DARKYELLOW   0x8400
#define NAVY         0x0010
#define CITY_BG      0x2145
#define SIDEWALK     0xBDF7
#define PANEL_BG     0x18E3
#define PANEL_EDGE   0x7BEF
#define LANE_FAINT   0x7BEF
#define ASPHALT_DARK 0x2965
#define POLE_GRAY    0x94B2
#define GLASS        0xBEFF
#define PANEL_TOP    0x2968
#define PANEL_INNER  0x10A2
#define BUILDING     0x5AEB
#define WINDOW_LIT   0xFF7A
#define WINDOW_DARK  0x39E7
#define ROAD_EDGE    0x6B4D
#define CROSSWALK    0xEF7D
#define CAR_SHADOW   0x1082

typedef enum {
    APPROACH_NORTH = 0,
    APPROACH_SOUTH,
    APPROACH_WEST,
    APPROACH_EAST
} Approach;

typedef enum {
    FLOW_NS = 0,
    FLOW_EW
} FlowAxis;

typedef enum {
    SCENE_TITLE = 0,
    SCENE_PLAYING,
    SCENE_PAUSED,
    SCENE_GAME_OVER
} Scene;

typedef enum {
    END_REASON_NONE = 0,
    END_REASON_TIME,
    END_REASON_GRIDLOCK,
    END_REASON_STARVATION
} EndReason;

typedef enum {
    PHASE_NS_GREEN = 0,
    PHASE_NS_YELLOW,
    PHASE_ALL_RED_TO_EW,
    PHASE_EW_GREEN,
    PHASE_EW_YELLOW,
    PHASE_ALL_RED_TO_NS
} SignalPhase;

typedef enum {
    ACTION_NONE = 0,
    ACTION_START,
    ACTION_REQUEST_NS,
    ACTION_REQUEST_EW,
    ACTION_PAUSE,
    ACTION_RESTART
} InputAction;

typedef struct {
    int dx;
    int dy;
} Vec2;

typedef enum {
    LANE_STRAIGHT = 0,
    LANE_TURN
} LaneType;

typedef enum {
    MANEUVER_STRAIGHT = 0,
    MANEUVER_TURN
} Maneuver;

typedef enum {
    DIR_DOWN = 0,
    DIR_UP,
    DIR_RIGHT,
    DIR_LEFT
} Direction;

typedef struct {
    bool active;
    bool waiting;
    bool scored;
    bool entered_conflict;
    bool turning;
    Approach approach;
    LaneType lane;
    Maneuver maneuver;
    Direction dir;
    int x;
    int y;
    int w;
    int h;
    short color;
} Vehicle;

typedef struct {
    SignalPhase phase;
    FlowAxis requested_flow;
    int phase_ticks;
    int hold_ticks;
} SignalController;

typedef struct {
    Scene scene;
    EndReason end_reason;
    SignalController signal;
    Vehicle vehicles[MAX_VEHICLES];
    int spawn_ticks[4];
    int queue_lengths[4];
    int throughput;
    int wait_ticks;
    int pressure_ticks;
    int danger_ticks;
    int score;
    int best_score;
    int elapsed_ticks;
    int starvation_ticks[4];
    uint32_t rng_state;
} GameState;

static volatile int *const ps2_ptr = (int *)PS2_BASE;
static volatile int *const timer_ptr = (int *)TIMER1_BASE;
static volatile int *const pixel_ctrl_ptr = (int *)PIXEL_CTRL_BASE;
static volatile short *pixel_buffer = (short *)PIXEL_BUF_BASE;

static void clear_screen(short color);

static void video_init(void) {
    uint32_t front_buffer = (uint32_t)(*pixel_ctrl_ptr);
    if (front_buffer == 0u) {
        front_buffer = PIXEL_BUF_BASE;
    }
    pixel_buffer = (volatile short *)(uintptr_t)front_buffer;
    clear_screen(BLACK);
}

static int text_length(const char *text) {
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static uint32_t next_random(GameState *game) {
    game->rng_state = game->rng_state * 1664525u + 1013904223u;
    return game->rng_state;
}

static int random_between(GameState *game, int low, int high) {
    if (high <= low) {
        return low;
    }
    return low + (int)(next_random(game) % (uint32_t)(high - low + 1));
}

static void plot_pixel(int x, int y, short color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return;
    }

    volatile short *addr =
        (volatile short *)((uintptr_t)pixel_buffer + (uintptr_t)(y << 10) + (uintptr_t)(x << 1));
    *addr = color;
}

static void draw_box(int x1, int y1, int x2, int y2, short color) {
    if (x1 > x2) {
        int temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        int temp = y1;
        y1 = y2;
        y2 = temp;
    }

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

static void draw_rect_outline(int x1, int y1, int x2, int y2, short color) {
    draw_box(x1, y1, x2, y1, color);
    draw_box(x1, y2, x2, y2, color);
    draw_box(x1, y1, x1, y2, color);
    draw_box(x2, y1, x2, y2, color);
}

static void clear_screen(short color) {
    draw_box(0, 0, SCREEN_W - 1, SCREEN_H - 1, color);
}

static void timer_init(uint32_t period_counts) {
    *(timer_ptr + 1) = 0x8;
    *(timer_ptr + 2) = (int)(period_counts & 0xFFFFu);
    *(timer_ptr + 3) = (int)((period_counts >> 16) & 0xFFFFu);
    *(timer_ptr + 0) = 0;
    *(timer_ptr + 1) = 0x6;
}

static bool timer_expired(void) {
    int status = *(timer_ptr + 0);
    if ((status & 0x1) != 0) {
        *(timer_ptr + 0) = 0;
        return true;
    }
    return false;
}

static int ps2_get_make_code(void) {
    static bool break_pending = false;
    static bool ext_pending = false;

    int data = *ps2_ptr;
    if ((data & 0x8000) == 0) {
        return -1;
    }

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

    if (ext_pending) {
        ext_pending = false;
        return 0xE000 | byte;
    }

    return byte;
}

static const uint8_t *glyph_for_char(char ch) {
    static const uint8_t space[7]   = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t dash[7]    = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t colon[7]   = {0, 4, 0, 0, 4, 0, 0};
    static const uint8_t zero[7]    = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t one[7]     = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t two[7]     = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t three[7]   = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t four[7]    = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t five[7]    = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t six[7]     = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t seven[7]   = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t eight[7]   = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t nine[7]    = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t letter_a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t letter_b[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t letter_c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t letter_d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t letter_e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t letter_f[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t letter_g[7] = {14, 17, 16, 23, 17, 17, 14};
    static const uint8_t letter_h[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t letter_i[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t letter_k[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t letter_l[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t letter_m[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t letter_n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t letter_o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t letter_p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t letter_q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t letter_r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t letter_s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t letter_t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t letter_u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t letter_v[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t letter_w[7] = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t letter_y[7] = {17, 17, 10, 4, 4, 4, 4};

    switch (ch) {
        case '0': return zero;
        case '1': return one;
        case '2': return two;
        case '3': return three;
        case '4': return four;
        case '5': return five;
        case '6': return six;
        case '7': return seven;
        case '8': return eight;
        case '9': return nine;
        case 'A': return letter_a;
        case 'B': return letter_b;
        case 'C': return letter_c;
        case 'D': return letter_d;
        case 'E': return letter_e;
        case 'F': return letter_f;
        case 'G': return letter_g;
        case 'H': return letter_h;
        case 'I': return letter_i;
        case 'K': return letter_k;
        case 'L': return letter_l;
        case 'M': return letter_m;
        case 'N': return letter_n;
        case 'O': return letter_o;
        case 'P': return letter_p;
        case 'Q': return letter_q;
        case 'R': return letter_r;
        case 'S': return letter_s;
        case 'T': return letter_t;
        case 'U': return letter_u;
        case 'V': return letter_v;
        case 'W': return letter_w;
        case 'Y': return letter_y;
        case '-': return dash;
        case ':': return colon;
        case ' ': return space;
        default:  return space;
    }
}

static void draw_char(int x, int y, char ch, short color, int scale) {
    const uint8_t *rows = glyph_for_char(ch);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if ((rows[row] & (1 << (4 - col))) != 0) {
                draw_box(x + col * scale, y + row * scale,
                         x + col * scale + scale - 1, y + row * scale + scale - 1, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char *text, short color, int scale) {
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(x + i * scale * 6, y, text[i], color, scale);
    }
}

static void draw_text_centered(int y, const char *text, short color, int scale) {
    int width = text_length(text) * scale * 6 - scale;
    int x = (SCREEN_W - width) / 2;
    draw_text(x, y, text, color, scale);
}

static void draw_int(int x, int y, int value, short color, int scale) {
    char buffer[12];
    int index = 0;
    bool negative = false;

    if (value < 0) {
        negative = true;
        value = -value;
    }

    do {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && index < 10);

    if (negative && index < 11) {
        buffer[index++] = '-';
    }

    for (int i = 0; i < index / 2; i++) {
        char temp = buffer[i];
        buffer[i] = buffer[index - 1 - i];
        buffer[index - 1 - i] = temp;
    }
    buffer[index] = '\0';

    draw_text(x, y, buffer, color, scale);
}

static FlowAxis flow_for_approach(Approach approach) {
    if (approach == APPROACH_NORTH || approach == APPROACH_SOUTH) {
        return FLOW_NS;
    }
    return FLOW_EW;
}

static bool rects_overlap(const Vehicle *vehicle, int x1, int y1, int x2, int y2) {
    int vx2 = vehicle->x + vehicle->w - 1;
    int vy2 = vehicle->y + vehicle->h - 1;
    if (vx2 < x1 || vehicle->x > x2) return false;
    if (vy2 < y1 || vehicle->y > y2) return false;
    return true;
}

static bool vehicle_in_conflict_zone(const Vehicle *vehicle) {
    return rects_overlap(vehicle, ROAD_X1, ROAD_Y1, ROAD_X2, ROAD_Y2);
}

static bool vehicle_past_stop_line(const Vehicle *vehicle) {
    switch (vehicle->approach) {
        case APPROACH_NORTH: return vehicle->y > STOP_N;
        case APPROACH_SOUTH: return vehicle->y < STOP_S;
        case APPROACH_WEST:  return vehicle->x > STOP_W;
        case APPROACH_EAST:  return vehicle->x < STOP_E;
        default:             return false;
    }
}

static int vehicle_sort_coord(const Vehicle *vehicle) {
    if (vehicle->approach == APPROACH_NORTH || vehicle->approach == APPROACH_SOUTH) {
        return vehicle->y;
    }
    return vehicle->x;
}

static bool is_ahead_of(const Vehicle *lhs, const Vehicle *rhs) {
    if (lhs->approach == APPROACH_NORTH || lhs->approach == APPROACH_WEST) {
        return vehicle_sort_coord(lhs) > vehicle_sort_coord(rhs);
    }
    return vehicle_sort_coord(lhs) < vehicle_sort_coord(rhs);
}

static void phase_to_lights(SignalPhase phase, short *ns_red, short *ns_yellow, short *ns_green,
                            short *ew_red, short *ew_yellow, short *ew_green) {
    *ns_red = DARKRED;
    *ns_yellow = DARKYELLOW;
    *ns_green = DARKGREEN;
    *ew_red = DARKRED;
    *ew_yellow = DARKYELLOW;
    *ew_green = DARKGREEN;

    switch (phase) {
        case PHASE_NS_GREEN:
            *ns_green = GREEN;
            *ew_red = RED;
            break;
        case PHASE_NS_YELLOW:
            *ns_yellow = YELLOW;
            *ew_red = RED;
            break;
        case PHASE_ALL_RED_TO_EW:
        case PHASE_ALL_RED_TO_NS:
            *ns_red = RED;
            *ew_red = RED;
            break;
        case PHASE_EW_GREEN:
            *ew_green = GREEN;
            *ns_red = RED;
            break;
        case PHASE_EW_YELLOW:
            *ew_yellow = YELLOW;
            *ns_red = RED;
            break;
    }
}

static bool approach_has_green(const SignalController *signal, Approach approach) {
    FlowAxis flow = flow_for_approach(approach);
    if (signal->phase == PHASE_NS_GREEN && flow == FLOW_NS) return true;
    if (signal->phase == PHASE_EW_GREEN && flow == FLOW_EW) return true;
    return false;
}

static const char *phase_label(SignalPhase phase) {
    switch (phase) {
        case PHASE_NS_GREEN:      return "NS GREEN";
        case PHASE_NS_YELLOW:     return "NS YELLOW";
        case PHASE_ALL_RED_TO_EW: return "ALL RED";
        case PHASE_EW_GREEN:      return "EW GREEN";
        case PHASE_EW_YELLOW:     return "EW YELLOW";
        case PHASE_ALL_RED_TO_NS: return "ALL RED";
        default:                  return "ALL RED";
    }
}

static short random_car_color(GameState *game) {
    static const short palette[6] = {CYAN, BLUE, MAGENTA, ORANGE, SILVER, YELLOW};
    return palette[random_between(game, 0, 5)];
}

static void clear_vehicles(GameState *game) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        game->vehicles[i].active = false;
        game->vehicles[i].waiting = false;
        game->vehicles[i].scored = false;
        game->vehicles[i].entered_conflict = false;
        game->vehicles[i].turning = false;
    }
}

static void signal_reset(SignalController *signal) {
    signal->phase = PHASE_NS_GREEN;
    signal->requested_flow = FLOW_NS;
    signal->phase_ticks = 0;
    signal->hold_ticks = MIN_GREEN_TICKS;
}

static void start_round(GameState *game) {
    game->scene = SCENE_PLAYING;
    game->end_reason = END_REASON_NONE;
    game->throughput = 0;
    game->wait_ticks = 0;
    game->pressure_ticks = 0;
    game->danger_ticks = 0;
    game->score = 0;
    game->elapsed_ticks = 0;
    clear_vehicles(game);
    signal_reset(&game->signal);

    for (int i = 0; i < 4; i++) {
        game->queue_lengths[i] = 0;
        game->starvation_ticks[i] = 0;
        game->spawn_ticks[i] = random_between(game, 6, 30);
    }
}

static void init_game(GameState *game) {
    game->scene = SCENE_TITLE;
    game->end_reason = END_REASON_NONE;
    game->best_score = 0;
    game->rng_state = 0x2432026u;
    start_round(game);
    game->scene = SCENE_TITLE;
}

static int difficulty_level(const GameState *game) {
    return game->elapsed_ticks / (FPS * 18);
}

static int spawn_interval(GameState *game) {
    int level = difficulty_level(game);
    int minimum = 18;
    int base = 74 - level * 5;
    if (base < minimum) {
        base = minimum;
    }
    return random_between(game, base, base + 20);
}

static Direction inbound_direction(Approach approach) {
    switch (approach) {
        case APPROACH_NORTH: return DIR_DOWN;
        case APPROACH_SOUTH: return DIR_UP;
        case APPROACH_WEST:  return DIR_RIGHT;
        case APPROACH_EAST:  return DIR_LEFT;
        default:             return DIR_DOWN;
    }
}

static int lane_spawn_x(Approach approach, LaneType lane) {
    switch (approach) {
        case APPROACH_NORTH: return (lane == LANE_TURN) ? N_TURN_X : N_STRAIGHT_X;
        case APPROACH_SOUTH: return (lane == LANE_TURN) ? S_TURN_X : S_STRAIGHT_X;
        case APPROACH_WEST:  return -CAR_LONG;
        case APPROACH_EAST:  return SCREEN_W + CAR_LONG;
        default:             return 0;
    }
}

static int lane_spawn_y(Approach approach, LaneType lane) {
    switch (approach) {
        case APPROACH_NORTH: return -CAR_LONG;
        case APPROACH_SOUTH: return SCREEN_H + CAR_LONG;
        case APPROACH_WEST:  return (lane == LANE_TURN) ? W_TURN_Y : W_STRAIGHT_Y;
        case APPROACH_EAST:  return (lane == LANE_TURN) ? E_TURN_Y : E_STRAIGHT_Y;
        default:             return 0;
    }
}

static int risk_percent(const GameState *game) {
    int risk = (game->danger_ticks * 100) / (FPS * 12);
    if (risk < 0) {
        risk = 0;
    }
    if (risk > 99) {
        risk = 99;
    }
    return risk;
}

static const char *end_reason_label(EndReason reason) {
    switch (reason) {
        case END_REASON_TIME:       return "TIME UP";
        case END_REASON_GRIDLOCK:   return "GRIDLOCK";
        case END_REASON_STARVATION: return "STARVATION";
        default:                    return "TIME UP";
    }
}

static void move_vehicle_forward(Vehicle *vehicle, int step) {
    switch (vehicle->dir) {
        case DIR_DOWN:  vehicle->y += step; break;
        case DIR_UP:    vehicle->y -= step; break;
        case DIR_RIGHT: vehicle->x += step; break;
        case DIR_LEFT:  vehicle->x -= step; break;
    }
}

static void advance_committed_vehicle(Vehicle *vehicle) {
    if (vehicle->maneuver == MANEUVER_TURN) {
        switch (vehicle->approach) {
            case APPROACH_NORTH:
                if (!vehicle->turning && vehicle->dir == DIR_DOWN && vehicle->y >= TURN_N_TRIGGER_Y) {
                    vehicle->turning = true;
                }
                if (vehicle->turning && vehicle->dir == DIR_DOWN) {
                    vehicle->x -= 1;
                    vehicle->y += 1;
                    if (vehicle->y >= TO_WEST_Y) {
                        vehicle->y = TO_WEST_Y;
                        vehicle->dir = DIR_LEFT;
                        vehicle->turning = false;
                    }
                    return;
                }
                break;

            case APPROACH_SOUTH:
                if (!vehicle->turning && vehicle->dir == DIR_UP && vehicle->y <= TURN_S_TRIGGER_Y) {
                    vehicle->turning = true;
                }
                if (vehicle->turning && vehicle->dir == DIR_UP) {
                    vehicle->x += 1;
                    vehicle->y -= 1;
                    if (vehicle->y <= TO_EAST_Y) {
                        vehicle->y = TO_EAST_Y;
                        vehicle->dir = DIR_RIGHT;
                        vehicle->turning = false;
                    }
                    return;
                }
                break;

            case APPROACH_WEST:
                if (!vehicle->turning && vehicle->dir == DIR_RIGHT && vehicle->x >= TURN_W_TRIGGER_X) {
                    vehicle->turning = true;
                }
                if (vehicle->turning && vehicle->dir == DIR_RIGHT) {
                    vehicle->x += 1;
                    vehicle->y -= 1;
                    if (vehicle->x >= TO_NORTH_X) {
                        vehicle->x = TO_NORTH_X;
                        vehicle->dir = DIR_UP;
                        vehicle->turning = false;
                    }
                    return;
                }
                break;

            case APPROACH_EAST:
                if (!vehicle->turning && vehicle->dir == DIR_LEFT && vehicle->x <= TURN_E_TRIGGER_X) {
                    vehicle->turning = true;
                }
                if (vehicle->turning && vehicle->dir == DIR_LEFT) {
                    vehicle->x -= 1;
                    vehicle->y += 1;
                    if (vehicle->x <= TO_SOUTH_X) {
                        vehicle->x = TO_SOUTH_X;
                        vehicle->dir = DIR_DOWN;
                        vehicle->turning = false;
                    }
                    return;
                }
                break;
        }
    }

    move_vehicle_forward(vehicle, CAR_SPEED);
}

static bool spawn_zone_clear(const GameState *game, Approach approach, LaneType lane) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        const Vehicle *vehicle = &game->vehicles[i];
        if (!vehicle->active || vehicle->approach != approach || vehicle->lane != lane) {
            continue;
        }

        switch (approach) {
            case APPROACH_NORTH:
                if (vehicle->dir == DIR_DOWN && vehicle->y < FOLLOW_SPACING * 2) return false;
                break;
            case APPROACH_SOUTH:
                if (vehicle->dir == DIR_UP && vehicle->y > SCREEN_H - FOLLOW_SPACING * 2 - CAR_LONG) return false;
                break;
            case APPROACH_WEST:
                if (vehicle->dir == DIR_RIGHT && vehicle->x < FOLLOW_SPACING * 2) return false;
                break;
            case APPROACH_EAST:
                if (vehicle->dir == DIR_LEFT && vehicle->x > SCREEN_W - FOLLOW_SPACING * 2 - CAR_LONG) return false;
                break;
        }
    }
    return true;
}

static void spawn_vehicle(GameState *game, Approach approach) {
    LaneType lane = LANE_STRAIGHT;
    if (!spawn_zone_clear(game, approach, lane)) {
        return;
    }

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *vehicle = &game->vehicles[i];
        if (vehicle->active) {
            continue;
        }

        vehicle->active = true;
        vehicle->waiting = false;
        vehicle->scored = false;
        vehicle->entered_conflict = false;
        vehicle->turning = false;
        vehicle->approach = approach;
        vehicle->lane = lane;
        vehicle->maneuver = (lane == LANE_TURN) ? MANEUVER_TURN : MANEUVER_STRAIGHT;
        vehicle->dir = inbound_direction(approach);
        vehicle->color = random_car_color(game);
        vehicle->x = lane_spawn_x(approach, lane);
        vehicle->y = lane_spawn_y(approach, lane);

        if (approach == APPROACH_NORTH || approach == APPROACH_SOUTH) {
            vehicle->w = CAR_SHORT;
            vehicle->h = CAR_LONG;
        } else {
            vehicle->w = CAR_LONG;
            vehicle->h = CAR_SHORT;
        }
        return;
    }
}

static bool any_vehicle_in_conflict(const GameState *game) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (game->vehicles[i].active && vehicle_in_conflict_zone(&game->vehicles[i])) {
            return true;
        }
    }
    return false;
}

static bool desired_rect_hits_conflict(const Vehicle *vehicle, int next_coord) {
    Vehicle temp = *vehicle;
    if (vehicle->approach == APPROACH_NORTH || vehicle->approach == APPROACH_SOUTH) {
        temp.y = next_coord;
    } else {
        temp.x = next_coord;
    }
    return vehicle_in_conflict_zone(&temp);
}

static int clamp_to_stop_line(const Vehicle *vehicle, int desired) {
    switch (vehicle->approach) {
        case APPROACH_NORTH:
            if (desired > STOP_N) desired = STOP_N;
            break;
        case APPROACH_SOUTH:
            if (desired < STOP_S) desired = STOP_S;
            break;
        case APPROACH_WEST:
            if (desired > STOP_W) desired = STOP_W;
            break;
        case APPROACH_EAST:
            if (desired < STOP_E) desired = STOP_E;
            break;
    }
    return desired;
}

static int apply_follow_limit(const Vehicle *vehicle, int desired, int leader_coord) {
    if (vehicle->approach == APPROACH_NORTH || vehicle->approach == APPROACH_WEST) {
        int max_coord = leader_coord - FOLLOW_SPACING;
        if (desired > max_coord) {
            desired = max_coord;
        }
    } else {
        int min_coord = leader_coord + FOLLOW_SPACING;
        if (desired < min_coord) {
            desired = min_coord;
        }
    }
    return desired;
}

static int active_vehicle_count(const GameState *game) {
    int count = 0;
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (game->vehicles[i].active) {
            count++;
        }
    }
    return count;
}

static void update_signal(GameState *game) {
    SignalController *signal = &game->signal;
    if (signal->phase_ticks > 0) {
        signal->phase_ticks--;
    }

    if (signal->hold_ticks > 0) {
        signal->hold_ticks--;
    }

    switch (signal->phase) {
        case PHASE_NS_GREEN:
            if (signal->requested_flow == FLOW_EW && signal->hold_ticks <= 0) {
                signal->phase = PHASE_NS_YELLOW;
                signal->phase_ticks = YELLOW_TICKS;
            }
            break;

        case PHASE_EW_GREEN:
            if (signal->requested_flow == FLOW_NS && signal->hold_ticks <= 0) {
                signal->phase = PHASE_EW_YELLOW;
                signal->phase_ticks = YELLOW_TICKS;
            }
            break;

        case PHASE_NS_YELLOW:
            if (signal->phase_ticks <= 0) {
                signal->phase = PHASE_ALL_RED_TO_EW;
                signal->phase_ticks = ALL_RED_TICKS;
            }
            break;

        case PHASE_EW_YELLOW:
            if (signal->phase_ticks <= 0) {
                signal->phase = PHASE_ALL_RED_TO_NS;
                signal->phase_ticks = ALL_RED_TICKS;
            }
            break;

        case PHASE_ALL_RED_TO_EW:
            if (signal->phase_ticks <= 0) {
                signal->phase = (signal->requested_flow == FLOW_NS) ? PHASE_NS_GREEN : PHASE_EW_GREEN;
                signal->hold_ticks = MIN_GREEN_TICKS;
            }
            break;

        case PHASE_ALL_RED_TO_NS:
            if (signal->phase_ticks <= 0) {
                signal->phase = (signal->requested_flow == FLOW_EW) ? PHASE_EW_GREEN : PHASE_NS_GREEN;
                signal->hold_ticks = MIN_GREEN_TICKS;
            }
            break;
    }
}

static void update_lane(GameState *game, Approach approach, LaneType lane, bool *conflict_locked) {
    int order[MAX_VEHICLES];
    int order_count = 0;
    bool processed[MAX_VEHICLES] = {false};

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *vehicle = &game->vehicles[i];
        if (!vehicle->active || vehicle->approach != approach || vehicle->lane != lane) {
            continue;
        }

        if (vehicle->dir == inbound_direction(approach) && !vehicle->turning) {
            order[order_count++] = i;
        }
    }

    for (int i = 0; i < order_count; i++) {
        for (int j = i + 1; j < order_count; j++) {
            Vehicle *lhs = &game->vehicles[order[i]];
            Vehicle *rhs = &game->vehicles[order[j]];
            if (!is_ahead_of(lhs, rhs)) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }

    int last_coord = 0;
    bool have_leader = false;

    for (int i = 0; i < order_count; i++) {
        Vehicle *vehicle = &game->vehicles[order[i]];
        processed[order[i]] = true;
        int current = vehicle_sort_coord(vehicle);
        int desired = current;
        vehicle->waiting = false;

        switch (approach) {
            case APPROACH_NORTH:
            case APPROACH_WEST:
                desired = current + CAR_SPEED;
                break;
            case APPROACH_SOUTH:
            case APPROACH_EAST:
                desired = current - CAR_SPEED;
                break;
        }

        if (!vehicle_past_stop_line(vehicle)) {
            bool may_enter = approach_has_green(&game->signal, approach) && !(*conflict_locked);
            if (!may_enter) {
                desired = clamp_to_stop_line(vehicle, desired);
            } else if (desired_rect_hits_conflict(vehicle, desired)) {
                *conflict_locked = true;
            }
        }

        if (have_leader) {
            desired = apply_follow_limit(vehicle, desired, last_coord);
        }

        if (desired == current && !vehicle_past_stop_line(vehicle)) {
            vehicle->waiting = true;
        }

        if (approach == APPROACH_NORTH || approach == APPROACH_SOUTH) {
            vehicle->y = desired;
        } else {
            vehicle->x = desired;
        }

        if (vehicle_in_conflict_zone(vehicle)) {
            vehicle->entered_conflict = true;
        }

        last_coord = vehicle_sort_coord(vehicle);
        have_leader = true;
    }

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *vehicle = &game->vehicles[i];
        if (!vehicle->active || vehicle->approach != approach || vehicle->lane != lane) {
            continue;
        }

        if (processed[i]) {
            continue;
        }

        if (vehicle->dir == inbound_direction(approach) && !vehicle->turning && !vehicle_past_stop_line(vehicle)) {
            continue;
        }

        vehicle->waiting = false;
        advance_committed_vehicle(vehicle);
        if (vehicle_in_conflict_zone(vehicle)) {
            vehicle->entered_conflict = true;
            *conflict_locked = true;
        }
    }
}

static void update_spawns(GameState *game) {
    for (int approach = 0; approach < 4; approach++) {
        game->spawn_ticks[approach]--;
        if (game->spawn_ticks[approach] <= 0) {
            spawn_vehicle(game, (Approach)approach);
            game->spawn_ticks[approach] = spawn_interval(game);
        }
    }
}

static void update_metrics_and_cleanup(GameState *game) {
    int max_queue = 0;
    int total_pressure = 0;
    int starving_approach = -1;

    for (int approach = 0; approach < 4; approach++) {
        game->queue_lengths[approach] = 0;
    }

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *vehicle = &game->vehicles[i];
        if (!vehicle->active) {
            continue;
        }

        if (!vehicle_past_stop_line(vehicle)) {
            game->queue_lengths[vehicle->approach]++;
        }

        if (vehicle->waiting) {
            game->wait_ticks++;
        }

        if (vehicle_in_conflict_zone(vehicle)) {
            vehicle->entered_conflict = true;
        }

        if (vehicle->entered_conflict && !vehicle_in_conflict_zone(vehicle) && !vehicle->scored) {
            vehicle->scored = true;
            game->throughput++;
        }

        if (vehicle->x > SCREEN_W + CAR_LONG || vehicle->x + vehicle->w < -CAR_LONG ||
            vehicle->y > SCREEN_H + CAR_LONG || vehicle->y + vehicle->h < -CAR_LONG) {
            vehicle->active = false;
        }
    }

    for (int approach = 0; approach < 4; approach++) {
        if (game->queue_lengths[approach] > max_queue) {
            max_queue = game->queue_lengths[approach];
        }
        if (game->queue_lengths[approach] > 2) {
            total_pressure += game->queue_lengths[approach] - 2;
        }

        if (game->queue_lengths[approach] >= 6) {
            game->starvation_ticks[approach]++;
        } else if (game->starvation_ticks[approach] > 0) {
            game->starvation_ticks[approach] -= 2;
            if (game->starvation_ticks[approach] < 0) {
                game->starvation_ticks[approach] = 0;
            }
        }

        if (game->starvation_ticks[approach] >= FPS * 18) {
            starving_approach = approach;
        }
    }

    game->pressure_ticks += total_pressure;
    if (max_queue >= 7 || active_vehicle_count(game) >= MAX_VEHICLES - 4) {
        game->danger_ticks += 2;
    } else if (max_queue >= 5) {
        game->danger_ticks += 1;
    } else if (game->danger_ticks > 0) {
        game->danger_ticks--;
    }

    game->score = game->throughput * 140 - (game->wait_ticks / 2) - game->pressure_ticks * 2 - game->danger_ticks * 4;
    if (game->score < 0) {
        game->score = 0;
    }

    if (starving_approach != -1) {
        game->scene = SCENE_GAME_OVER;
        game->end_reason = END_REASON_STARVATION;
    } else if (max_queue >= GRIDLOCK_QUEUE + 3 || game->danger_ticks >= FPS * 12) {
        game->scene = SCENE_GAME_OVER;
        game->end_reason = END_REASON_GRIDLOCK;
    } else if (game->elapsed_ticks >= ROUND_TICKS) {
        game->scene = SCENE_GAME_OVER;
        game->end_reason = END_REASON_TIME;
    }

    if (game->scene == SCENE_GAME_OVER && game->score > game->best_score) {
        game->best_score = game->score;
    }
}

static void update_gameplay(GameState *game) {
    game->elapsed_ticks++;
    update_signal(game);
    update_spawns(game);

    bool conflict_locked = any_vehicle_in_conflict(game);
    update_lane(game, APPROACH_NORTH, LANE_STRAIGHT, &conflict_locked);
    conflict_locked = any_vehicle_in_conflict(game);
    update_lane(game, APPROACH_SOUTH, LANE_STRAIGHT, &conflict_locked);
    conflict_locked = any_vehicle_in_conflict(game);
    update_lane(game, APPROACH_WEST, LANE_STRAIGHT, &conflict_locked);
    conflict_locked = any_vehicle_in_conflict(game);
    update_lane(game, APPROACH_EAST, LANE_STRAIGHT, &conflict_locked);

    update_metrics_and_cleanup(game);
}

static InputAction action_from_scancode(int key) {
    switch (key) {
        case 0x29: return ACTION_START;
        case 0x1C: return ACTION_REQUEST_NS;
        case 0x23: return ACTION_REQUEST_EW;
        case 0x4D: return ACTION_PAUSE;
        case 0x2D: return ACTION_RESTART;
        case 0x5A: return ACTION_START;
        default:   return ACTION_NONE;
    }
}

static bool handle_action(GameState *game, InputAction action) {
    if (action == ACTION_NONE) {
        return false;
    }

    switch (game->scene) {
        case SCENE_TITLE:
            if (action == ACTION_START || action == ACTION_RESTART) {
                start_round(game);
                return true;
            }
            return false;

        case SCENE_PLAYING:
            if (action == ACTION_REQUEST_NS) {
                game->signal.requested_flow = FLOW_NS;
                return true;
            }
            if (action == ACTION_REQUEST_EW) {
                game->signal.requested_flow = FLOW_EW;
                return true;
            }
            if (action == ACTION_PAUSE) {
                game->scene = SCENE_PAUSED;
                return true;
            }
            if (action == ACTION_RESTART) {
                start_round(game);
                return true;
            }
            return false;

        case SCENE_PAUSED:
            if (action == ACTION_PAUSE || action == ACTION_START) {
                game->scene = SCENE_PLAYING;
                return true;
            }
            if (action == ACTION_RESTART) {
                start_round(game);
                return true;
            }
            if (action == ACTION_REQUEST_NS) {
                game->signal.requested_flow = FLOW_NS;
                return true;
            }
            if (action == ACTION_REQUEST_EW) {
                game->signal.requested_flow = FLOW_EW;
                return true;
            }
            return false;

        case SCENE_GAME_OVER:
            if (action == ACTION_START || action == ACTION_RESTART) {
                start_round(game);
                return true;
            }
            return false;
    }

    return false;
}

static void draw_light_stack(int x, int y, short top, short mid, short bot) {
    draw_box(x - 4, y - 4, x + 19, y + 47, BLACK);
    draw_box(x - 2, y - 2, x + 17, y + 44, PANEL_EDGE);
    draw_box(x, y, x + 15, y + 41, DARKGRAY);
    draw_rect_outline(x, y, x + 15, y + 41, BLACK);
    draw_box(x + 1, y + 1, x + 14, y + 5, PANEL_TOP);
    draw_box(x + 4, y + 4, x + 11, y + 11, top);
    draw_box(x + 4, y + 16, x + 11, y + 23, mid);
    draw_box(x + 4, y + 28, x + 11, y + 35, bot);
    draw_box(x + 5, y + 5, x + 7, y + 7, WHITE);
    draw_box(x + 5, y + 17, x + 7, y + 19, WHITE);
    draw_box(x + 5, y + 29, x + 7, y + 31, WHITE);
}

static void draw_panel(int x1, int y1, int x2, int y2) {
    draw_box(x1, y1, x2, y2, PANEL_BG);
    draw_box(x1, y1, x2, y1 + 4, PANEL_TOP);
    draw_rect_outline(x1, y1, x2, y2, PANEL_EDGE);
    draw_rect_outline(x1 + 1, y1 + 1, x2 - 1, y2 - 1, BLACK);
    if (x2 - x1 > 8 && y2 - y1 > 8) {
        draw_rect_outline(x1 + 3, y1 + 5, x2 - 3, y2 - 3, PANEL_INNER);
    }
}

static void draw_building_block(int x1, int y1, int x2, int y2) {
    draw_box(x1, y1, x2, y2, BUILDING);
    draw_rect_outline(x1, y1, x2, y2, PANEL_EDGE);
    draw_rect_outline(x1 + 1, y1 + 1, x2 - 1, y2 - 1, BLACK);

    for (int y = y1 + 8; y + 5 < y2; y += 10) {
        for (int x = x1 + 7; x + 5 < x2; x += 10) {
            short color = (((x + y) / 10) % 2 == 0) ? WINDOW_LIT : WINDOW_DARK;
            draw_box(x, y, x + 4, y + 4, color);
        }
    }
}

static void draw_arrow_up(int x, int y, short color) {
    draw_box(x + 3, y, x + 5, y + 8, color);
    draw_box(x + 1, y + 2, x + 7, y + 4, color);
    draw_box(x + 2, y + 9, x + 6, y + 10, color);
}

static void draw_arrow_down(int x, int y, short color) {
    draw_box(x + 3, y + 2, x + 5, y + 10, color);
    draw_box(x + 1, y + 6, x + 7, y + 8, color);
    draw_box(x + 2, y, x + 6, y + 1, color);
}

static void draw_arrow_left(int x, int y, short color) {
    draw_box(x, y + 3, x + 8, y + 5, color);
    draw_box(x + 2, y + 1, x + 4, y + 7, color);
    draw_box(x + 9, y + 2, x + 10, y + 6, color);
}

static void draw_arrow_right(int x, int y, short color) {
    draw_box(x + 2, y + 3, x + 10, y + 5, color);
    draw_box(x + 6, y + 1, x + 8, y + 7, color);
    draw_box(x, y + 2, x + 1, y + 6, color);
}

static void draw_card_label(int x, int y, const char *label, int value, short accent) {
    draw_panel(x, y, x + 72, y + 18);
    draw_text(x + 5, y + 7, label, PANEL_EDGE, 1);
    draw_int(x + 45, y + 7, value, accent, 1);
}

static void draw_intersection_base(void) {
    clear_screen(GRASS);
    draw_box(0, ROAD_Y1, SCREEN_W - 1, ROAD_Y2, ROAD);
    draw_box(ROAD_X1, 0, ROAD_X2, SCREEN_H - 1, ROAD);
    draw_rect_outline(0, ROAD_Y1, SCREEN_W - 1, ROAD_Y2, ROAD_EDGE);
    draw_rect_outline(ROAD_X1, 0, ROAD_X2, SCREEN_H - 1, ROAD_EDGE);
    draw_box(159, 0, 160, SCREEN_H - 1, WHITE);
    draw_box(0, 119, SCREEN_W - 1, 120, WHITE);

    for (int x = 0; x < SCREEN_W; x += 22) {
        if (x + 10 < ROAD_X1 || x > ROAD_X2) {
            draw_box(x, TO_EAST_Y + 3, x + 9, TO_EAST_Y + 5, WHITE);
            draw_box(x, TO_WEST_Y + 3, x + 9, TO_WEST_Y + 5, WHITE);
        }
    }
    for (int y = 0; y < SCREEN_H; y += 22) {
        if (y + 10 < ROAD_Y1 || y > ROAD_Y2) {
            draw_box(TO_SOUTH_X + 3, y, TO_SOUTH_X + 5, y + 9, WHITE);
            draw_box(TO_NORTH_X + 3, y, TO_NORTH_X + 5, y + 9, WHITE);
        }
    }

    draw_arrow_down(N_STRAIGHT_X - 1, 38, WHITE);
    draw_arrow_up(S_STRAIGHT_X - 1, 191, WHITE);
    draw_arrow_right(34, W_STRAIGHT_Y - 1, WHITE);
    draw_arrow_left(274, E_STRAIGHT_Y - 1, WHITE);
}

static void draw_signal_hud(const GameState *game) {
    short ns_red, ns_yellow, ns_green;
    short ew_red, ew_yellow, ew_green;
    phase_to_lights(game->signal.phase, &ns_red, &ns_yellow, &ns_green, &ew_red, &ew_yellow, &ew_green);

    draw_box(151, 42, 154, ROAD_Y1 - 1, POLE_GRAY);
    draw_box(167, ROAD_Y2 + 1, 170, 196, POLE_GRAY);
    draw_box(86, 113, ROAD_X1 - 1, 116, POLE_GRAY);
    draw_box(ROAD_X2 + 1, 124, 232, 127, POLE_GRAY);
    draw_box(146, 38, 159, 43, PANEL_EDGE);
    draw_box(162, 196, 175, 201, PANEL_EDGE);
    draw_box(80, 108, 86, 121, PANEL_EDGE);
    draw_box(232, 120, 238, 133, PANEL_EDGE);

    draw_light_stack(144, 20, ns_red, ns_yellow, ns_green);
    draw_light_stack(160, 196, ns_red, ns_yellow, ns_green);
    draw_light_stack(66, 94, ew_red, ew_yellow, ew_green);
    draw_light_stack(236, 110, ew_red, ew_yellow, ew_green);
}

static void draw_vehicle(const Vehicle *vehicle) {
    if (!vehicle->active) {
        return;
    }
    draw_box(vehicle->x + 1, vehicle->y + 1,
             vehicle->x + vehicle->w, vehicle->y + vehicle->h, CAR_SHADOW);
    draw_box(vehicle->x, vehicle->y, vehicle->x + vehicle->w - 1, vehicle->y + vehicle->h - 1, vehicle->color);
    draw_rect_outline(vehicle->x, vehicle->y, vehicle->x + vehicle->w - 1, vehicle->y + vehicle->h - 1, BLACK);
    if (vehicle->w > vehicle->h) {
        draw_box(vehicle->x + 2, vehicle->y + 1, vehicle->x + vehicle->w - 3, vehicle->y + vehicle->h - 2, GLASS);
        draw_box(vehicle->x + 1, vehicle->y + 1, vehicle->x + 2, vehicle->y + vehicle->h - 2, WHITE);
        draw_box(vehicle->x + vehicle->w - 3, vehicle->y + 2,
                 vehicle->x + vehicle->w - 2, vehicle->y + vehicle->h - 3, RED);
    } else {
        draw_box(vehicle->x + 1, vehicle->y + 2, vehicle->x + vehicle->w - 2, vehicle->y + vehicle->h - 3, GLASS);
        draw_box(vehicle->x + 1, vehicle->y + 1, vehicle->x + vehicle->w - 2, vehicle->y + 2, WHITE);
        draw_box(vehicle->x + 2, vehicle->y + vehicle->h - 3,
                 vehicle->x + vehicle->w - 3, vehicle->y + vehicle->h - 2, RED);
    }

    if (vehicle->maneuver == MANEUVER_TURN) {
        if (vehicle->w > vehicle->h) {
            draw_box(vehicle->x + vehicle->w - 4, vehicle->y + 1,
                     vehicle->x + vehicle->w - 3, vehicle->y + vehicle->h - 2, YELLOW);
        } else {
            draw_box(vehicle->x + 1, vehicle->y + vehicle->h - 4,
                     vehicle->x + vehicle->w - 2, vehicle->y + vehicle->h - 3, YELLOW);
        }
    }
}

static void draw_queue_badge(int x, int y, char label, int value) {
    draw_panel(x, y, x + 34, y + 14);
    draw_char(x + 3, y + 4, label, CYAN, 1);
    draw_int(x + 12, y + 4, value, WHITE, 1);
}

static void draw_status_bar(const GameState *game) {
    int seconds_left = (ROUND_TICKS - game->elapsed_ticks) / FPS;
    if (seconds_left < 0) {
        seconds_left = 0;
    }

    draw_panel(0, 0, SCREEN_W - 1, 24);
    draw_panel(0, SCREEN_H - 24, SCREEN_W - 1, SCREEN_H - 1);

    draw_card_label(4, 3, "SCORE", game->score, CYAN);
    draw_card_label(80, 3, "THRU", game->throughput, GREEN);
    draw_card_label(156, 3, "WAIT", game->wait_ticks / FPS, ORANGE);
    draw_card_label(232, 3, "TIME", seconds_left, YELLOW);

    draw_queue_badge(5, SCREEN_H - 21, 'N', game->queue_lengths[APPROACH_NORTH]);
    draw_queue_badge(44, SCREEN_H - 21, 'S', game->queue_lengths[APPROACH_SOUTH]);
    draw_queue_badge(83, SCREEN_H - 21, 'W', game->queue_lengths[APPROACH_WEST]);
    draw_queue_badge(122, SCREEN_H - 21, 'E', game->queue_lengths[APPROACH_EAST]);

    draw_panel(162, SCREEN_H - 21, 248, SCREEN_H - 6);
    draw_panel(252, SCREEN_H - 21, 314, SCREEN_H - 6);
    draw_text(168, SCREEN_H - 16, "PHASE", PANEL_EDGE, 1);
    draw_text(202, SCREEN_H - 16, phase_label(game->signal.phase), CYAN, 1);
    draw_text(257, SCREEN_H - 16, "RISK", PANEL_EDGE, 1);
    draw_int(289, SCREEN_H - 16, risk_percent(game), RED, 1);
}

static void render_playfield(const GameState *game) {
    draw_intersection_base();

    for (int i = 0; i < MAX_VEHICLES; i++) {
        draw_vehicle(&game->vehicles[i]);
    }

    draw_signal_hud(game);
    draw_status_bar(game);
}

static void render_title_screen(void) {
    clear_screen(CITY_BG);
    draw_building_block(12, 18, 88, 224);
    draw_building_block(232, 18, 308, 224);
    draw_panel(54, 18, 265, 220);
    draw_box(96, 62, 222, 74, ROAD);
    draw_box(150, 36, 166, 126, ROAD);
    draw_box(154, 36, 157, 126, WHITE);
    draw_box(96, 67, 222, 69, WHITE);
    draw_light_stack(134, 36, RED, DARKYELLOW, DARKGREEN);
    draw_light_stack(170, 104, DARKRED, DARKYELLOW, GREEN);
    draw_text_centered(32, "TRAFFIC CTRL", YELLOW, 3);
    draw_text_centered(60, "CITY INTERSECTION MANAGER", PANEL_EDGE, 1);
    draw_panel(76, 104, 243, 124);
    draw_text_centered(111, "A REQUEST NS", CYAN, 1);
    draw_panel(76, 130, 243, 150);
    draw_text_centered(137, "D REQUEST EW", CYAN, 1);
    draw_panel(76, 156, 243, 176);
    draw_text_centered(163, "P PAUSE   R RESTART", WHITE, 1);
    draw_text_centered(192, "KEEP FLOWING  AVOID GRIDLOCK", ORANGE, 1);
    draw_text_centered(210, "SPACE TO START", WHITE, 2);
}

static void render_paused_overlay(void) {
    draw_panel(52, 82, 267, 154);
    draw_text_centered(96, "PAUSED", YELLOW, 3);
    draw_text_centered(128, "SPACE OR P TO RESUME", WHITE, 1);
    draw_text_centered(142, "R TO RESTART", WHITE, 1);
}

static void render_game_over(const GameState *game) {
    clear_screen(CITY_BG);
    draw_building_block(12, 18, 88, 224);
    draw_building_block(232, 18, 308, 224);
    draw_panel(42, 18, 277, 220);
    draw_text_centered(26, "GAME OVER", RED, 3);
    draw_text_centered(72, end_reason_label(game->end_reason), YELLOW, 2);
    draw_text_centered(94, "ELIMINATED BY", PANEL_EDGE, 1);
    draw_text_centered(108, end_reason_label(game->end_reason), ORANGE, 1);
    draw_text_centered(132, "FINAL SCORE", WHITE, 2);
    draw_int(138, 136, game->score, CYAN, 2);
    draw_text_centered(170, "THRU", WHITE, 1);
    draw_int(182, 136, game->throughput, GREEN, 2);
    draw_text_centered(170, "HIGH SCORE", WHITE, 1);
    draw_int(182, 196, game->best_score, MAGENTA, 2);
    draw_text_centered(214, "SPACE TO PLAY AGAIN", WHITE, 1);
}

static void render_scene(const GameState *game) {
    switch (game->scene) {
        case SCENE_TITLE:
            render_title_screen();
            break;

        case SCENE_PLAYING:
            render_playfield(game);
            break;

        case SCENE_PAUSED:
            render_playfield(game);
            render_paused_overlay();
            break;

        case SCENE_GAME_OVER:
            render_game_over(game);
            break;
    }
}

int main(void) {
    GameState game;
    init_game(&game);
    video_init();
    timer_init(TICK_COUNTS);
    render_scene(&game);

    while (1) {
        bool redraw = false;
        int key = ps2_get_make_code();

        while (key != -1) {
            if (handle_action(&game, action_from_scancode(key))) {
                redraw = true;
            }
            key = ps2_get_make_code();
        }

        if (timer_expired()) {
            if (game.scene == SCENE_PLAYING) {
                update_gameplay(&game);
                if ((game.elapsed_ticks % RENDER_DIVIDER) == 0 || game.scene != SCENE_PLAYING) {
                    redraw = true;
                }
            } else {
                redraw = true;
            }
        }

        if (redraw) {
            render_scene(&game);
        }
    }

    return 0;
}
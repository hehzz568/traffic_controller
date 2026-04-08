#ifndef TRAFFIC_GAME_H
#define TRAFFIC_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "address_map.h"

/* Aliases used by the original single-file version. */
#define TIMER1_BASE     TIMER_BASE
#define PIXEL_CTRL_BASE PIXEL_BUF_CTRL_BASE
#define PIXEL_BUF_BASE  FPGA_PIXEL_BUF_BASE
#define BACK_BUF_BASE   0x02000000

/* Screen constants */
#define SCREEN_W 320
#define SCREEN_H 240

/* Colors */
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

/* Hardware pointers */
extern volatile int *ps2_ptr;
extern volatile int *timer_ptr;
extern volatile int *hex30_ptr;
extern volatile int *hex54_ptr;
extern volatile int *pixel_ctrl_ptr;
extern volatile short *pixel_buffer;

/* Shared game state */
extern LightState light_state;
extern ControlMode mode;
extern Scene scene;
extern int phase_ticks;
extern uint32_t rng_state;
extern Car cars[MAX_CARS];
extern Pedestrian peds[MAX_PEDS];
extern int score;
extern int best_score;
extern int passed;
extern int wait_ticks_total;
extern int elapsed_ticks;
extern EndReason end_reason;
extern int crash_x;
extern int crash_y;
extern int queue_n;
extern int queue_s;
extern int queue_w;
extern int queue_e;
extern LightState next_green_state;
extern LightState resume_green_state;
extern int ped_waiting_horizontal;
extern int ped_waiting_vertical;
extern int ped_wait_ticks_horizontal;
extern int ped_wait_ticks_vertical;
extern int ped_groups_served;
extern bool ped_priority_horizontal;
extern RushAxis rush_axis;
extern int rush_ticks_left;
extern int bonus_score;
extern int flow_streak;

/* Video and IO */
void wait_for_vsync(void);
void present_frame(void);
void video_init(void);
void plot_pixel(int x, int y, short color);
void draw_box(int x1, int y1, int x2, int y2, short color);
void clear_screen(short color);
void timer_init(uint32_t period_counts);
bool timer_expired(void);
int ps2_get_make_code(void);

/* Text drawing */
int text_len(const char *s);
void draw_char(int x, int y, char ch, short color, int scale);
void draw_text(int x, int y, const char *text, short color, int scale);
void draw_text_centered(int y, const char *text, short color, int scale);
void draw_text_in_box(int x1, int x2, int y, const char *text, short color, int scale);
void format_int_text(int value, char *buf);
void draw_int(int x, int y, int value, short color, int scale);
void draw_int_centered(int y, int value, short color, int scale);
void draw_int_in_box(int x1, int x2, int y, int value, short color, int scale);
int text_width(const char *text, int scale);
void draw_text_right(int x_right, int y, const char *text, short color, int scale);
void draw_int_right(int x_right, int y, int value, short color, int scale);
void copy_text(char *dst, const char *src);
void append_text(char *dst, const char *src);
void format_status_count_text(const char *status, int count, char *buf);

/* Simulation */
uint32_t next_rand(void);
short random_car_color(void);
const char *rush_axis_label(void);
const char *rush_notice_label(void);
short rush_axis_color(void);
bool is_rush_dir(Direction dir);
void choose_new_rush_axis(void);
void update_rush_cycle(void);
bool is_ped_walk_state(LightState state);
bool is_vehicle_green_state(LightState state);
bool any_ped_request_pending(void);
LightState choose_pending_ped_state(void);
LightState choose_resume_green_state(void);
void clear_pedestrians(void);
bool ped_flow_allowed_horizontal(void);
bool ped_flow_allowed_vertical(void);
int count_active_pedestrians(bool horizontal);
void spawn_pedestrian(bool horizontal, int x, int y, int dx, int dy, short color);
void add_ped_waiters(bool horizontal, int count);
void maybe_queue_ped_request(void);
void release_waiting_pedestrians(bool horizontal, int count);
void start_ped_phase(LightState walk_state);
void maybe_spawn_pedestrians_for_current_state(void);
void update_pedestrians(void);
const char *ped_status_horizontal_label(void);
const char *ped_status_vertical_label(void);
short ped_status_horizontal_color(void);
short ped_status_vertical_color(void);
bool is_green_for_dir(Direction dir);
bool passed_stop_line(const Car *car);
int car_width(const Car *car);
int car_height(const Car *car);
bool rect_overlap(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2);
bool cars_overlap(const Car *a, const Car *b);
void reset_round(void);
int spawn_chance_percent(void);
void maybe_spawn_car(void);
void update_queue_lengths(void);
bool detect_crash(void);
int axis_for_dir(Direction dir);
bool car_hits_conflict_zone_at(const Car *car, int x, int y);
bool conflict_zone_blocked(const Car *car, int nx, int ny);
void update_score(void);
int wait_seconds_total(void);
int remaining_round_seconds(void);
int hex_digit_pattern(int digit);
void update_hex_timer(void);
bool blocked_by_leader(const Car *car, int nx, int ny);
const char *light_state_label(void);
const char *light_state_long_label(void);
int phase_countdown_ticks(void);
int phase_countdown_tenths(void);
void update_cars(void);
void update_lights_auto(void);
void update_light_transition(void);
void request_light_state(LightState target);
bool ped_area_clear(void);

/* Rendering */
void draw_light_vertical(int x, int y, short c_top, short c_mid, short c_bot);
void draw_light_horizontal(int x, int y, short c_left, short c_mid, short c_right);
void draw_intersection_base(void);
void draw_lights(void);
void draw_vehicle_sprite(const Car *car);
void draw_pedestrian_sprite(const Pedestrian *ped);
void draw_crosswalk_guides(void);
void draw_pedestrians(void);
void draw_waiting_pedestrians(void);
void draw_cars(void);
void draw_hud(void);
void redraw_all(void);
void draw_page_frame(short fill);
void draw_panel(int x1, int y1, int x2, int y2, short fill, short accent);
void draw_compact_button(int x1, int y1, int x2, int y2, short accent);
void draw_label_strip(int x1, int y1, int x2, int y2, short fill);
void draw_static_scene(SceneRenderer renderer);

/* Scenes */
void draw_title_scene(void);
void draw_title(void);
void draw_instructions_scene(void);
void draw_instructions(void);
void draw_paused_scene(void);
void draw_paused(void);
void draw_game_over_scene(void);
void draw_game_over(void);

#endif

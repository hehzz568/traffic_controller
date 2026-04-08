#include "traffic_game.h"

volatile int *ps2_ptr = (int *)PS2_BASE;
volatile int *timer_ptr = (int *)TIMER1_BASE;
volatile int *hex30_ptr = (int *)HEX3_HEX0_BASE;
volatile int *hex54_ptr = (int *)HEX5_HEX4_BASE;
volatile int *pixel_ctrl_ptr = (int *)PIXEL_CTRL_BASE;
volatile short *pixel_buffer = (short *)PIXEL_BUF_BASE;

LightState light_state = NS_GREEN;
ControlMode mode = AUTO_MODE;
Scene scene = SCENE_TITLE;
int phase_ticks = 0;
uint32_t rng_state = 0x2432026u;
Car cars[MAX_CARS];
Pedestrian peds[MAX_PEDS];
int score = 0;
int best_score = 0;
int passed = 0;
int wait_ticks_total = 0;
int elapsed_ticks = 0;
EndReason end_reason = END_TIME;
int crash_x = SCREEN_W / 2;
int crash_y = SCREEN_H / 2;
int queue_n = 0;
int queue_s = 0;
int queue_w = 0;
int queue_e = 0;
LightState next_green_state = NS_GREEN;
LightState resume_green_state = NS_GREEN;
int ped_waiting_horizontal = 0;
int ped_waiting_vertical = 0;
int ped_wait_ticks_horizontal = 0;
int ped_wait_ticks_vertical = 0;
int ped_groups_served = 0;
bool ped_priority_horizontal = true;
RushAxis rush_axis = RUSH_NS;
int rush_ticks_left = 0;
int bonus_score = 0;
int flow_streak = 0;

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

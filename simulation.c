#include "traffic_game.h"

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


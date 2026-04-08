#include "traffic_game.h"

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

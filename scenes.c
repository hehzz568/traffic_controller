#include "traffic_game.h"

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

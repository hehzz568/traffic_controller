#include "traffic_game.h"

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

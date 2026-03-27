# DE1-SoC Traffic Controller

Single-intersection VGA traffic game for the DE1-SoC / CPUlator.

## What The Program Does

The current version is a simplified single-lane intersection simulator:

- Cars spawn from the north, south, east, and west approaches.
- The player controls whether the junction runs in `AUTO` mode or `MANUAL` mode.
- In auto mode, the lights use queue pressure to decide when to yield to the other axis.
- In manual mode, the player can force north-south green or east-west green.
- Cars stop at red lights, queue behind cars in the same lane, and move through the intersection when allowed.
- The VGA display uses double buffering to avoid visible redraw flicker.
- Traffic demand ramps up over time, so later parts of the round are harder.

## Scenes

- `Title`: shows controls and the win/lose rule summary.
- `Playing`: main simulation.
- `Paused`: freezes the round and shows a small overlay.
- `Round Clear / Crash Out`: end screen after timeout or collision.

## Win And Lose Conditions

- `Crash Out`: any two cars overlap, which counts as a collision. This is a loss.
- `Round Clear`: the timer reaches the end of the round without a crash. This is a successful finish.

There is currently no other elimination rule besides collision.

## Score Formula

The score is intentionally simple and deterministic:

- `+25` for every car that successfully passes the stop line and clears the junction flow
- `-(wait_ticks_total / 40)` as a waiting penalty
- Score is clamped so it never goes below `0`

In other words:

```text
score = max(0, passed * 25 - wait_ticks_total / 40)
```

This means:

- Moving more cars through the junction raises the score.
- Letting queues sit at red lights for too long lowers the score.
- There is no hidden random score penalty anymore.

## HUD

During play the HUD shows:

- Current `SCORE`
- Total cars `PASS`ed through the intersection
- Accumulated `WAIT` time in seconds (summed across all waiting cars)
- Remaining `TIME`
- Current `BEST` score for this run of the program
- Queue counts for `N`, `S`, `W`, and `E`

## Controls

- `Space`: start, pause, resume, or retry after the round ends
- `P`: pause or resume during play
- `A`: toggle `AUTO` / `MANUAL`
- `1`: force north-south green in manual mode
- `2`: force east-west green in manual mode
- `R`: restart the current round
- `S`: return to the title screen

## Display Notes

- Front buffer uses the default pixel buffer at `0x08000000`
- Back buffer is stored in a global SDRAM array and swapped using the pixel controller at `0xFF203020`
- This avoids drawing directly on the visible frame and greatly reduces flicker on VGA

## Code Structure

Main areas inside `vga.c`:

- VGA primitives: `plot_pixel()`, `draw_box()`, text rendering helpers
- Video sync / double buffering: `video_init()`, `present_frame()`
- Game simulation: `maybe_spawn_car()`, `update_cars()`, `detect_crash()`, `update_lights_auto()`
- Scene rendering: `draw_title()`, `redraw_all()`, `draw_paused()`, `draw_game_over()`
- Main loop: timer tick update + PS/2 keyboard polling

## Current Limitations

- The game uses one lane per approach only
- There are no turning vehicles yet
- Collision detection is rectangle-based and intentionally simple
- The pixel font is custom and only supports the characters needed by the current UI

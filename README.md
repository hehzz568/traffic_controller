# Traffic Game Redesign

This project turns the original VGA traffic-light demo into a playable traffic-control game for the DE1-SoC board.

## Core Gameplay

- Vehicles spawn from all four directions with increasing difficulty over time.
- Each approach now has two inbound lanes: a straight lane and a turn lane.
- Turn-lane vehicles automatically perform right turns through the intersection.
- The player manually requests which axis gets the next green signal.
- Every signal change passes through yellow and all-red clearance states.
- Cars queue at red lights, follow the car ahead, and only enter the intersection when the conflict zone is safe.
- Traffic lights are drawn outside the driving lanes so they stay visible during play.
- The HUD tracks score, throughput, wait time, risk, queue sizes, current phase, and requested next phase.

## Controls

- `Space` or `Enter`: start game, resume from pause, or play again after game over
- `A`: request north-south green
- `D`: request east-west green
- `P`: pause or resume
- `R`: restart the round

## Objective

Keep traffic moving for the full round while avoiding elimination:

- Throughput increases score
- Waiting vehicles and long queues reduce score
- Sustained heavy congestion raises the risk meter and can trigger `GRIDLOCK`
- A neglected direction can trigger `STARVATION`
- Survive until the timer expires for a clean finish

## Demo Flow

1. Start at the title screen.
2. Press `Space` to begin.
3. Use `A` and `D` to control which direction gets the next green phase.
4. Watch queue lengths, risk, and which turn lane is backing up before switching.
5. Survive until time runs out or restart to improve the high score.

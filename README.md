# Vector Defense | Geometric Survival (Prime Edition)

A high-performance, vector-based tower defense game built from the ground up using **C++17** and **Raylib**. Protect the central core from evolving geometric threats through strategic node placement, progressive technology unlocks, and high-impact active abilities.

## 🕹️ Gameplay Features

* **Dynamic Wave System:** Challenges scale in complexity. Face swarms of enemies that evolve from simple triangles into complex decagons as you progress.
* **Progressive Node Technology:**
    * **Standard (Green Square):** Reliable high-velocity laser fire for consistent DPS.
    * **Cryo-Slow (Blue Hexagon):** Unlocks at **5 slots**. Freezes threats, reducing enemy speed significantly.
    * **Tesla-Tech (Gold Octagon):** Unlocks at **7 slots**. Chains high-voltage arcs between multiple enemies.
* **Active Ability: Red Pulse:** Discharge a high-damage shockwave from the core to clear swarms and repel heavy threats.
* **Persistent Hall of Fame:** Save your survivor data after a system failure. The local leaderboard tracks the top 10 scores across all sessions via `scores.txt`.
* **Strategic Armory:** Harvest data fragments from destroyed entities to expand node slots, overclock laser fire rates, and repair core integrity.
* **Experimental Power-ups:** EMP (Purple), Overdrive (Gold), and Nanobot Repair (Cyan).

## 🛠️ Technical Details

* **Engine:** Custom game loop built with Raylib.
* **Storage:** Local file I/O implementation for high score persistence.
* **Visuals:** Procedural polygon rendering, particle seeking logic, and thick-ring energy animations.
* **Optimization:** Static linking for a completely portable Windows executable.

## 🚀 Getting Started

1. Clone the repository.
2. Open the directory in VS Code.
3. Build using `Ctrl + Shift + B`.
4. Run `main.exe`.

## 🎮 Controls

* **Mouse Left Click:** Place Nodes / Collect Power-ups / Navigate UI.
* **[1] / [2] / [3]:** Toggle between Standard, Cryo, and Tesla nodes (once unlocked).
* **[SPACE] / UI Button:** Discharge Red Pulse shockwave.
* **[U] Key:** Access System Armory during Build Phases.
* **[Enter]:** Start waves / Initialize boot sequence.
* **Keyboard:** Type your name on the Game Over screen to save your score.
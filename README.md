# Vector Defense | Geometric Survival

A high-performance, polished vector-based tower defense game built from the ground up using **C++17** and **Raylib**. Protect the central core from evolving geometric threats through strategic node placement, progressive technology unlocks, and cinematic active abilities.

## 🕹️ Gameplay Features

* **Neon Bloom Engine:** Features an embedded GLSL shader that provides a high-end glow effect to all vector lines and particles without requiring external shader files.
* **Cinematic Pacing:** Professional game flow featuring "Wave Splash" intro titles and a robust State Management system (Gameplay, Paused, Shop, and Game Over).
* **Immersive Audio:** Integrated tactical sound system for immediate player feedback (Laser fire, Pulse booms, and UI interactions).
* **Tactical Node Placement:** Use the "Ghost Node" system to preview placement and ranges. Features real-time validation to prevent building in the core's heart.
* **Progressive Technology:**
    * **Standard (Square):** Efficient high-velocity laser fire.
    * **Cryo-Slow (Hexagon):** Unlocks at **5 slots**. Reduces enemy velocity significantly.
    * **Tesla-Tech (Octagon):** Unlocks at **7 slots**. Chains high-voltage gold arcs between multiple enemies.
* **Hall of Fame:** A persistent local leaderboard system that saves survivor data to `scores.txt`. Features in-game name entry with a tactile keyboard interface and descending score sorting.

## 🛠️ Requirements & Setup

1. **Audio Assets:** Ensure the following files are in your project root:
   * `blip.wav` (UI Interactions)
   * `boom.wav` (Pulse Discharge)
   * `shoot.wav` (Laser Fire)
2. **Build:** Use the provided `tasks.json` or compile via terminal using the `-static` flag and linking `raylib`, `opengl32`, `gdi32`, and `winmm`.

## 🎮 Controls

* **Mouse Left Click:** Place Nodes / Collect Power-ups / Interact with UI.
* **[1] / [2] / [3]:** Toggle between Standard, Cryo, and Tesla nodes (once tech is unlocked).
* **[ESC] / [P]:** Toggle System Pause.
* **[SPACE] / UI Button:** Discharge Red Pulse shockwave (requires charges).
* **[U] Key:** Access System Armory during Build Phases.
* **[Enter]:** Start waves / Initialize boot sequence.
* **Typing:** Input your name on the System Failure screen to sync data to the Hall of Fame.
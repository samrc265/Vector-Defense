# Vector Defense | Geometric Survival

A high-performance, vector-based tower defense game built from the ground up using **C++17** and **Raylib**. Protect the central core from evolving geometric threats through strategic node placement, system upgrades, and experimental power-ups.

## 🕹️ Gameplay Features

* **Dynamic Wave System:** Challenges scale in complexity. Face intense swarms that evolve and gain speed as you progress.
* **Epic Boss Encounters:** Every 10 waves, a "Prime Polygon" boss anomaly enters the field with massive integrity and adaptive behaviors.
* **Strategic Armory:** Spend harvested data fragments to expand node slots, overclock laser recharge rates, and repair core integrity.
* **Experimental Power-ups:**
    * **EMP (Purple):** Freezes all enemy movement on screen.
    * **Overdrive (Gold):** Forces all defense nodes into a turbo-firing state.
    * **Nanobots (Cyan):** Direct core heart repair via particle absorption.
* **Tactical Splitting:** High-tier enemies (Hexagons and above) split into two faster sections upon destruction, demanding rapid target switching.

## 🛠️ Technical Details

* **Engine:** Custom-built game loop using Raylib.
* **Visuals:** Hand-coded particle systems, procedural geometric health rendering, and dynamic camera shake/flash effects.
* **Optimization:** Efficient memory management and spatial calculations for high entity counts.

## 🚀 Getting Started

### Prerequisites

* C++ Compiler (MinGW-w64 recommended)
* Raylib library installed

### Building the Project

1. Clone the repository.
2. Open the directory in VS Code.
3. Use the provided `tasks.json` to build: `Ctrl` + `Shift` + `B`.
4. Run `main.exe`.

## 🎮 Controls

* **Mouse Left Click:** Place Defense Nodes / Collect Power-up Cores.
* **[U] Key:** Open System Armory (Build Phase only).
* **[Enter]:** Initialize Boot Sequence / Start Waves.
* **[Backspace] / [Esc]:** Navigation.
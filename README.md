# Vector Defense | Geometric Survival (Prime Edition)

A high-performance, vector-based tower defense game built from the ground up using **C++17** and **Raylib**. Protect the central core from evolving geometric threats through strategic node placement, system upgrades, and experimental power-ups.

## 🕹️ Gameplay Features

* **Dynamic Wave System:** Challenges scale in complexity. Face swarms of enemies that gain speed and health as the wave depth increases.
* **Dual Node Defense:**
    * **Standard (Green):** High-velocity laser fire designed for maximum DPS.
    * **Cryo-Slow (Blue):** Specialized hexagon nodes that freeze threats, reducing enemy movement speed by 60%.
* **Active Ability: Red Pulse:** Discharge a high-frequency shockwave from the core to damage and repel all nearby threats. Pulse charges can be purchased in the Armory.
* **Epic Boss Encounters:** Every 10 waves, a "Prime Polygon" boss anomaly enters the field with massive integrity and adaptive behaviors.
* **Strategic Armory:** Spend harvested data fragments to expand node slots, permanently overclock laser fire rates, and repair core integrity.
* **Experimental Power-ups:**
    * **EMP (Purple):** Total movement lock-down of all hostiles.
    * **Overdrive (Gold):** Forces all defense nodes into a high-energy sparking turbo-fire state.
    * **Nanobots (Cyan):** Direct core heart repair via seeking particle absorption.

## 🛠️ Technical Details

* **Engine:** Custom-built game loop using Raylib.
* **Visuals:** Thick-ring procedural animations, particle seeking logic, and dynamic camera displacement.
* **Optimization:** Static linking for a standalone, asset-free executable.

## 🚀 Getting Started

1. Clone the repository.
2. Open the directory in VS Code.
3. Use the provided `tasks.json` to build: `Ctrl` + `Shift` + `B`.
4. Run `main.exe`.

## 🎮 Controls

* **Mouse Left Click:** Place Nodes / Collect Power-ups / Activate UI Buttons.
* **[1] / [2]:** Toggle between Standard and Cryo-Slow Node types.
* **[SPACE] / UI Button:** Discharge Red Pulse (requires charges).
* **[U] Key:** Open System Armory (Build Phase only).
* **[Enter]:** Initialize Boot Sequence / Start Waves.
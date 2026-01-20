#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>

// --- Visual & Logic Constants ---
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const float CORE_RADIUS = 50.0f;
const float EXCLUSION_RADIUS = CORE_RADIUS + 35.0f; 
const int UI_HEADER_HEIGHT = 60;
const int UI_FOOTER_HEIGHT = 85;

// --- Color Palette ---
const Color V_CYAN      = { 0, 255, 255, 255 };
const Color V_LIME      = { 0, 255, 100, 255 }; 
const Color V_RED       = { 255, 60, 60, 255 };
const Color V_GOLD      = { 255, 215, 0, 255 };
const Color V_WHITE     = { 245, 245, 245, 255 };
const Color V_SKYBLUE   = { 100, 200, 255, 255 };
const Color V_PURPLE    = { 200, 100, 255, 255 };
const Color V_DARKGRAY  = { 30, 30, 35, 255 };
const Color V_BLACK     = { 10, 10, 12, 255 };

enum GameScreen { START_MENU, GUIDE, GAMEPLAY, UPGRADE_MENU, GAME_OVER, LEADERBOARD };
enum PowerType { PWR_EMP, PWR_OVERDRIVE, PWR_HEAL };
enum TowerType { TWR_STANDARD, TWR_CRYO, TWR_TESLA };

struct Enemy {
    Vector2 position;
    float speed;
    int sides;
    float health;
    float maxHealth;
    bool active;
    float radius; 
    float slowTimer; 
};

struct Tower {
    Vector2 position;
    float shootTimer;
    TowerType type;
};

struct PowerUp {
    Vector2 position;
    PowerType type;
    float timer; 
    bool active;
    float rotation; 
};

struct Laser {
    Vector2 start; Vector2 end; float lifetime; Color col;
};

struct Particle {
    Vector2 pos; Vector2 vel; Color col;
    float life; float maxLife; bool seekingCore; 
};

struct MenuShape {
    Vector2 pos; float speed; float rotation; float rotSpeed; int sides; float size;
};

struct Notification {
    std::string text; float timer; Color col;
};

struct ScoreEntry {
    std::string name;
    int score;
};

// --- Global Score Management ---
std::vector<ScoreEntry> highScores;

void LoadHighScores() {
    highScores.clear();
    std::ifstream file("scores.txt");
    std::string name;
    int score;
    while (file >> name >> score) {
        highScores.push_back({name, score});
    }
    file.close();
    // Sort descending
    std::sort(highScores.begin(), highScores.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
        return a.score > b.score;
    });
}

void SaveScore(std::string name, int score) {
    if (name.empty()) name = "ANONYMOUS";
    std::ofstream file("scores.txt", std::ios::app);
    file << name << " " << score << "\n";
    file.close();
    LoadHighScores();
}

// --- Utilities ---
float GetDistance(Vector2 v1, Vector2 v2) {
    return sqrtf(powf(v2.x - v1.x, 2) + powf(v2.y - v1.y, 2));
}

bool DrawCustomButton(Rectangle bounds, const char* text, Color baseCol, int fontSize = 24) {
    Vector2 mouse = GetMousePosition();
    bool hovering = CheckCollisionPointRec(mouse, bounds);
    DrawRectangleRec(bounds, hovering ? ColorAlpha(baseCol, 0.35f) : ColorAlpha(V_DARKGRAY, 0.6f));
    DrawRectangleLinesEx(bounds, 2, hovering ? baseCol : ColorAlpha(V_WHITE, 0.2f));
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, bounds.x + (bounds.width/2 - textWidth/2), bounds.y + (bounds.height/2 - fontSize/2), fontSize, hovering ? V_WHITE : ColorAlpha(V_WHITE, 0.7f));
    return hovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void DrawHealthBody(Vector2 pos, int sides, float radius, float healthRatio, Color col) {
    if (healthRatio > 0.5f) DrawPoly(pos, sides, radius, 0, ColorAlpha(col, healthRatio * 0.4f));
    DrawPolyLinesEx(pos, sides, radius, 0, 2.5f, ColorAlpha(col, healthRatio + 0.2f));
}

void SpawnParticleBurst(std::vector<Particle>& particles, Vector2 pos, Color col, int count, float speed) {
    for (int i = 0; i < count; i++) {
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float s = (float)GetRandomValue(50, 200) * 0.01f * speed;
        particles.push_back({ pos, {cosf(angle) * s, sinf(angle) * s}, col, 1.0f, 1.0f, false });
    }
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vector-Defense | Prime Edition");
    SetTargetFPS(60);
    LoadHighScores();

    GameScreen currentScreen = START_MENU;
    Vector2 corePos = { (float)SCREEN_WIDTH / 2, (float)SCREEN_HEIGHT / 2 };
    
    // Persistent Stats
    int coreHealth = 20, maxCoreHealth = 20, score = 0, currency = 0;
    int currentWave = 0, enemiesToSpawn = 0;
    float spawnTimer = 0.0f;
    bool waveActive = false, bossInQueue = false; 
    
    // Upgrades & Progression
    int maxTowers = 3;
    float towerFireRate = 0.8f, towerRange = 230.0f; 
    int pulseWaveCharges = 0;
    TowerType currentSelection = TWR_STANDARD;
    bool cryoUnlocked = false, teslaUnlocked = false;
    bool pendingCryoNotify = false, pendingTeslaNotify = false;

    // Name Entry for Leaderboard
    char playerName[13] = "\0";
    int letterCount = 0;
    bool scoreSaved = false;

    // Animation States
    float empTimer = 0.0f, overdriveTimer = 0.0f, empWaveRadius = 0.0f, pulseVisualRadius = 0.0f; 
    float shakeIntensity = 0.0f, damageFlashTimer = 0.0f;

    Camera2D camera = { 0 }; camera.zoom = 1.0f;

    std::vector<Enemy> enemies;
    std::vector<Tower> towers;
    std::vector<Laser> lasers;
    std::vector<PowerUp> powerups;
    std::vector<Particle> particles;
    std::vector<MenuShape> menuShapes;
    std::vector<Notification> notifications;

    for (int i = 0; i < 20; i++) {
        menuShapes.push_back({
            {(float)GetRandomValue(0, SCREEN_WIDTH), (float)GetRandomValue(0, SCREEN_HEIGHT)},
            (float)GetRandomValue(40, 100) / 100.0f,
            (float)GetRandomValue(0, 360),
            (float)GetRandomValue(-20, 20) / 10.0f,
            GetRandomValue(3, 8),
            (float)GetRandomValue(30, 120)
        });
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mousePos = GetMousePosition();

        bool mouseInHeader = mousePos.y < UI_HEADER_HEIGHT;
        bool mouseInFooter = (!waveActive && enemies.empty()) && (mousePos.y > SCREEN_HEIGHT - UI_FOOTER_HEIGHT);
        Rectangle pulseRect = { 25, (float)SCREEN_HEIGHT - 120, 230, 50 };
        bool overPulseButton = (waveActive && pulseWaveCharges > 0 && CheckCollisionPointRec(mousePos, pulseRect));
        bool mouseOnUi = mouseInHeader || mouseInFooter || overPulseButton || (currentScreen == UPGRADE_MENU) || (currentScreen == GAME_OVER);

        if (shakeIntensity > 0) {
            camera.offset.x = GetRandomValue(-shakeIntensity, shakeIntensity);
            camera.offset.y = GetRandomValue(-shakeIntensity, shakeIntensity);
            shakeIntensity -= 15.0f * dt;
        } else camera.offset = {0,0};

        if (damageFlashTimer > 0) damageFlashTimer -= dt;

        switch (currentScreen) {
            case START_MENU: {
                for (auto &ms : menuShapes) {
                    ms.pos.y -= ms.speed; ms.rotation += ms.rotSpeed;
                    if (ms.pos.y < -ms.size) { ms.pos.y = SCREEN_HEIGHT + ms.size; ms.pos.x = GetRandomValue(0, SCREEN_WIDTH); }
                }
                if (IsKeyPressed(KEY_ENTER)) currentScreen = GAMEPLAY; 
            } break;

            case GUIDE: if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) currentScreen = START_MENU; break;
            case LEADERBOARD: if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) currentScreen = START_MENU; break;
            
            case UPGRADE_MENU: {
                if (IsKeyPressed(KEY_U) || IsKeyPressed(KEY_ENTER)) {
                    currentScreen = GAMEPLAY;
                    if (pendingCryoNotify) {
                        notifications.push_back({"CRYO-TECH UNLOCKED!", 5.0f, V_SKYBLUE});
                        notifications.push_back({"PRESS [2] TO SELECT", 5.0f, V_WHITE});
                        pendingCryoNotify = false;
                    }
                    if (pendingTeslaNotify) {
                        notifications.push_back({"TESLA-TECH UNLOCKED!", 5.0f, V_GOLD});
                        notifications.push_back({"PRESS [3] TO SELECT", 5.0f, V_WHITE});
                        pendingTeslaNotify = false;
                    }
                }
            } break;

            case GAMEPLAY: {
                if (IsKeyPressed(KEY_ONE)) currentSelection = TWR_STANDARD;
                if (IsKeyPressed(KEY_TWO) && cryoUnlocked) currentSelection = TWR_CRYO;
                if (IsKeyPressed(KEY_THREE) && teslaUnlocked) currentSelection = TWR_TESLA;

                bool pulseTriggered = (waveActive && pulseWaveCharges > 0 && DrawCustomButton(pulseRect, TextFormat("ACTIVATE PULSE [%d]", pulseWaveCharges), V_SKYBLUE, 18));

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !mouseOnUi) {
                    bool pickedUp = false;
                    for(auto &p : powerups) {
                        if(p.active && GetDistance(mousePos, p.position) < 45) {
                            if(p.type == PWR_EMP) { empTimer = 4.5f; empWaveRadius = 10.0f; notifications.push_back({"SYSTEM EMP ACTIVATED", 2.0f, V_PURPLE}); }
                            else if(p.type == PWR_OVERDRIVE) { overdriveTimer = 7.0f; notifications.push_back({"LASER OVERDRIVE ONLINE", 2.0f, V_GOLD}); }
                            else if(p.type == PWR_HEAL) { 
                                coreHealth = std::min(coreHealth + 3, maxCoreHealth); 
                                notifications.push_back({"INTEGRITY RESTORED", 2.0f, V_CYAN}); 
                                for(int i=0; i<80; i++) particles.push_back({{p.position.x + (float)GetRandomValue(-20,20), p.position.y + (float)GetRandomValue(-20,20)}, {0,0}, V_CYAN, 1.5f, 1.5f, true});
                            }
                            p.active = false; pickedUp = true; break;
                        }
                    }
                    if (!pickedUp && towers.size() < (size_t)maxTowers && GetDistance(mousePos, corePos) > EXCLUSION_RADIUS) {
                        towers.push_back({ mousePos, 0.0f, currentSelection });
                    }
                }

                if ((IsKeyPressed(KEY_SPACE) || pulseTriggered) && pulseWaveCharges > 0 && waveActive) {
                    pulseWaveCharges--; shakeIntensity = 25.0f; pulseVisualRadius = 10.0f;
                    notifications.push_back({"PULSE DISCHARGED", 2.5f, V_RED});
                    for (auto &e : enemies) {
                        float dist = GetDistance(corePos, e.position);
                        if (dist < 450.0f) { 
                            e.health -= (500.0f - dist) / 5.0f; 
                            if(e.health <= 0) e.active = false; 
                        }
                    }
                }

                if (waveActive) {
                    spawnTimer += dt;
                    float spawnRate = std::max(0.15f, 1.25f - (currentWave * 0.06f));
                    if (enemiesToSpawn > 0 && spawnTimer > spawnRate) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy e; e.position = { corePos.x + cosf(angle) * 850.0f, corePos.y + sinf(angle) * 850.0f };
                        e.radius = 22.0f; e.sides = GetRandomValue(3, std::min(10, 3 + (currentWave / 2)));
                        float speedMult = std::min(1.6f, 1.0f + (currentWave * 0.035f));
                        e.speed = (180.0f - ((float)e.sides * 8.0f)) * speedMult; 
                        e.maxHealth = (float)e.sides * 1.2f; 
                        e.health = e.maxHealth; e.active = true; e.slowTimer = 0; enemies.push_back(e); enemiesToSpawn--; spawnTimer = 0;
                    } 
                    else if (enemiesToSpawn <= 0 && bossInQueue && spawnTimer > 1.8f) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy boss; boss.position = { corePos.x+cosf(angle)*850.0f, corePos.y+sinf(angle)*850.0f };
                        boss.sides = 24; boss.radius = 90.0f; boss.maxHealth = 180.0f + ((float)currentWave * 25.0f);
                        boss.health = boss.maxHealth; boss.speed = 25.0f; boss.active = true; boss.slowTimer = 0;
                        enemies.push_back(boss); bossInQueue = false; spawnTimer = 0; notifications.push_back({"BOSS DETECTED", 3.0f, V_RED});
                    }
                }

                std::vector<Enemy> newSplits;
                for (auto it = enemies.begin(); it != enemies.end();) {
                    if (empTimer <= 0) { 
                        float moveSpeed = it->speed; if (it->slowTimer > 0) { moveSpeed *= 0.4f; it->slowTimer -= dt; }
                        float angle = atan2f(corePos.y - it->position.y, corePos.x - it->position.x);
                        it->position.x += cosf(angle) * moveSpeed * dt; it->position.y += sinf(angle) * moveSpeed * dt;
                    }
                    if (GetDistance(it->position, corePos) < CORE_RADIUS) {
                        if (it->sides == 24) coreHealth -= 5; else coreHealth--;
                        shakeIntensity = 18.0f; damageFlashTimer = 0.18f; it = enemies.erase(it);
                    } else if (!it->active) {
                        currency += (it->sides * 14) + 20; score += (int)(it->maxHealth * 100);
                        SpawnParticleBurst(particles, it->position, V_WHITE, 12, 2.0f);
                        if (it->sides >= 6 && it->radius > 20.0f) { 
                            for(int s=0; s<2; s++) {
                                Enemy mini; mini.position = it->position; mini.sides = 3; 
                                mini.radius = 16.0f; mini.maxHealth = 5; mini.health = 5;
                                mini.speed = 180.0f; mini.active = true; mini.slowTimer = 0;
                                newSplits.push_back(mini);
                            }
                        }
                        if(GetRandomValue(1, 100) <= 20) powerups.push_back({it->position, (PowerType)GetRandomValue(0, 2), 10.0f, true, 0.0f});
                        it = enemies.erase(it);
                    } else ++it;
                }
                for(auto &ns : newSplits) enemies.push_back(ns);

                if (waveActive && enemiesToSpawn <= 0 && !bossInQueue && enemies.empty()) { 
                    waveActive = false; towers.clear(); notifications.push_back({"WAVE CLEAR", 2.0f, V_SKYBLUE});
                }

                float currentRate = (overdriveTimer > 0) ? 0.05f : towerFireRate;
                for (auto &t : towers) {
                    t.shootTimer += dt;
                    if (overdriveTimer > 0 && GetRandomValue(0, 4) == 0) {
                        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
                        particles.push_back({{t.position.x + cosf(a)*18, t.position.y + sinf(a)*18}, {0, -120}, V_GOLD, 0.4f, 0.4f, false});
                    }
                    float effectiveRate = currentRate;
                    if (t.type == TWR_CRYO) effectiveRate *= 1.5f;
                    if (t.type == TWR_TESLA) effectiveRate *= 1.5f; 

                    if (t.shootTimer >= effectiveRate) {
                        Enemy* target = nullptr; float minDist = towerRange;
                        for (auto& e : enemies) {
                            float d = GetDistance(t.position, e.position);
                            if (d < minDist) { minDist = d; target = &e; }
                        }
                        if (target) {
                            Color laserCol = V_WHITE;
                            if (t.type == TWR_CRYO) { target->health -= 0.5f; target->slowTimer = 1.5f; laserCol = V_SKYBLUE; }
                            else if (t.type == TWR_TESLA) {
                                target->health -= 0.8f; laserCol = V_GOLD;
                                Enemy* secondary = nullptr; float secMinDist = 200.0f;
                                for (auto& e2 : enemies) {
                                    if (&e2 == target) continue;
                                    float d2 = GetDistance(target->position, e2.position);
                                    if (d2 < secMinDist) { secMinDist = d2; secondary = &e2; }
                                }
                                if (secondary) {
                                    secondary->health -= 0.6f; if (secondary->health <= 0) secondary->active = false;
                                    lasers.push_back({ target->position, secondary->position, 0.12f, V_GOLD });
                                }
                            }
                            else target->health -= 1.0f;
                            if (target->health <= 0) target->active = false;
                            lasers.push_back({ t.position, target->position, 0.07f, laserCol }); t.shootTimer = 0;
                        }
                    }
                }

                for (auto it = lasers.begin(); it != lasers.end();) { it->lifetime -= dt; if (it->lifetime <= 0) it = lasers.erase(it); else ++it; }
                
                for (auto it = powerups.begin(); it != powerups.end();) {
                    if (waveActive) {
                        it->timer -= dt; 
                    }
                    it->rotation += 120.0f * dt;
                    if (it->timer <= 0 || !it->active) {
                        it = powerups.erase(it);
                    } else {
                        ++it;
                    }
                }

                for (auto it = particles.begin(); it != particles.end();) {
                    it->life -= dt;
                    if (it->seekingCore) {
                        float a = atan2f(corePos.y - it->pos.y, corePos.x - it->pos.x);
                        it->pos.x += cosf(a) * 600.0f * dt; it->pos.y += sinf(a) * 600.0f * dt;
                        if (GetDistance(it->pos, corePos) < 15.0f) it->life = 0;
                    } else { it->pos.x += it->vel.x * dt; it->pos.y += it->vel.y * dt; }
                    if (it->life <= 0) it = particles.erase(it); else ++it;
                }
                for (auto it = notifications.begin(); it != notifications.end();) { it->timer -= dt; if (it->timer <= 0) it = notifications.erase(it); else ++it; }
                if (empWaveRadius > 0) { empWaveRadius += 1600.0f * dt; if (empWaveRadius > 2500.0f) empWaveRadius = 0; }
                if (pulseVisualRadius > 0) { pulseVisualRadius += 2200.0f * dt; if (pulseVisualRadius > 1500.0f) pulseVisualRadius = 0; }
                
                if (empTimer > 0) {
                    empTimer -= dt;
                }
                if (overdriveTimer > 0) {
                    overdriveTimer -= dt;
                }

                if (coreHealth <= 0) { currentScreen = GAME_OVER; scoreSaved = false; playerName[0] = '\0'; letterCount = 0; }
                if (IsKeyPressed(KEY_U) && !waveActive) currentScreen = UPGRADE_MENU;
            } break;

            case GAME_OVER: {
                // Name entry logic
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 125) && (letterCount < 12)) {
                        playerName[letterCount] = (char)key;
                        playerName[letterCount+1] = '\0';
                        letterCount++;
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    letterCount--;
                    if (letterCount < 0) letterCount = 0;
                    playerName[letterCount] = '\0';
                }
            } break;
        }

        BeginDrawing();
            ClearBackground(V_BLACK);
            BeginMode2D(camera);
                for(int i = -100; i < SCREEN_WIDTH + 100; i += 64) DrawLine(i, -100, i, SCREEN_HEIGHT + 100, {30, 30, 35, 255});
                for(int i = -100; i < SCREEN_HEIGHT + 100; i += 64) DrawLine(-100, i, SCREEN_WIDTH + 100, i, {30, 30, 35, 255});
                if (currentScreen == START_MENU) {
                    for (const auto& ms : menuShapes) DrawPolyLinesEx(ms.pos, ms.sides, ms.size, ms.rotation, 1.5f, ColorAlpha(V_DARKGRAY, 0.4f));
                    DrawText("VECTOR DEFENSE", SCREEN_WIDTH/2 - MeasureText("VECTOR DEFENSE", 60)/2, 220, 60, V_CYAN);
                    DrawRectangle(SCREEN_WIDTH/2 - 120, 295, 240, 2, V_SKYBLUE);
                }
                if (currentScreen == GAMEPLAY || currentScreen == UPGRADE_MENU || currentScreen == START_MENU) {
                    for(const auto& p : particles) DrawCircle(p.pos.x, p.pos.y, 2, p.col);
                    if (empWaveRadius > 0) DrawCircleLines(corePos.x, corePos.y, empWaveRadius, ColorAlpha(V_PURPLE, 1.0f - (empWaveRadius/2500.0f)));
                    if (pulseVisualRadius > 0) DrawRing(corePos, pulseVisualRadius - 15.0f, pulseVisualRadius, 0, 360, 60, ColorAlpha(V_RED, 1.0f - (pulseVisualRadius/1500.0f)));
                    for (const auto& l : lasers) DrawLineEx(l.start, l.end, 3.0f, l.col);
                    for(const auto& p : powerups) DrawPolyLinesEx({p.position.x, p.position.y + sinf(GetTime()*5)*5}, 4, 18, p.rotation, 2, (p.type == PWR_EMP ? V_PURPLE : (p.type == PWR_OVERDRIVE ? V_GOLD : V_CYAN)));
                    if (currentScreen != START_MENU) {
                        DrawCircleLines(corePos.x, corePos.y, EXCLUSION_RADIUS, ColorAlpha(V_RED, 0.3f));
                        DrawCircleLines(corePos.x, corePos.y, CORE_RADIUS, V_CYAN);
                        DrawCircleLines(corePos.x, corePos.y, CORE_RADIUS - 8, V_SKYBLUE);
                        DrawCircle(corePos.x, corePos.y, 4, V_WHITE);
                    }
                    if (currentScreen == GAMEPLAY && !mouseOnUi && towers.size() < (size_t)maxTowers) {
                        bool canPlace = GetDistance(mousePos, corePos) > EXCLUSION_RADIUS;
                        DrawCircleLines(mousePos.x, mousePos.y, towerRange, ColorAlpha(V_WHITE, 0.3f));
                        DrawHealthBody(mousePos, (currentSelection == TWR_STANDARD ? 4 : (currentSelection == TWR_CRYO ? 6 : 8)), 18, 1.0f, ColorAlpha(canPlace ? (currentSelection == TWR_STANDARD ? V_LIME : (currentSelection == TWR_CRYO ? V_SKYBLUE : V_GOLD)) : V_RED, 0.5f));
                    }
                    for (const auto& t : towers) {
                        DrawCircleLines(t.position.x, t.position.y, towerRange, ColorAlpha(V_WHITE, 0.1f));
                        DrawHealthBody(t.position, (t.type == TWR_STANDARD ? 4 : (t.type == TWR_CRYO ? 6 : 8)), 18, 1.0f, (t.type == TWR_STANDARD ? V_LIME : (t.type == TWR_CRYO ? V_SKYBLUE : V_GOLD)));
                    }
                    for (const auto& e : enemies) DrawHealthBody(e.position, e.sides, e.radius, e.health/e.maxHealth, e.slowTimer > 0 ? V_SKYBLUE : V_RED);
                }
            EndMode2D();

            if (damageFlashTimer > 0) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_RED, damageFlashTimer * 1.5f));

            if (currentScreen == START_MENU) {
                if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 150, 360, 300, 65 }, "BOOT SEQUENCE", V_LIME)) { currentScreen = GAMEPLAY; waveActive = false; }
                if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 150, 440, 300, 65 }, "LEADERBOARD", V_GOLD)) currentScreen = LEADERBOARD;
                if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 150, 520, 300, 65 }, "SYSTEM GUIDE", V_WHITE)) currentScreen = GUIDE;
            } else if (currentScreen == GUIDE) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText("SYSTEM OPERATIONAL GUIDE", 60, 60, 35, V_SKYBLUE);
                int x1 = 70, x2 = 650, y = 140;
                DrawText("THREAT LOG", x1, y, 22, V_RED); DrawText("- Splitting: Complex shapes split into fragments.", x1, y+35, 18, V_WHITE); DrawText("- BOSS LOG: Heavy Primes emerge every 10 waves.", x1, y+60, 18, V_GOLD);
                DrawText("DEFENSE LOG", x1, y+130, 22, V_LIME); DrawText("- [1] Standard: Green squares. Normal DPS.", x1, y+165, 18, V_WHITE); DrawText("- [2] Cryo-Slow: Blue hexagons. Freezes threats.", x1, y+190, 18, V_WHITE); DrawText("- [3] Tesla: Gold Octagon. Chain lightning.", x1, y+215, 18, V_WHITE); DrawText("- Range Rings: Faint white circles show coverage.", x1, y+240, 18, V_WHITE);
                DrawText("POWER-UPS", x2, y, 22, V_GOLD); DrawText("- [EMP] Purple: Total movement lock-down.", x2, y+35, 18, V_PURPLE); DrawText("- [OVERDRIVE] Gold: Maximum fire-rate sparks.", x2, y+60, 18, V_GOLD); DrawText("- [NANOBOTS] Cyan: Core absorption repair.", x2, y+85, 18, V_SKYBLUE);
                DrawText("SYSTEM CYCLE", x2, y+155, 22, V_CYAN); DrawText("- [SPACE/Button]: Discharge Red Pulse charges.", x2, y+190, 18, V_WHITE); DrawText("- Armory [U]: Upgrade slots and laser fire speed.", x2, y+215, 18, V_WHITE); DrawText("- Time: Power-up timers freeze during build phase.", x2, y+240, 18, V_GOLD);
                if (DrawCustomButton({ (float)SCREEN_WIDTH/2-100, 620, 200, 50 }, "< RETURN", V_WHITE)) currentScreen = START_MENU;
            } else if (currentScreen == LEADERBOARD) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText("SYSTEM HALL OF FAME", SCREEN_WIDTH/2 - MeasureText("SYSTEM HALL OF FAME", 35)/2, 60, 35, V_GOLD);
                int startY = 140;
                for(int i=0; i<std::min(10, (int)highScores.size()); i++) {
                    DrawText(TextFormat("%d. %s", i+1, highScores[i].name.c_str()), SCREEN_WIDTH/2 - 200, startY + (i*40), 22, V_WHITE);
                    DrawText(TextFormat("%d", highScores[i].score), SCREEN_WIDTH/2 + 150, startY + (i*40), 22, V_SKYBLUE);
                }
                if (DrawCustomButton({ (float)SCREEN_WIDTH/2-100, 620, 200, 50 }, "< RETURN", V_WHITE)) currentScreen = START_MENU;
            } else if (currentScreen == GAMEPLAY || currentScreen == UPGRADE_MENU) {
                DrawRectangle(0, 0, SCREEN_WIDTH, UI_HEADER_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText(TextFormat("INTEGRITY: %d", coreHealth), 25, 20, 22, coreHealth < 5 ? V_RED : V_WHITE);
                DrawText(TextFormat("FRAGMENTS: %d", currency), 220, 20, 22, V_GOLD);
                DrawText(TextFormat("NODES: %d/%d", (int)towers.size(), maxTowers), 420, 20, 22, V_LIME);
                DrawText(TextFormat("WAVE: %d", currentWave), 580, 20, 22, V_SKYBLUE);
                DrawText(TextFormat("PULSE: %d", pulseWaveCharges), 720, 20, 22, V_CYAN);
                std::string mStr = (currentSelection == TWR_CRYO ? "CRYO" : (currentSelection == TWR_TESLA ? "TESLA" : "STANDARD"));
                Color mCol = (currentSelection == TWR_CRYO ? V_SKYBLUE : (currentSelection == TWR_TESLA ? V_GOLD : V_LIME));
                DrawText(TextFormat("ACTIVE: %s", mStr.c_str()), SCREEN_WIDTH - 250, 20, 20, mCol);
                if (waveActive) {
                    if (pulseWaveCharges > 0) DrawCustomButton(pulseRect, TextFormat("ACTIVATE PULSE [%d]", pulseWaveCharges), V_SKYBLUE, 18);
                    DrawText(TextFormat("THREATS: %d", (int)enemies.size() + enemiesToSpawn + (bossInQueue?1:0)), 25, SCREEN_HEIGHT - 35, 20, V_SKYBLUE);
                }
                for (int i = 0; i < (int)notifications.size(); i++) {
                    float alpha = notifications[i].timer / 2.0f;
                    DrawText(notifications[i].text.c_str(), (float)SCREEN_WIDTH/2 - (float)MeasureText(notifications[i].text.c_str(), 30)/2.0f, 110.0f + ((float)i * 45.0f), 30, ColorAlpha(notifications[i].col, alpha));
                }
                if (currentScreen == GAMEPLAY && !waveActive && enemies.empty()) {
                    DrawRectangle(0, SCREEN_HEIGHT - UI_FOOTER_HEIGHT, SCREEN_WIDTH, UI_FOOTER_HEIGHT, ColorAlpha(V_BLACK, 0.85f));
                    DrawText("SYSTEM IDLE // BUILD PHASE", 40, SCREEN_HEIGHT - 55, 20, V_SKYBLUE);
                    std::string p = "[1] STANDARD"; if(cryoUnlocked) p += " | [2] CRYO"; if(teslaUnlocked) p += " | [3] TESLA";
                    DrawText(p.c_str(), 40, SCREEN_HEIGHT - 75, 18, V_DARKGRAY);
                    if (DrawCustomButton({ (float)SCREEN_WIDTH - 550, (float)SCREEN_HEIGHT - 72, 250, 60 }, "OPEN ARMORY [U]", V_GOLD)) currentScreen = UPGRADE_MENU;
                    if (DrawCustomButton({ (float)SCREEN_WIDTH - 280, (float)SCREEN_HEIGHT - 72, 250, 60 }, "START WAVE", V_LIME)) {
                        currentWave++; waveActive = true; enemiesToSpawn = 7 + (currentWave * 5); if (currentWave % 10 == 0) bossInQueue = true;
                    }
                }
                if (currentScreen == UPGRADE_MENU) {
                    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.9f));
                    DrawText("SYSTEM ARMORY", SCREEN_WIDTH/2 - 120, 60, 35, V_SKYBLUE);
                    DrawText(TextFormat("AVAILABLE DATA: %d", currency), SCREEN_WIDTH/2 - MeasureText(TextFormat("AVAILABLE DATA: %d", currency), 24)/2, 120, 24, V_GOLD);
                    int sC = 400 + (maxTowers - 3) * 350; int fC = 600 + (int)((0.8f - towerFireRate) * 10000); 
                    if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 200, 180, 400, 65 }, TextFormat("BUY NODE SLOT (%d)", sC), V_LIME)) { 
                        if(currency >= sC){ currency -= sC; maxTowers++; 
                            if(maxTowers==5&&!cryoUnlocked){ cryoUnlocked=true; pendingCryoNotify=true;}
                            if(maxTowers==7&&!teslaUnlocked){ teslaUnlocked=true; pendingTeslaNotify=true;}
                        } 
                    }
                    if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 200, 260, 400, 65 }, "PULSE CHARGE (300)", V_SKYBLUE)) { if(currency >= 300){ currency -= 300; pulseWaveCharges++; } }
                    if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 200, 340, 400, 65 }, TextFormat("OVERCLOCK FIRE (%d)", fC), V_GOLD)) { if(currency >= fC){ currency -= fC; towerFireRate *= 0.85f; } }
                    if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 200, 420, 400, 65 }, "CORE REPAIR (450)", V_CYAN)) { if(currency >= 450 && coreHealth < maxCoreHealth){ currency -= 450; coreHealth = std::min(coreHealth+6, maxCoreHealth); } }
                    DrawText("PRESS [U] TO DISMISS", SCREEN_WIDTH/2 - 115, 540, 20, V_DARKGRAY);
                }
            } else if (currentScreen == GAME_OVER) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 40, 10, 12, 255 });
                DrawText("SYSTEM FAILURE", SCREEN_WIDTH/2 - MeasureText("SYSTEM FAILURE", 45)/2, 60, 45, V_RED);
                
                int bW = 800, bH = 420;
                int bX = SCREEN_WIDTH/2 - bW/2, bY = 140;
                DrawRectangle(bX, bY, bW, bH, ColorAlpha(V_BLACK, 0.7f));
                DrawRectangleLines(bX, bY, bW, bH, V_DARKGRAY);
                
                DrawText("MISSION PERFORMANCE LOG", bX + 40, bY + 30, 26, V_SKYBLUE);
                DrawText(TextFormat("TOTAL HARVESTED DATA: %d", score), bX + 40, bY + 90, 20, V_WHITE);
                DrawText(TextFormat("FINAL WAVE DEPTH: %d", currentWave), bX + 40, bY + 125, 20, V_WHITE);
                DrawText(TextFormat("REMAINING FRAGMENTS: %d", currency), bX + 40, bY + 160, 20, V_GOLD);
                
                DrawText("FINAL CONFIGURATION:", bX + 440, bY + 90, 20, V_LIME);
                DrawText(TextFormat("- ACTIVE NODE SLOTS: %d", maxTowers), bX + 440, bY + 125, 18, V_WHITE);
                DrawText(TextFormat("- LASER RECHARGE: %.2fs", towerFireRate), bX + 440, bY + 155, 18, V_WHITE);
                
                // Name entry section
                if (!scoreSaved) {
                    DrawText("RECOVER SURVIVOR DATA?", bX + 40, bY + 230, 22, V_CYAN);
                    DrawRectangle(bX + 40, bY + 270, 300, 50, ColorAlpha(V_DARKGRAY, 0.5f));
                    DrawRectangleLines(bX + 40, bY + 270, 300, 50, V_CYAN);
                    DrawText(playerName, bX + 55, bY + 282, 24, V_WHITE);
                    if ((GetTime() * 2) - (int)(GetTime() * 2) > 0.5) {
                         DrawRectangle(bX + 55 + MeasureText(playerName, 24), bY + 280, 15, 30, V_WHITE);
                    }
                    DrawText("ENTER NAME & CLICK SAVE", bX + 40, bY + 330, 16, V_DARKGRAY);
                    
                    if (DrawCustomButton({ (float)bX + 360, (float)bY + 270, 200, 50 }, "SAVE DATA", V_CYAN, 20)) {
                        SaveScore(playerName, score);
                        scoreSaved = true;
                    }
                } else {
                    DrawText("SURVIVOR DATA SYNCED TO HALL OF FAME", bX + 40, bY + 282, 22, V_LIME);
                }

                if (DrawCustomButton({ (float)SCREEN_WIDTH/2 - 150, 600, 300, 65 }, "REBOOT SYSTEM", V_GOLD)) {
                    score = 0; currency = 0; currentWave = 0; pulseWaveCharges = 0; coreHealth = maxCoreHealth;
                    towers.clear(); enemies.clear(); lasers.clear(); powerups.clear(); particles.clear(); notifications.clear();
                    maxTowers = 3; towerFireRate = 0.8f; currentScreen = GAMEPLAY; waveActive = false; 
                    cryoUnlocked = false; teslaUnlocked = false; pendingCryoNotify = false; pendingTeslaNotify = false;
                }
            }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
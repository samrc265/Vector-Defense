#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>

// --- CONSTANTS & CONFIGURATION ---
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const float CORE_RADIUS = 50.0f;
const float EXCLUSION_RADIUS = CORE_RADIUS + 35.0f; 
const int UI_HEADER_HEIGHT = 60;
const int UI_FOOTER_HEIGHT = 85;

// --- COLOR PALETTE ---
const Color V_CYAN      = { 0, 255, 255, 255 };
const Color V_LIME      = { 0, 255, 100, 255 }; 
const Color V_RED       = { 255, 60, 60, 255 };
const Color V_GOLD      = { 255, 215, 0, 255 };
const Color V_WHITE     = { 245, 245, 245, 255 };
const Color V_SKYBLUE   = { 100, 200, 255, 255 };
const Color V_PURPLE    = { 200, 100, 255, 255 };
const Color V_DARKGRAY  = { 30, 30, 35, 255 };
const Color V_BLACK     = { 10, 10, 12, 255 };

// --- DATA STRUCTURES ---
enum GameScreen { START_MENU, GUIDE, GAMEPLAY, UPGRADE_MENU, GAME_OVER, LEADERBOARD, PAUSED };
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

// --- GLOBAL ASSETS & SHADERS ---
Sound sndBlip, sndBoom, sndShoot;

const char* bloomShaderCode = 
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "void main() {\n"
    "    vec4 base = texture(texture0, fragTexCoord);\n"
    "    vec4 bloom = vec4(0.0);\n"
    "    float size = 0.002;\n"
    "    bloom += texture(texture0, fragTexCoord + vec2(-size, -size)) * 0.15;\n"
    "    bloom += texture(texture0, fragTexCoord + vec2(size, -size)) * 0.15;\n"
    "    bloom += texture(texture0, fragTexCoord + vec2(-size, size)) * 0.15;\n"
    "    bloom += texture(texture0, fragTexCoord + vec2(size, size)) * 0.15;\n"
    "    finalColor = base + (bloom * 0.8);\n"
    "}\n";

// --- PERSISTENT STORAGE ---
std::vector<ScoreEntry> highScores;

void LoadHighScores() {
    highScores.clear();
    std::ifstream file("scores.txt");
    std::string name;
    int score;
    while (file >> name >> score) highScores.push_back({name, score});
    file.close();
    std::sort(highScores.begin(), highScores.end(), [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
}

void SaveScore(std::string name, int score) {
    if (name.empty()) name = "ANONYMOUS";
    std::ofstream file("scores.txt", std::ios::app);
    file << name << " " << score << "\n";
    file.close();
    LoadHighScores();
}

// --- CORE UTILITIES ---
float GetDistance(Vector2 v1, Vector2 v2) { return sqrtf(powf(v2.x - v1.x, 2) + powf(v2.y - v1.y, 2)); }

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

// --- MAIN APPLICATION ---
int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vector-Defense | Prime Edition");
    InitAudioDevice();
    SetTargetFPS(60);

    // --- ASSET LOADING (Looking in sounds/ folder) ---
    sndBlip = LoadSound("sounds/blip.wav");
    sndBoom = LoadSound("sounds/boom.wav");
    sndShoot = LoadSound("sounds/shoot.wav");
    LoadHighScores();

    Shader bloom = LoadShaderFromMemory(0, bloomShaderCode);
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

    GameScreen currentScreen = START_MENU;
    Vector2 corePos = { (float)SCREEN_WIDTH / 2, (float)SCREEN_HEIGHT / 2 };
    
    int coreHealth = 20, maxCoreHealth = 20, score = 0, currency = 0, currentWave = 0, enemiesToSpawn = 0;
    float spawnTimer = 0.0f;
    bool waveActive = false, bossInQueue = false; 
    
    int maxTowers = 3;
    float towerFireRate = 0.8f, towerRange = 230.0f; 
    int pulseWaveCharges = 0;
    TowerType currentSelection = TWR_STANDARD;
    bool cryoUnlocked = false, teslaUnlocked = false, pendingCryoNotify = false, pendingTeslaNotify = false;

    char playerName[13] = "\0";
    int letterCount = 0;
    bool scoreSaved = false;

    float waveIntroTimer = 0.0f, empTimer = 0.0f, overdriveTimer = 0.0f, empWaveRadius = 0.0f, pulseVisualRadius = 0.0f, shakeIntensity = 0.0f, damageFlashTimer = 0.0f;

    Camera2D camera = { 0 }; camera.zoom = 1.0f;
    std::vector<Enemy> enemies;
    std::vector<Tower> towers;
    std::vector<Laser> lasers;
    std::vector<PowerUp> powerups;
    std::vector<Particle> particles;
    std::vector<MenuShape> menuShapes;
    std::vector<Notification> notifications;

    for (int i = 0; i < 20; i++) {
        menuShapes.push_back({{(float)GetRandomValue(0, SCREEN_WIDTH), (float)GetRandomValue(0, SCREEN_HEIGHT)}, (float)GetRandomValue(40, 100)/100.0f, (float)GetRandomValue(0, 360), (float)GetRandomValue(-20, 20)/10.0f, GetRandomValue(3, 8), (float)GetRandomValue(30, 120)});
    }

    // --- GAME LOOP ---
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mousePos = GetMousePosition();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
            if (currentScreen == GAMEPLAY) currentScreen = PAUSED;
            else if (currentScreen == PAUSED) currentScreen = GAMEPLAY;
        }

        bool mouseInHeader = mousePos.y < UI_HEADER_HEIGHT;
        bool mouseInFooter = (!waveActive && enemies.empty()) && (mousePos.y > SCREEN_HEIGHT - UI_FOOTER_HEIGHT);
        Rectangle pulseRect = { 25, (float)SCREEN_HEIGHT - 120, 230, 50 };
        bool overPulseButton = (waveActive && pulseWaveCharges > 0 && CheckCollisionPointRec(mousePos, pulseRect));
        bool mouseOnUi = mouseInHeader || mouseInFooter || overPulseButton || (currentScreen == UPGRADE_MENU) || (currentScreen == GAME_OVER) || (currentScreen == PAUSED);

        if (shakeIntensity > 0) {
            camera.offset.x = GetRandomValue(-shakeIntensity, shakeIntensity);
            camera.offset.y = GetRandomValue(-shakeIntensity, shakeIntensity);
            shakeIntensity -= 15.0f * dt;
        } else { camera.offset = {0,0}; }

        if (damageFlashTimer > 0) damageFlashTimer -= dt;

        // --- SYSTEM UPDATE ---
        switch (currentScreen) {
            case START_MENU: {
                for (auto &ms : menuShapes) {
                    ms.pos.y -= ms.speed; ms.rotation += ms.rotSpeed;
                    if (ms.pos.y < -ms.size) { ms.pos.y = SCREEN_HEIGHT + ms.size; ms.pos.x = (float)GetRandomValue(0, SCREEN_WIDTH); }
                }
                if (IsKeyPressed(KEY_ENTER)) { PlaySound(sndBlip); currentScreen = GAMEPLAY; }
            } break;

            case GUIDE: { if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) currentScreen = START_MENU; } break;
            case LEADERBOARD: { if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) currentScreen = START_MENU; } break;
            case PAUSED: break;

            case UPGRADE_MENU: {
                int slotCost = 400 + (maxTowers - 3) * 350;
                int fireCost = 600 + (int)((0.8f - towerFireRate) * 10000);

                if (CheckCollisionPointRec(mousePos, { SCREEN_WIDTH/2 - 200, 180, 400, 65 }) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (currency >= slotCost) { currency -= slotCost; maxTowers++; PlaySound(sndBlip);
                        if (maxTowers == 5 && !cryoUnlocked) { cryoUnlocked = true; pendingCryoNotify = true; }
                        if (maxTowers == 7 && !teslaUnlocked) { teslaUnlocked = true; pendingTeslaNotify = true; }
                    }
                }
                if (CheckCollisionPointRec(mousePos, { SCREEN_WIDTH/2 - 200, 260, 400, 65 }) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (currency >= 300) { currency -= 300; pulseWaveCharges++; PlaySound(sndBlip); }
                }
                if (CheckCollisionPointRec(mousePos, { SCREEN_WIDTH/2 - 200, 340, 400, 65 }) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (currency >= fireCost) { currency -= fireCost; towerFireRate *= 0.85f; PlaySound(sndBlip); }
                }
                if (CheckCollisionPointRec(mousePos, { SCREEN_WIDTH/2 - 200, 420, 400, 65 }) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (currency >= 450 && coreHealth < maxCoreHealth) { currency -= 450; coreHealth = std::min(coreHealth + 6, maxCoreHealth); PlaySound(sndBlip); }
                }

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
                if (waveIntroTimer > 0) waveIntroTimer -= dt;
                if (IsKeyPressed(KEY_ONE)) currentSelection = TWR_STANDARD;
                if (IsKeyPressed(KEY_TWO) && cryoUnlocked) currentSelection = TWR_CRYO;
                if (IsKeyPressed(KEY_THREE) && teslaUnlocked) currentSelection = TWR_TESLA;

                if (waveActive && pulseWaveCharges > 0 && CheckCollisionPointRec(mousePos, pulseRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    pulseWaveCharges--; shakeIntensity = 35.0f; pulseVisualRadius = 10.0f; PlaySound(sndBoom);
                    notifications.push_back({"PULSE DISCHARGED", 2.5f, V_RED});
                    for (auto &e : enemies) {
                        float dist = GetDistance(corePos, e.position);
                        if (dist < 450.0f) { e.health -= (500.0f - dist) / 5.0f; if(e.health <= 0) e.active = false; }
                    }
                }

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
                            PlaySound(sndBlip); p.active = false; pickedUp = true; break;
                        }
                    }
                    if (!pickedUp && towers.size() < (size_t)maxTowers && GetDistance(mousePos, corePos) > EXCLUSION_RADIUS) {
                        PlaySound(sndBlip); towers.push_back({ mousePos, 0.0f, currentSelection });
                    }
                }

                if (IsKeyPressed(KEY_SPACE) && pulseWaveCharges > 0 && waveActive) {
                    pulseWaveCharges--; shakeIntensity = 35.0f; pulseVisualRadius = 10.0f; PlaySound(sndBoom);
                    notifications.push_back({"PULSE DISCHARGED", 2.5f, V_RED});
                    for (auto &e : enemies) {
                        float dist = GetDistance(corePos, e.position);
                        if (dist < 450.0f) { e.health -= (500.0f - dist) / 5.0f; if(e.health <= 0) e.active = false; }
                    }
                }

                if (waveActive && waveIntroTimer <= 0) {
                    spawnTimer += dt;
                    if (enemiesToSpawn > 0 && spawnTimer > std::max(0.15f, 1.25f - (currentWave * 0.06f))) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy e; e.position = { corePos.x + cosf(angle) * 850.0f, corePos.y + sinf(angle) * 850.0f };
                        e.radius = 22.0f; e.sides = GetRandomValue(3, std::min(10, 3 + (currentWave / 2)));
                        e.speed = (180.0f - ((float)e.sides * 8.0f)) * std::min(1.6f, 1.0f + (currentWave * 0.035f)); 
                        e.maxHealth = (float)e.sides * 1.2f; e.health = e.maxHealth; e.active = true; e.slowTimer = 0; enemies.push_back(e); enemiesToSpawn--; spawnTimer = 0;
                    } else if (enemiesToSpawn <= 0 && bossInQueue && spawnTimer > 1.8f) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy boss; boss.position = { corePos.x+cosf(angle)*850.0f, corePos.y+sinf(angle)*850.0f };
                        boss.sides = 24; boss.radius = 90.0f; boss.maxHealth = 180.0f + ((float)currentWave * 25.0f); boss.health = boss.maxHealth; boss.speed = 25.0f; boss.active = true; boss.slowTimer = 0;
                        enemies.push_back(boss); bossInQueue = false; spawnTimer = 0; notifications.push_back({"BOSS DETECTED", 3.0f, V_RED});
                    }
                }

                for (auto it = enemies.begin(); it != enemies.end();) {
                    if (empTimer <= 0) { 
                        float moveSpeed = it->speed; if (it->slowTimer > 0) { moveSpeed *= 0.4f; it->slowTimer -= dt; }
                        float angle = atan2f(corePos.y - it->position.y, corePos.x - it->position.x);
                        it->position.x += cosf(angle) * moveSpeed * dt; it->position.y += sinf(angle) * moveSpeed * dt;
                    }
                    if (GetDistance(it->position, corePos) < CORE_RADIUS) {
                        if (it->sides == 24) { coreHealth -= 5; shakeIntensity = 45.0f; } else { coreHealth--; shakeIntensity = 18.0f; }
                        damageFlashTimer = 0.18f; it = enemies.erase(it);
                    } else if (!it->active) {
                        currency += (it->sides * 14) + 20; score += (int)(it->maxHealth * 100);
                        SpawnParticleBurst(particles, it->position, V_WHITE, 12, 2.0f);
                        if (it->sides >= 6) { for(int s=0; s<2; s++) enemies.push_back({it->position, 180.0f, 3, 5.0f, 5.0f, true, 16.0f, 0}); }
                        if(GetRandomValue(1, 100) <= 20) powerups.push_back({it->position, (PowerType)GetRandomValue(0, 2), 10.0f, true, 0.0f});
                        it = enemies.erase(it);
                    } else ++it;
                }

                if (waveActive && enemiesToSpawn <= 0 && !bossInQueue && enemies.empty()) { waveActive = false; towers.clear(); notifications.push_back({"WAVE CLEAR", 2.0f, V_SKYBLUE}); }

                for (auto &t : towers) {
                    t.shootTimer += dt;
                    if (overdriveTimer > 0 && GetRandomValue(0, 4) == 0) particles.push_back({{t.position.x + (float)GetRandomValue(-15,15), t.position.y + (float)GetRandomValue(-15,15)}, {0, -120}, V_GOLD, 0.4f, 0.4f, false});
                    
                    float rate = (overdriveTimer > 0) ? 0.05f : towerFireRate;
                    if (t.type == TWR_CRYO || t.type == TWR_TESLA) rate *= 1.5f;
                    
                    if (t.shootTimer >= rate) {
                        Enemy* target = nullptr; float minDist = towerRange;
                        for (auto& e : enemies) { float d = GetDistance(t.position, e.position); if (d < minDist) { minDist = d; target = &e; } }
                        if (target) {
                            PlaySound(sndShoot); shakeIntensity += 1.5f; 
                            if (t.type == TWR_CRYO) { target->health -= 0.5f; target->slowTimer = 1.5f; lasers.push_back({ t.position, target->position, 0.07f, V_SKYBLUE }); }
                            else if (t.type == TWR_TESLA) { 
                                target->health -= 0.8f; lasers.push_back({ t.position, target->position, 0.07f, V_GOLD });
                                Enemy* sec = nullptr; float sDist = 200.0f;
                                for (auto& e2 : enemies) { if (&e2 == target) continue; float d2 = GetDistance(target->position, e2.position); if (d2 < sDist) { sDist = d2; sec = &e2; } }
                                if (sec) { sec->health -= 0.6f; if (sec->health <= 0) sec->active = false; lasers.push_back({ target->position, sec->position, 0.12f, V_GOLD }); }
                            } else { target->health -= 1.0f; lasers.push_back({ t.position, target->position, 0.07f, V_WHITE }); }
                            if (target->health <= 0) target->active = false;
                            t.shootTimer = 0;
                        }
                    }
                }

                for (auto it = lasers.begin(); it != lasers.end();) { it->lifetime -= dt; if (it->lifetime <= 0) it = lasers.erase(it); else ++it; }
                for (auto it = powerups.begin(); it != powerups.end();) { 
                    if (waveActive) { it->timer -= dt; }
                    it->rotation += 120.0f * dt; 
                    if (it->timer <= 0 || !it->active) { it = powerups.erase(it); } else { ++it; }
                }
                for (auto it = particles.begin(); it != particles.end();) {
                    it->life -= dt;
                    if (it->seekingCore) { float a = atan2f(corePos.y - it->pos.y, corePos.x - it->pos.x); it->pos.x += cosf(a) * 600.0f * dt; it->pos.y += sinf(a) * 600.0f * dt; if (GetDistance(it->pos, corePos) < 15.0f) it->life = 0; }
                    else { it->pos.x += it->vel.x * dt; it->pos.y += it->vel.y * dt; }
                    if (it->life <= 0) it = particles.erase(it); else ++it;
                }
                for (auto it = notifications.begin(); it != notifications.end();) { it->timer -= dt; if (it->timer <= 0) it = notifications.erase(it); else ++it; }
                if (empWaveRadius > 0) { empWaveRadius += 1600.0f * dt; if (empWaveRadius > 2500.0f) { empWaveRadius = 0; } }
                if (pulseVisualRadius > 0) { pulseVisualRadius += 2200.0f * dt; if (pulseVisualRadius > 1500.0f) { pulseVisualRadius = 0; } }
                if (empTimer > 0) empTimer -= dt;
                if (overdriveTimer > 0) overdriveTimer -= dt;

                if (coreHealth <= 0) { currentScreen = GAME_OVER; scoreSaved = false; playerName[0] = '\0'; letterCount = 0; }
                if (IsKeyPressed(KEY_U) && !waveActive) { currentScreen = UPGRADE_MENU; }
            } break;

            case GAME_OVER: {
                int key = GetCharPressed();
                while (key > 0) { if ((key >= 32) && (key <= 125) && (letterCount < 12)) { playerName[letterCount] = (char)key; playerName[letterCount+1] = '\0'; letterCount++; } key = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE)) { letterCount--; if (letterCount < 0) { letterCount = 0; } playerName[letterCount] = '\0'; }
            } break;
        }

        // --- RENDERING PIPELINE ---
        BeginTextureMode(target);
            ClearBackground(V_BLACK);
            BeginMode2D(camera);
                for(int i = -100; i < SCREEN_WIDTH + 100; i += 64) DrawLine(i, -100, i, SCREEN_HEIGHT + 100, {30, 30, 35, 255});
                for(int i = -100; i < SCREEN_HEIGHT + 100; i += 64) DrawLine(-100, i, SCREEN_WIDTH + 100, i, {30, 30, 35, 255});
                
                if (currentScreen == START_MENU) {
                    for (const auto& ms : menuShapes) DrawPolyLinesEx(ms.pos, ms.sides, ms.size, ms.rotation, 1.5f, ColorAlpha(V_DARKGRAY, 0.4f));
                    DrawText("VECTOR DEFENSE", SCREEN_WIDTH/2 - MeasureText("VECTOR DEFENSE", 60)/2, 220, 60, V_CYAN);
                }
                if (currentScreen != GUIDE && currentScreen != LEADERBOARD) {
                    for(const auto& p : particles) DrawCircle((int)p.pos.x, (int)p.pos.y, 2, p.col);
                    if (empWaveRadius > 0) DrawCircleLines((int)corePos.x, (int)corePos.y, empWaveRadius, ColorAlpha(V_PURPLE, 1.0f - (empWaveRadius/2500.0f)));
                    if (pulseVisualRadius > 0) DrawRing(corePos, pulseVisualRadius - 15.0f, pulseVisualRadius, 0, 360, 60, ColorAlpha(V_RED, 1.0f - (pulseVisualRadius/1500.0f)));
                    for (const auto& l : lasers) DrawLineEx(l.start, l.end, 3.0f, l.col);
                    for(const auto& p : powerups) DrawPolyLinesEx({p.position.x, p.position.y + sinf(GetTime()*5)*5}, 4, 18, p.rotation, 2, (p.type == PWR_EMP ? V_PURPLE : (p.type == PWR_OVERDRIVE ? V_GOLD : V_CYAN)));
                    if (currentScreen != START_MENU) {
                        DrawCircleLines((int)corePos.x, (int)corePos.y, EXCLUSION_RADIUS, ColorAlpha(V_RED, 0.3f));
                        DrawCircleLines((int)corePos.x, (int)corePos.y, CORE_RADIUS, V_CYAN);
                        DrawCircle((int)corePos.x, (int)corePos.y, 4, V_WHITE);
                    }
                    if (currentScreen == GAMEPLAY && !mouseOnUi && towers.size() < (size_t)maxTowers) {
                        bool valid = GetDistance(mousePos, corePos) > EXCLUSION_RADIUS;
                        DrawCircleLines((int)mousePos.x, (int)mousePos.y, towerRange, ColorAlpha(valid ? V_WHITE : V_RED, 0.3f));
                        DrawHealthBody(mousePos, (currentSelection == TWR_STANDARD ? 4 : (currentSelection == TWR_CRYO ? 6 : 8)), 18, 1.0f, ColorAlpha(valid ? (currentSelection == TWR_STANDARD ? V_LIME : (currentSelection == TWR_CRYO ? V_SKYBLUE : V_GOLD)) : V_RED, 0.5f));
                    }
                    for (const auto& t : towers) { DrawCircleLines((int)t.position.x, (int)t.position.y, towerRange, ColorAlpha(V_WHITE, 0.1f)); DrawHealthBody(t.position, (t.type == TWR_STANDARD ? 4 : (t.type == TWR_CRYO ? 6 : 8)), 18, 1.0f, (t.type == TWR_STANDARD ? V_LIME : (t.type == TWR_CRYO ? V_SKYBLUE : V_GOLD))); }
                    for (const auto& e : enemies) DrawHealthBody(e.position, e.sides, e.radius, e.health/e.maxHealth, e.slowTimer > 0 ? V_SKYBLUE : V_RED);
                }
            EndMode2D();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(V_BLACK);
            BeginShaderMode(bloom);
                DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
            EndShaderMode();

            if (damageFlashTimer > 0) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_RED, damageFlashTimer * 1.5f));
            
            if (currentScreen == START_MENU) {
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 360, 300, 65 }, "BOOT SEQUENCE", V_LIME)) { currentScreen = GAMEPLAY; waveActive = false; }
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 440, 300, 65 }, "LEADERBOARD", V_GOLD)) currentScreen = LEADERBOARD;
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 520, 300, 65 }, "SYSTEM GUIDE", V_WHITE)) currentScreen = GUIDE;
            } 
            else if (currentScreen == PAUSED) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.6f));
                DrawText("SYSTEM PAUSED", SCREEN_WIDTH/2 - MeasureText("SYSTEM PAUSED", 40)/2, 280, 40, V_CYAN);
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 120, 350, 240, 60 }, "RESUME", V_LIME)) currentScreen = GAMEPLAY;
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 120, 420, 240, 60 }, "QUIT", V_RED)) currentScreen = START_MENU;
            }
            else if (currentScreen == GUIDE || currentScreen == LEADERBOARD) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                if (currentScreen == GUIDE) {
                    DrawText("SYSTEM OPERATIONAL GUIDE", 60, 60, 35, V_SKYBLUE);
                    int x1 = 70, x2 = 650, y = 140;
                    DrawText("THREAT LOG", x1, y, 22, V_RED); DrawText("- Splitting: Complex shapes split into fragments.", x1, y+35, 18, V_WHITE); DrawText("- BOSS LOG: Heavy Primes emerge every 10 waves.", x1, y+60, 18, V_GOLD);
                    DrawText("DEFENSE LOG", x1, y+130, 22, V_LIME); DrawText("- [1] Standard: Green squares. Normal DPS.", x1, y+165, 18, V_WHITE); DrawText("- [2] Cryo-Slow: Blue hexagons. Freezes threats.", x1, y+190, 18, V_WHITE); DrawText("- [3] Tesla: Gold Octagon. Chain lightning.", x1, y+215, 18, V_WHITE);
                    DrawText("POWER-UPS", x2, y, 22, V_GOLD); DrawText("- [EMP] Purple: Total movement lock-down.", x2, y+35, 18, V_PURPLE); DrawText("- [OVERDRIVE] Gold: Maximum fire-rate sparks.", x2, y+60, 18, V_GOLD); DrawText("- [NANOBOTS] Cyan: Core absorption repair.", x2, y+85, 18, V_SKYBLUE);
                    DrawText("SYSTEM CYCLE", x2, y+155, 22, V_CYAN); DrawText("- [SPACE/Button]: Discharge Red Pulse charges.", x2, y+190, 18, V_WHITE); DrawText("- Armory [U]: Upgrade slots and laser fire speed.", x2, y+215, 18, V_WHITE);
                } else {
                    DrawText("SYSTEM HALL OF FAME", SCREEN_WIDTH/2 - MeasureText("SYSTEM HALL OF FAME", 35)/2, 60, 35, V_GOLD);
                    for(int i=0; i<std::min(10, (int)highScores.size()); i++) { DrawText(TextFormat("%d. %s", i+1, highScores[i].name.c_str()), SCREEN_WIDTH/2 - 200, 140 + (i*40), 22, V_WHITE); DrawText(TextFormat("%d", highScores[i].score), SCREEN_WIDTH/2 + 150, 140 + (i*40), 22, V_SKYBLUE); }
                }
                if (DrawCustomButton({ SCREEN_WIDTH/2-100, 620, 200, 50 }, "< RETURN", V_WHITE)) currentScreen = START_MENU;
            } else if (currentScreen == GAMEPLAY || currentScreen == UPGRADE_MENU) {
                DrawRectangle(0, 0, SCREEN_WIDTH, UI_HEADER_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText(TextFormat("INTEGRITY: %d", coreHealth), 25, 20, 22, coreHealth < 5 ? V_RED : V_WHITE); DrawText(TextFormat("FRAGMENTS: %d", currency), 220, 20, 22, V_GOLD); DrawText(TextFormat("NODES: %d/%d", (int)towers.size(), maxTowers), 420, 20, 22, V_LIME); DrawText(TextFormat("WAVE: %d", currentWave), 580, 20, 22, V_SKYBLUE); DrawText(TextFormat("PULSE: %d", pulseWaveCharges), 720, 20, 22, V_CYAN);
                std::string mStr = (currentSelection == TWR_CRYO ? "CRYO" : (currentSelection == TWR_TESLA ? "TESLA" : "STANDARD"));
                Color mCol = (currentSelection == TWR_CRYO ? V_SKYBLUE : (currentSelection == TWR_TESLA ? V_GOLD : V_LIME));
                DrawText(TextFormat("ACTIVE: %s", mStr.c_str()), SCREEN_WIDTH - 250, 20, 20, mCol);
                
                if (waveIntroTimer > 0) {
                    float alpha = (waveIntroTimer > 1.0f) ? 1.0f : waveIntroTimer;
                    std::string waveText = "WAVE " + std::to_string(currentWave);
                    DrawText(waveText.c_str(), SCREEN_WIDTH/2 - MeasureText(waveText.c_str(), 80)/2, SCREEN_HEIGHT/2 - 40, 80, ColorAlpha(V_WHITE, alpha));
                }

                if (waveActive) {
                    if (pulseWaveCharges > 0) {
                        DrawRectangleRec(pulseRect, CheckCollisionPointRec(mousePos, pulseRect) ? ColorAlpha(V_SKYBLUE, 0.35f) : ColorAlpha(V_DARKGRAY, 0.6f));
                        DrawRectangleLinesEx(pulseRect, 2, CheckCollisionPointRec(mousePos, pulseRect) ? V_SKYBLUE : ColorAlpha(V_WHITE, 0.2f));
                        int tw = MeasureText(TextFormat("ACTIVATE PULSE [%d]", pulseWaveCharges), 18);
                        DrawText(TextFormat("ACTIVATE PULSE [%d]", pulseWaveCharges), pulseRect.x + (pulseRect.width/2 - tw/2), pulseRect.y + (pulseRect.height/2 - 9), 18, V_WHITE);
                    }
                    DrawText(TextFormat("THREATS: %d", (int)enemies.size() + enemiesToSpawn + (bossInQueue?1:0)), 25, SCREEN_HEIGHT - 35, 20, V_SKYBLUE);
                }
                for (int i = 0; i < (int)notifications.size(); i++) DrawText(notifications[i].text.c_str(), SCREEN_WIDTH/2 - MeasureText(notifications[i].text.c_str(), 30)/2, 110 + (i * 45), 30, ColorAlpha(notifications[i].col, notifications[i].timer/2.0f));
                
                if (currentScreen == GAMEPLAY && !waveActive && enemies.empty()) {
                    DrawRectangle(0, SCREEN_HEIGHT - UI_FOOTER_HEIGHT, SCREEN_WIDTH, UI_FOOTER_HEIGHT, ColorAlpha(V_BLACK, 0.85f));
                    DrawText("SYSTEM IDLE // BUILD PHASE", 40, SCREEN_HEIGHT - 55, 20, V_SKYBLUE);
                    std::string p = "[1] STANDARD"; if(cryoUnlocked) p += " | [2] CRYO"; if(teslaUnlocked) p += " | [3] TESLA"; DrawText(p.c_str(), 40, SCREEN_HEIGHT - 75, 18, V_DARKGRAY);
                    if (DrawCustomButton({ SCREEN_WIDTH - 550, SCREEN_HEIGHT - 72, 250, 60 }, "OPEN ARMORY [U]", V_GOLD)) currentScreen = UPGRADE_MENU;
                    if (DrawCustomButton({ SCREEN_WIDTH - 280, SCREEN_HEIGHT - 72, 250, 60 }, "START WAVE", V_LIME)) {
                        currentWave++; waveActive = true; enemiesToSpawn = 7 + (currentWave * 5); 
                        waveIntroTimer = 2.5f; 
                        if (currentWave % 10 == 0) bossInQueue = true;
                    }
                }
                if (currentScreen == UPGRADE_MENU) {
                    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.9f));
                    DrawText("SYSTEM ARMORY", SCREEN_WIDTH/2 - 120, 60, 35, V_SKYBLUE); DrawText(TextFormat("AVAILABLE DATA: %d", currency), SCREEN_WIDTH/2 - MeasureText(TextFormat("AVAILABLE DATA: %d", currency), 24)/2, 120, 24, V_GOLD);
                    int sC = 400 + (maxTowers - 3) * 350; int fC = 600 + (int)((0.8f - towerFireRate) * 10000); 
                    
                    DrawCustomButton({ SCREEN_WIDTH/2 - 200, 180, 400, 65 }, TextFormat("BUY NODE SLOT (%d)", sC), V_LIME);
                    DrawCustomButton({ SCREEN_WIDTH/2 - 200, 260, 400, 65 }, "PULSE CHARGE (300)", V_SKYBLUE);
                    DrawCustomButton({ SCREEN_WIDTH/2 - 200, 340, 400, 65 }, TextFormat("OVERCLOCK FIRE (%d)", fC), V_GOLD);
                    DrawCustomButton({ SCREEN_WIDTH/2 - 200, 420, 400, 65 }, "CORE REPAIR (450)", V_CYAN);
                    
                    DrawText("PRESS [U] TO DISMISS", SCREEN_WIDTH/2 - 115, 540, 20, V_DARKGRAY);
                }
            } else if (currentScreen == GAME_OVER) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 40, 10, 12, 255 });
                DrawText("SYSTEM FAILURE", SCREEN_WIDTH/2 - MeasureText("SYSTEM FAILURE", 45)/2, 60, 45, V_RED);
                int bW = 800, bH = 420, bX = SCREEN_WIDTH/2 - bW/2, bY = 140;
                DrawRectangle(bX, bY, bW, bH, ColorAlpha(V_BLACK, 0.7f)); DrawRectangleLines(bX, bY, bW, bH, V_DARKGRAY);
                DrawText("MISSION PERFORMANCE LOG", bX + 40, bY + 30, 26, V_SKYBLUE); 
                DrawText(TextFormat("TOTAL DATA: %d", score), bX + 40, bY + 90, 20, V_WHITE); 
                DrawText(TextFormat("WAVE DEPTH: %d", currentWave), bX + 40, bY + 125, 20, V_WHITE); 
                DrawText(TextFormat("REMAINING FRAGMENTS: %d", currency), bX + 40, bY + 160, 20, V_GOLD);
                
                DrawText("FINAL CONFIG:", bX + 440, bY + 90, 20, V_LIME); 
                DrawText(TextFormat("- NODES: %d", maxTowers), bX + 440, bY + 125, 18, V_WHITE); 
                DrawText(TextFormat("- RECHARGE: %.2fs", towerFireRate), bX + 440, bY + 155, 18, V_WHITE);
                
                if (!scoreSaved) { 
                    DrawText("RECOVER SURVIVOR DATA?", bX + 40, bY + 230, 22, V_CYAN); 
                    DrawRectangle(bX + 40, bY + 270, 300, 50, ColorAlpha(V_DARKGRAY, 0.5f)); 
                    DrawRectangleLines(bX + 40, bY + 270, 300, 50, V_CYAN); 
                    DrawText(playerName, bX + 55, bY + 282, 24, V_WHITE); 
                    if ((GetTime() * 2) - (int)(GetTime() * 2) > 0.5) { DrawRectangle(bX + 55 + MeasureText(playerName, 24), bY + 280, 15, 30, V_WHITE); }
                    if (DrawCustomButton({ (float)bX + 360, (float)bY + 270, 200, 50 }, "SAVE DATA", V_CYAN, 20)) { SaveScore(playerName, score); scoreSaved = true; } 
                } else { DrawText("DATA SYNCED TO HALL OF FAME", bX + 40, bY + 282, 22, V_LIME); }
                
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 600, 300, 65 }, "REBOOT SYSTEM", V_GOLD)) { 
                    score = 0; currency = 0; currentWave = 0; pulseWaveCharges = 0; coreHealth = maxCoreHealth; 
                    towers.clear(); enemies.clear(); lasers.clear(); powerups.clear(); particles.clear(); notifications.clear(); 
                    maxTowers = 3; towerFireRate = 0.8f; currentScreen = GAMEPLAY; waveActive = false; cryoUnlocked = false; teslaUnlocked = false; 
                }
            }
        EndDrawing();
    }

    // --- CLEANUP ---
    UnloadSound(sndBlip); UnloadSound(sndBoom); UnloadSound(sndShoot); 
    UnloadShader(bloom); UnloadRenderTexture(target); 
    CloseAudioDevice(); CloseWindow(); 
    return 0;
}
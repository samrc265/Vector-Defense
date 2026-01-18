#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

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

enum GameScreen { START_MENU, GUIDE, GAMEPLAY, UPGRADE_MENU, GAME_OVER };
enum PowerType { PWR_EMP, PWR_OVERDRIVE, PWR_HEAL, PWR_NONE };

struct Enemy {
    Vector2 position;
    float speed;
    int sides;
    float health;
    float maxHealth;
    bool active;
    float radius; 
};

struct Tower {
    Vector2 position;
    float shootTimer;
};

struct PowerUp {
    Vector2 position;
    PowerType type;
    float timer; 
    bool active;
    float rotation; 
};

struct Laser {
    Vector2 start; Vector2 end; float lifetime;
};

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color col;
    float life;
    float maxLife;
    bool seekingCore; 
};

struct MenuShape {
    Vector2 pos;
    float speed;
    float rotation;
    float rotSpeed;
    int sides;
    float size;
};

struct Notification {
    std::string text;
    float timer;
    Color col;
};

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
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vector-Defense | Portfolio Edition");
    SetTargetFPS(60);

    GameScreen currentScreen = START_MENU;
    Vector2 corePos = { (float)SCREEN_WIDTH / 2, (float)SCREEN_HEIGHT / 2 };
    
    // Persistent Stats
    int coreHealth = 20, maxCoreHealth = 20, score = 0, currency = 0;
    int currentWave = 0, enemiesToSpawn = 0;
    float spawnTimer = 0.0f;
    bool waveActive = false;
    bool bossInQueue = false; 
    
    // Upgrades
    int maxTowers = 3;
    float towerFireRate = 0.8f;
    float towerRange = 230.0f; 

    // Power-up States
    float empTimer = 0.0f;
    float overdriveTimer = 0.0f;
    float empWaveRadius = 0.0f; 

    // FX States
    float shakeIntensity = 0.0f;
    float damageFlashTimer = 0.0f;
    Camera2D camera = { 0 };
    camera.zoom = 1.0f;

    std::vector<Enemy> enemies;
    std::vector<Tower> towers;
    std::vector<Laser> lasers;
    std::vector<PowerUp> powerups;
    std::vector<Particle> particles;
    std::vector<MenuShape> menuShapes;
    std::vector<Notification> notifications;

    // Initialize Menu Shapes (Ghost Polygons)
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
        bool mouseOnUi = mouseInHeader || mouseInFooter || (currentScreen == UPGRADE_MENU);

        if (shakeIntensity > 0) {
            camera.offset.x = GetRandomValue(-shakeIntensity, shakeIntensity);
            camera.offset.y = GetRandomValue(-shakeIntensity, shakeIntensity);
            shakeIntensity -= 15.0f * dt;
        } else { camera.offset = (Vector2){ 0, 0 }; }

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
            case UPGRADE_MENU: if (IsKeyPressed(KEY_U) || IsKeyPressed(KEY_ENTER)) currentScreen = GAMEPLAY; break;

            case GAMEPLAY: {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !mouseOnUi) {
                    bool pickedUp = false;
                    for(auto &p : powerups) {
                        if(p.active && GetDistance(mousePos, p.position) < 45) {
                            if(p.type == PWR_EMP) { 
                                empTimer = 4.5f; empWaveRadius = 10.0f; 
                                notifications.push_back({"SYSTEM EMP ACTIVATED", 2.0f, V_PURPLE});
                            }
                            else if(p.type == PWR_OVERDRIVE) { 
                                overdriveTimer = 7.0f; SpawnParticleBurst(particles, p.position, V_GOLD, 40, 5.0f); 
                                notifications.push_back({"LASER OVERDRIVE ONLINE", 2.0f, V_GOLD});
                            }
                            else if(p.type == PWR_HEAL) {
                                coreHealth = std::min(coreHealth + 3, maxCoreHealth); 
                                notifications.push_back({"INTEGRITY RESTORED", 2.0f, V_CYAN});
                                for(int i=0; i<150; i++) {
                                    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                                    float dist = (float)GetRandomValue(0, 50);
                                    particles.push_back({{p.position.x + cosf(angle)*dist, p.position.y + sinf(angle)*dist}, {0,0}, V_CYAN, 2.0f, 2.0f, true});
                                }
                            }
                            p.active = false; pickedUp = true; break;
                        }
                    }
                    if (!pickedUp && towers.size() < (size_t)maxTowers && GetDistance(mousePos, corePos) > EXCLUSION_RADIUS) {
                        towers.push_back({ mousePos, 0.0f });
                    }
                }

                if (waveActive) {
                    spawnTimer += dt;
                    float spawnRate = std::max(0.15f, 1.25f - (currentWave * 0.06f));
                    if (enemiesToSpawn > 0 && spawnTimer > spawnRate) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy e; e.position = { corePos.x + cosf(angle) * 850.0f, corePos.y + sinf(angle) * 850.0f };
                        e.radius = 22.0f; int maxSides = std::min(10, 3 + (currentWave / 2)); e.sides = GetRandomValue(3, maxSides);
                        float speedMult = std::min(1.6f, 1.0f + (currentWave * 0.035f));
                        if (e.sides == 3) { e.maxHealth = 2; e.speed = 190 * speedMult; }
                        else if (e.sides == 4) { e.maxHealth = 4; e.speed = 130 * speedMult; }
                        else if (e.sides == 5) { e.maxHealth = 9; e.speed = 90 * speedMult; }
                        else if (e.sides == 6) { e.maxHealth = 15; e.speed = 70 * speedMult; }
                        else if (e.sides == 7) { e.maxHealth = 22; e.speed = 55 * speedMult; }
                        else if (e.sides == 8) { e.maxHealth = 30; e.speed = 45 * speedMult; }
                        else if (e.sides == 9) { e.maxHealth = 40; e.speed = 35 * speedMult; }
                        else { e.maxHealth = 55; e.speed = 25 * speedMult; } 
                        e.health = e.maxHealth; e.active = true; enemies.push_back(e); enemiesToSpawn--; spawnTimer = 0;
                    } 
                    else if (enemiesToSpawn <= 0 && bossInQueue && spawnTimer > 1.8f) {
                        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                        Enemy boss; boss.position = { corePos.x + cosf(angle) * 850.0f, corePos.y + sinf(angle) * 850.0f };
                        boss.sides = 20; boss.radius = 90.0f; boss.maxHealth = 180.0f + (currentWave * 25.0f);
                        boss.health = boss.maxHealth; boss.speed = 20.0f; boss.active = true;
                        enemies.push_back(boss); bossInQueue = false; spawnTimer = 0;
                        notifications.push_back({"BOSS ANOMALY DETECTED", 3.0f, V_RED});
                    }
                }

                std::vector<Enemy> newSplits;
                for (auto it = enemies.begin(); it != enemies.end();) {
                    if (empTimer <= 0) { 
                        float angle = atan2f(corePos.y - it->position.y, corePos.x - it->position.x);
                        it->position.x += cosf(angle) * it->speed * dt;
                        it->position.y += sinf(angle) * it->speed * dt;
                    }
                    if (GetDistance(it->position, corePos) < CORE_RADIUS) {
                        coreHealth--; shakeIntensity = 18.0f; damageFlashTimer = 0.18f; 
                        SpawnParticleBurst(particles, it->position, V_RED, 20, 3.0f);
                        it = enemies.erase(it);
                    } else if (!it->active) {
                        currency += (it->sides * 10) + 20; score += (int)(it->maxHealth * 100);
                        SpawnParticleBurst(particles, it->position, V_WHITE, 12, 2.0f);
                        if (it->sides >= 6 && it->sides < 20) { 
                            for(int s=0; s<2; s++) {
                                Enemy mini; mini.position = it->position; mini.sides = 4;
                                mini.radius = 16.0f; mini.maxHealth = 4; mini.health = 4;
                                mini.speed = it->speed * 1.4f; mini.active = true; newSplits.push_back(mini);
                            }
                        }
                        if(GetRandomValue(1, 100) <= 20) powerups.push_back({it->position, (PowerType)GetRandomValue(0, 2), 10.0f, true, 0.0f});
                        it = enemies.erase(it);
                    } else ++it;
                }
                for(auto &ns : newSplits) enemies.push_back(ns);

                if (waveActive && enemiesToSpawn <= 0 && !bossInQueue && enemies.empty()) { 
                    waveActive = false; towers.clear(); notifications.push_back({"WAVE NEUTRALIZED", 2.0f, V_SKYBLUE});
                }

                float currentRate = (overdriveTimer > 0) ? 0.04f : towerFireRate;
                for (auto &t : towers) {
                    t.shootTimer += dt;
                    if (overdriveTimer > 0 && GetRandomValue(0, 4) == 0) {
                        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
                        particles.push_back({{t.position.x + cosf(a)*18, t.position.y + sinf(a)*18}, {0, -100}, V_GOLD, 0.5f, 0.5f, false});
                    }
                    if (t.shootTimer >= currentRate) {
                        Enemy* target = nullptr; float minDist = towerRange;
                        for (auto& e : enemies) {
                            float d = GetDistance(t.position, e.position);
                            if (d < minDist) { minDist = d; target = &e; }
                        }
                        if (target) {
                            target->health -= 1.0f; if (target->health <= 0) target->active = false;
                            lasers.push_back({ t.position, target->position, 0.07f }); t.shootTimer = 0;
                        }
                    }
                }

                for (auto it = lasers.begin(); it != lasers.end();) {
                    it->lifetime -= dt; if (it->lifetime <= 0) it = lasers.erase(it); else ++it;
                }
                
                // Fixed misleading indentation and logic scope
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
                        float angle = atan2f(corePos.y - it->pos.y, corePos.x - it->pos.x);
                        it->pos.x += cosf(angle) * 550.0f * dt; it->pos.y += sinf(angle) * 550.0f * dt;
                        if (GetDistance(it->pos, corePos) < 12) it->life = 0;
                    } else { it->pos.x += it->vel.x * dt; it->pos.y += it->vel.y * dt; }
                    if (it->life <= 0) it = particles.erase(it); else ++it;
                }
                
                for (auto it = notifications.begin(); it != notifications.end();) {
                    it->timer -= dt; if (it->timer <= 0) it = notifications.erase(it); else ++it;
                }

                if (empWaveRadius > 0) { empWaveRadius += 1600.0f * dt; if (empWaveRadius > 2500.0f) empWaveRadius = 0; }
                if (empTimer > 0) empTimer -= dt;
                if (overdriveTimer > 0) overdriveTimer -= dt;
                if (coreHealth <= 0) currentScreen = GAME_OVER;
                if (IsKeyPressed(KEY_U) && !waveActive) currentScreen = UPGRADE_MENU;
            } break;

            case GAME_OVER: if (IsKeyPressed(KEY_ENTER)) {
                score = 0; currency = 0; currentWave = 0; enemiesToSpawn = 0; coreHealth = maxCoreHealth;
                towers.clear(); enemies.clear(); lasers.clear(); powerups.clear(); particles.clear(); notifications.clear();
                overdriveTimer = 0; empTimer = 0; empWaveRadius = 0; bossInQueue = false;
                maxTowers = 3; towerFireRate = 0.8f; currentScreen = GAMEPLAY; waveActive = false;
            } break;
        }

        BeginDrawing();
            ClearBackground(V_BLACK);
            BeginMode2D(camera);
                for(int i = -100; i < SCREEN_WIDTH + 100; i += 64) DrawLine(i, -100, i, SCREEN_HEIGHT + 100, {30, 30, 35, 255});
                for(int i = -100; i < SCREEN_HEIGHT + 100; i += 64) DrawLine(-100, i, SCREEN_WIDTH + 100, i, {30, 30, 35, 255});
                if (currentScreen == START_MENU) { for (const auto& ms : menuShapes) DrawPolyLinesEx(ms.pos, ms.sides, ms.size, ms.rotation, 1.5f, ColorAlpha(V_DARKGRAY, 0.4f)); }
                
                if (currentScreen == GAMEPLAY || currentScreen == UPGRADE_MENU || currentScreen == START_MENU) {
                    for(const auto& p : particles) DrawCircle(p.pos.x, p.pos.y, 2, ColorAlpha(p.col, p.life/p.maxLife));
                    if (empWaveRadius > 0) DrawCircleLines(corePos.x, corePos.y, empWaveRadius, ColorAlpha(V_PURPLE, 1.0f - (empWaveRadius/2500.0f)));
                    for (const auto& l : lasers) DrawLineEx(l.start, l.end, 3.0f, overdriveTimer > 0 ? V_GOLD : V_WHITE);
                    for(const auto& p : powerups) {
                        Color pCol = (p.type == PWR_EMP) ? V_PURPLE : (p.type == PWR_OVERDRIVE) ? V_GOLD : V_CYAN;
                        float floatAnim = sinf(GetTime() * 5.0f) * 6.0f;
                        Vector2 drawPos = {p.position.x, p.position.y + floatAnim};
                        DrawPolyLinesEx(drawPos, 4, 18, p.rotation, 2, ColorAlpha(pCol, p.timer/10.0f + 0.3f));
                        DrawRectangleLinesEx({drawPos.x - 8, drawPos.y - 8, 16, 16}, 2, pCol);
                    }
                    if (currentScreen != START_MENU) {
                        DrawCircleLines(corePos.x, corePos.y, EXCLUSION_RADIUS, ColorAlpha(V_RED, 0.3f));
                        DrawCircleLines(corePos.x, corePos.y, CORE_RADIUS, V_CYAN);
                        DrawCircleLines(corePos.x, corePos.y, CORE_RADIUS - 8, V_SKYBLUE);
                        DrawCircle(corePos.x, corePos.y, 4, V_WHITE);
                    }
                    if (currentScreen == GAMEPLAY && !mouseOnUi && towers.size() < (size_t)maxTowers) {
                        bool canPlace = GetDistance(mousePos, corePos) > EXCLUSION_RADIUS;
                        DrawCircleLines(mousePos.x, mousePos.y, towerRange, ColorAlpha(canPlace ? V_WHITE : V_RED, 0.4f));
                        DrawPoly(mousePos, 4, 18, 45, ColorAlpha(canPlace ? V_LIME : V_RED, 0.2f));
                    }
                    for (const auto& t : towers) {
                        DrawCircleLines(t.position.x, t.position.y, towerRange, ColorAlpha(V_SKYBLUE, 0.2f));
                        DrawHealthBody(t.position, 4, 18, 1.0f, V_LIME);
                    }
                    for (const auto& e : enemies) DrawHealthBody(e.position, e.sides, e.radius, e.health/e.maxHealth, empTimer > 0 ? V_PURPLE : V_RED);
                }
            EndMode2D();

            if (damageFlashTimer > 0) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_RED, damageFlashTimer * 1.5f));

            if (currentScreen == START_MENU) {
                DrawText("VECTOR_DEFENSE", SCREEN_WIDTH/2 - MeasureText("VECTOR_DEFENSE", 60)/2, 200, 60, V_CYAN);
                DrawRectangle(SCREEN_WIDTH/2 - 120, 275, 240, 2, V_SKYBLUE);
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 360, 300, 65 }, "BOOT SEQUENCE", V_LIME)) { currentScreen = GAMEPLAY; waveActive = false; }
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 445, 300, 65 }, "SYSTEM GUIDE", V_WHITE)) currentScreen = GUIDE;
            } 
            else if (currentScreen == GUIDE) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText("SYSTEM OPERATIONAL GUIDE", 60, 60, 35, V_SKYBLUE);
                int x1 = 70, x2 = 650, y = 140;
                DrawText("THREAT LOG", x1, y, 22, V_RED);
                DrawText("- Splitting: Complex shapes split into 2 sections", x1, y + 40, 18, V_WHITE);
                DrawText("- BOSS LOG: Prime Polygons appear every 10 waves", x1, y + 65, 18, V_GOLD);
                DrawText("DEFENSE LOG", x1, y + 140, 22, V_LIME);
                DrawText("- Nodes: Automated laser defense", x1, y + 180, 18, V_WHITE);
                DrawText("- Reset: Nodes decommission at wave end", x1, y + 205, 18, V_WHITE);
                DrawText("- Zone: Building prohibited in red-core heart", x1, y + 230, 18, V_WHITE);
                DrawText("POWER-UPS", x2, y, 22, V_GOLD);
                DrawText("- [EMP] Purple: Freezes movement", x2, y + 40, 18, V_PURPLE);
                DrawText("- [OVERDRIVE] Gold: Turbo fire", x2, y + 65, 18, V_GOLD);
                DrawText("- [NANOBOTS] Cyan: Core absorption repair", x2, y + 90, 18, V_SKYBLUE);
                DrawText("CYCLE", x2, y + 170, 22, V_CYAN);
                DrawText("- Build Phase: Strategize & Buy", x2, y + 210, 18, V_WHITE);
                DrawText("- Time: Power-ups freeze during Build phase", x2, y + 235, 18, V_GOLD);
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 100, 620, 200, 50 }, "< RETURN", V_WHITE)) currentScreen = START_MENU;
            }
            else if (currentScreen == GAMEPLAY || currentScreen == UPGRADE_MENU) {
                DrawRectangle(0, 0, SCREEN_WIDTH, UI_HEADER_HEIGHT, ColorAlpha(V_BLACK, 0.95f));
                DrawText(TextFormat("CORE: %d", coreHealth), 25, 20, 22, coreHealth < 5 ? V_RED : V_WHITE);
                DrawText(TextFormat("DATA: %d", currency), 200, 20, 22, V_GOLD);
                DrawText(TextFormat("NODES: %d/%d", (int)towers.size(), maxTowers), 400, 20, 22, V_LIME);
                DrawText(TextFormat("WAVE: %d", currentWave), 600, 20, 22, V_SKYBLUE);
                
                if (waveActive) {
                    const char* threatsText = TextFormat("THREATS REMAINING: %d", (int)enemies.size() + enemiesToSpawn + (bossInQueue?1:0));
                    DrawText(threatsText, 25, SCREEN_HEIGHT - 35, 20, V_SKYBLUE); // Back to bottom-left
                }
                
                for (int i = 0; i < (int)notifications.size(); i++) {
                    float alpha = notifications[i].timer / 2.0f;
                    int tW = MeasureText(notifications[i].text.c_str(), 30);
                    DrawText(notifications[i].text.c_str(), SCREEN_WIDTH/2 - tW/2, 110 + (i * 45), 30, ColorAlpha(notifications[i].col, alpha));
                }

                if (currentScreen == GAMEPLAY && !waveActive && enemies.empty()) {
                    DrawRectangle(0, SCREEN_HEIGHT - UI_FOOTER_HEIGHT, SCREEN_WIDTH, UI_FOOTER_HEIGHT, ColorAlpha(V_BLACK, 0.85f));
                    DrawText("SYSTEM IDLE // BUILD PHASE", 40, SCREEN_HEIGHT - 55, 20, V_SKYBLUE);
                    if (DrawCustomButton({ SCREEN_WIDTH - 550, SCREEN_HEIGHT - 72, 250, 60 }, "OPEN ARMORY [U]", V_GOLD)) currentScreen = UPGRADE_MENU;
                    if (DrawCustomButton({ SCREEN_WIDTH - 280, SCREEN_HEIGHT - 72, 250, 60 }, "START WAVE", V_LIME)) {
                        currentWave++; waveActive = true; enemiesToSpawn = 7 + (currentWave * 6); if (currentWave % 10 == 0) bossInQueue = true;
                    }
                }
                if (currentScreen == UPGRADE_MENU) {
                    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 0, 0, 0, 240 });
                    DrawText("SYSTEM_ARMORY", SCREEN_WIDTH/2 - 110, 80, 32, V_SKYBLUE);
                    DrawText(TextFormat("AVAILABLE DATA: %d", currency), SCREEN_WIDTH/2 - MeasureText(TextFormat("AVAILABLE DATA: %d", currency), 24)/2, 130, 24, V_GOLD);
                    int slotCost = 400 + (maxTowers - 3) * 350; int fireCost = 700 + (int)((0.8f - towerFireRate) * 10000); 
                    if (DrawCustomButton({ SCREEN_WIDTH/2 - 200, 200, 400, 75 }, TextFormat("ADD NODE SLOT (%d)", slotCost), V_LIME)) { if (currency >= slotCost) { currency -= slotCost; maxTowers++; } }
                    if (DrawCustomButton({ SCREEN_WIDTH/2 - 200, 295, 400, 75 }, TextFormat("OVERCLOCK FIRE (%d)", fireCost), V_GOLD)) { if (currency >= fireCost) { currency -= fireCost; towerFireRate *= 0.82f; } }
                    if (DrawCustomButton({ SCREEN_WIDTH/2 - 200, 390, 400, 75 }, "CORE REPAIR (450)", V_CYAN)) { if (currency >= 450 && coreHealth < maxCoreHealth) { currency -= 450; coreHealth = std::min(coreHealth + 6, maxCoreHealth); } }
                    DrawText("PRESS [U] TO DISMISS", SCREEN_WIDTH/2 - 115, 530, 20, V_DARKGRAY);
                }
            }
            else if (currentScreen == GAME_OVER) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 40, 10, 12, 255 });
                DrawText("SYSTEM FAILURE", SCREEN_WIDTH/2 - 180, 80, 45, V_RED);
                int boardY = 180, centerX = SCREEN_WIDTH/2;
                DrawRectangle(centerX - 350, boardY, 700, 320, ColorAlpha(V_BLACK, 0.6f));
                DrawRectangleLines(centerX - 350, boardY, 700, 320, V_DARKGRAY);
                DrawText("MISSION PERFORMANCE LOG", centerX - MeasureText("MISSION PERFORMANCE LOG", 26)/2, boardY + 20, 26, V_SKYBLUE);
                DrawText(TextFormat("TOTAL HARVESTED DATA: %d", score), centerX - 320, boardY + 80, 20, V_WHITE);
                DrawText(TextFormat("FINAL WAVE DEPTH: %d", currentWave), centerX - 320, boardY + 115, 20, V_WHITE);
                DrawText(TextFormat("REMAINING FRAGMENTS: %d", currency), centerX - 320, boardY + 150, 20, V_GOLD);
                DrawText("DEFENSE CONFIGURATION:", centerX + 20, boardY + 80, 20, V_LIME);
                DrawText(TextFormat("- ACTIVE NODE SLOTS: %d", maxTowers), centerX + 20, boardY + 115, 18, V_WHITE);
                DrawText(TextFormat("- LASER RECHARGE: %.2fs", towerFireRate), centerX + 20, boardY + 145, 18, V_WHITE);
                if (DrawCustomButton({ SCREEN_WIDTH/2 - 150, 540, 300, 65 }, "REBOOT SYSTEM", V_GOLD)) {
                    score = 0; currency = 0; currentWave = 0; enemiesToSpawn = 0; coreHealth = maxCoreHealth;
                    towers.clear(); enemies.clear(); lasers.clear(); powerups.clear(); particles.clear(); notifications.clear();
                    maxTowers = 3; towerFireRate = 0.8f; currentScreen = GAMEPLAY; waveActive = false;
                }
            }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
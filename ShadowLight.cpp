// main.cpp
// Простой уровень "Огонь и Вода" на raylib (один файл).
// Controls:
//  Fireboy: A D - left/right, W - jump
//  Watergirl: Left/Right arrows, Up arrow - jump
//  R - restart level, Esc - exit

#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <iostream>

// Параметры мира
static const int TILE_SIZE = 32;
static const float GRAVITY = 1200.0f;   // px/s^2
static const float MOVE_SPEED = 180.0f; // px/s
static const float JUMP_FORCE = 420.0f; // initial jump velocity (px/s)
static const float MAX_FALL_SPEED = 1000.0f;

struct Player {
    Rectangle rect;    // позиция и размеры
    Vector2 vel;       // скорость
    Color color;       // цвет рисования
    Vector2 spawn;     // точка возрождения
    bool onGround = false;
    bool alive = true;
    int leftKey, rightKey, jumpKey;
    int gemsCollected = 0;
    std::string name;
};

// Кристалл
struct Gem {
    Rectangle rect;
    bool isRed;
    bool collected = false;
};

// Простая помощь по ремонту строк уровня: заданный символ -> тайл
enum TileChar {
    TC_EMPTY = '.',
    TC_SOLID = '#',
    TC_WATER = '~',
    TC_LAVA = '^',
    TC_GEM_R = 'r',
    TC_GEM_B = 'b',
    TC_SPAWN_F = 'F',
    TC_SPAWN_W = 'W',
    TC_EXIT_R = 'R',
    TC_EXIT_B = 'B'
};

int main() {
    // --- Задание уровня (символы) ---
    // Можно рисовать произвольные строки разной длины — код автоматически дополнит до нужной ширины.
    std::vector<std::string> rawLevel = {
        "############################",
        "#F..r....#...........b....#",
        "#.......###....~~~........#",
        "#....................^....#",
        "#...###..............^....#",
        "#.............B....R......#",
        "#...~~~..............^....#",
        "#...........###..........#",
        "#W.......................#",
        "############################"
    };

    // вычислить максимальную ширину и высоту
    int rows = (int)rawLevel.size();
    int maxCols = 0;
    for (auto& s : rawLevel) if ((int)s.size() > maxCols) maxCols = (int)s.size();
    if (maxCols < 10) maxCols = 10; // защита

    // подогнать ширину и сделать стенки по краям
    for (int r = 0;r < rows;r++) {
        if ((int)rawLevel[r].size() < maxCols) rawLevel[r] += std::string(maxCols - rawLevel[r].size(), '.');
        // гарантируем грани (левая/правая)
        rawLevel[r][0] = '#';
        rawLevel[r][maxCols - 1] = '#';
    }
    // гарантируем верх/низ полными сплошными стенами
    rawLevel[0] = std::string(maxCols, '#');
    rawLevel[rows - 1] = std::string(maxCols, '#');

    // Конфигурация окна
    int screenW = maxCols * TILE_SIZE;
    int screenH = rows * TILE_SIZE;
    InitWindow(screenW, screenH, "Fire & Water - simple level (raylib)");
    SetTargetFPS(60);

    // Списки тайлов и объектов
    std::vector<Rectangle> solids;
    std::vector<Rectangle> waterTiles;
    std::vector<Rectangle> lavaTiles;
    std::vector<Gem> gems;
    Rectangle exitRed = { 0,0,0,0 };
    Rectangle exitBlue = { 0,0,0,0 };
    bool hasExitRed = false, hasExitBlue = false;

    // Игроки
    Player fireboy;
    Player watergirl;
    fireboy.color = RED;
    watergirl.color = BLUE;
    fireboy.leftKey = KEY_A; fireboy.rightKey = KEY_D; fireboy.jumpKey = KEY_W;
    watergirl.leftKey = KEY_LEFT; watergirl.rightKey = KEY_RIGHT; watergirl.jumpKey = KEY_UP;
    fireboy.name = "Fireboy";
    watergirl.name = "Watergirl";

    // Парсинг уровня
    for (int r = 0;r < rows;r++) {
        for (int c = 0;c < maxCols;c++) {
            char ch = rawLevel[r][c];
            float x = c * TILE_SIZE;
            float y = r * TILE_SIZE;
            Rectangle tileRect = { x, y, (float)TILE_SIZE, (float)TILE_SIZE };

            if (ch == TC_SOLID) {
                solids.push_back(tileRect);
            }
            else if (ch == TC_WATER) {
                waterTiles.push_back(tileRect);
            }
            else if (ch == TC_LAVA) {
                lavaTiles.push_back(tileRect);
            }
            else if (ch == TC_GEM_R) {
                Gem g; g.rect = { x + 6.0f, y + 6.0f, (float)TILE_SIZE - 12.0f, (float)TILE_SIZE - 12.0f }; g.isRed = true;
                gems.push_back(g);
            }
            else if (ch == TC_GEM_B) {
                Gem g; g.rect = { x + 6.0f, y + 6.0f, (float)TILE_SIZE - 12.0f, (float)TILE_SIZE - 12.0f }; g.isRed = false;
                gems.push_back(g);
            }
            else if (ch == TC_SPAWN_F) {
                fireboy.spawn = { x + (TILE_SIZE - 24) / 2.0f, y + (TILE_SIZE - 28) / 2.0f };
            }
            else if (ch == TC_SPAWN_W) {
                watergirl.spawn = { x + (TILE_SIZE - 24) / 2.0f, y + (TILE_SIZE - 28) / 2.0f };
            }
            else if (ch == TC_EXIT_R) {
                exitRed = tileRect; hasExitRed = true;
            }
            else if (ch == TC_EXIT_B) {
                exitBlue = tileRect; hasExitBlue = true;
            }
        }
    }

    // Размер персонажей (чуть меньше тайла для корректных пересечений)
    fireboy.rect = { fireboy.spawn.x, fireboy.spawn.y, 24.0f, 28.0f };
    watergirl.rect = { watergirl.spawn.x, watergirl.spawn.y, 24.0f, 28.0f };

    // Защита - если спавн не задан в уровне, поместим их в дефолтные позиции
    if (fireboy.spawn.x == 0 && fireboy.spawn.y == 0) {
        fireboy.spawn = { TILE_SIZE + 4.0f, TILE_SIZE + 4.0f };
        fireboy.rect.x = fireboy.spawn.x; fireboy.rect.y = fireboy.spawn.y;
    }
    if (watergirl.spawn.x == 0 && watergirl.spawn.y == 0) {
        watergirl.spawn = { 2 * TILE_SIZE + 8.0f, TILE_SIZE + 4.0f };
        watergirl.rect.x = watergirl.spawn.x; watergirl.rect.y = watergirl.spawn.y;
    }

    bool levelComplete = false;
    float finishTimer = 0.0f;

    // Главный цикл
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f; // защита от больших дельт

        // Рестарт уровня
        if (IsKeyPressed(KEY_R)) {
            // вернуть игроков на точки спавна, сбросить кристаллы
            fireboy.rect.x = fireboy.spawn.x; fireboy.rect.y = fireboy.spawn.y; fireboy.vel = { 0,0 }; fireboy.alive = true; fireboy.onGround = false; fireboy.gemsCollected = 0;
            watergirl.rect.x = watergirl.spawn.x; watergirl.rect.y = watergirl.spawn.y; watergirl.vel = { 0,0 }; watergirl.alive = true; watergirl.onGround = false; watergirl.gemsCollected = 0;
            for (auto& g : gems) g.collected = false;
            levelComplete = false; finishTimer = 0.0f;
        }

        // --- Обновление игроков (одинаково для обоих) ---
        auto updatePlayer = [&](Player& p, bool isFire) {
            if (!p.alive) {
                // простое мгновенное возрождение (можно добавить таймер)
                p.rect.x = p.spawn.x; p.rect.y = p.spawn.y;
                p.vel = { 0,0 };
                p.alive = true;
                p.onGround = false;
            }

            // Горизонтальное движение
            float desiredVX = 0.0f;
            if (IsKeyDown(p.leftKey)) desiredVX -= MOVE_SPEED;
            if (IsKeyDown(p.rightKey)) desiredVX += MOVE_SPEED;
            p.vel.x = desiredVX;

            // Прыжок (только при нажатии)
            if (IsKeyPressed(p.jumpKey) && p.onGround) {
                p.vel.y = -JUMP_FORCE;
                p.onGround = false;
            }

            // Гравитация
            p.vel.y += GRAVITY * dt;
            if (p.vel.y > MAX_FALL_SPEED) p.vel.y = MAX_FALL_SPEED;

            // Перемещение X и столкновения с твердыми блоками
            p.rect.x += p.vel.x * dt;
            for (auto& s : solids) {
                if (CheckCollisionRecs(p.rect, s)) {
                    // Мы столкнулись по X
                    if (p.vel.x > 0) {
                        p.rect.x = s.x - p.rect.width - 0.001f;
                    }
                    else if (p.vel.x < 0) {
                        p.rect.x = s.x + s.width + 0.001f;
                    }
                    p.vel.x = 0;
                }
            }

            // Перемещение Y и столкновения
            p.rect.y += p.vel.y * dt;
            bool collidedY = false;
            for (auto& s : solids) {
                if (CheckCollisionRecs(p.rect, s)) {
                    collidedY = true;
                    if (p.vel.y > 0) {
                        // падаем — ставим на поверхность
                        p.rect.y = s.y - p.rect.height - 0.001f;
                        p.onGround = true;
                    }
                    else if (p.vel.y < 0) {
                        // уперлись сверху
                        p.rect.y = s.y + s.height + 0.001f;
                    }
                    p.vel.y = 0;
                }
            }
            if (!collidedY) p.onGround = false;

            // Проверка столкновения с опасностями (вода/лава)
            // Fireboy умирает в воде, Watergirl умирает в лаве
            if (isFire) {
                for (auto& w : waterTiles) if (CheckCollisionRecs(p.rect, w)) { p.alive = false; return; }
            }
            else {
                for (auto& l : lavaTiles) if (CheckCollisionRecs(p.rect, l)) { p.alive = false; return; }
            }

            // Сбор кристаллов
            for (auto& g : gems) {
                if (!g.collected && CheckCollisionRecs(p.rect, g.rect)) {
                    if ((g.isRed && isFire) || (!g.isRed && !isFire)) {
                        g.collected = true;
                        p.gemsCollected++;
                    }
                }
            }
            };

        // Обновляем обоих игроков
        updatePlayer(fireboy, true);
        updatePlayer(watergirl, false);

        // Проверка выхода — оба в своих выходах -> победа
        bool fireInExit = false, waterInExit = false;
        if (hasExitRed) {
            if (CheckCollisionRecs(fireboy.rect, exitRed)) fireInExit = true;
        }
        if (hasExitBlue) {
            if (CheckCollisionRecs(watergirl.rect, exitBlue)) waterInExit = true;
        }
        if (fireInExit && waterInExit) {
            if (!levelComplete) {
                levelComplete = true;
                finishTimer = 0.0f;
            }
        }

        if (levelComplete) finishTimer += dt;

        // --- Рендер ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Рисуем тайлы
        for (auto& s : solids) DrawRectangleRec(s, GRAY);
        for (auto& w : waterTiles) DrawRectangleRec(w, Color{ 100,180,255,255 }); // голубой
        for (auto& l : lavaTiles) DrawRectangleRec(l, Color{ 255,120,60,255 });    // оранж

        // рисуем выходы
        if (hasExitRed)  DrawRectangleRec(exitRed, Color{ 190,40,40,255 });
        if (hasExitBlue) DrawRectangleRec(exitBlue, Color{ 40,60,190,255 });

        // рисуем кристаллы
        for (auto& g : gems) {
            if (!g.collected) {
                Color gc = g.isRed ? Color{ 220,40,40,255 } : Color{ 40,120,220,255 };
                DrawRectangleRec(g.rect, gc);
                // небольшой блеск
                DrawCircle((int)(g.rect.x + g.rect.width * 0.5f),
                    (int)(g.rect.y + g.rect.height * 0.5f),
                    3.0f, WHITE);
            }
        }


        // Рисуем игроков
        DrawRectangleRec(fireboy.rect, fireboy.color);
        DrawRectangleRec(watergirl.rect, watergirl.color);

        // HUD: собранные кристаллы
        DrawText(TextFormat("Fireboy (A D W) gems: %d", fireboy.gemsCollected), 10, 8, 16, BLACK);
        DrawText(TextFormat("Watergirl (<- -> Up) gems: %d", watergirl.gemsCollected), 300, 8, 16, BLACK);

        // Инструкции
        DrawText("R - restart  |  ESC - quit", 10, screenH - 22, 14, DARKGRAY);

        // Состояния
        if (!fireboy.alive) {
            DrawText("Fireboy died! Respawning...", screenW / 2 - 120, 40, 14, RED);
        }
        if (!watergirl.alive) {
            DrawText("Watergirl died! Respawning...", screenW / 2 - 120, 60, 14, BLUE);
        }

        if (levelComplete) {
            DrawRectangle(0, screenH / 2 - 40, screenW, 80, Fade(GREEN, 0.15f));
            DrawText("LEVEL COMPLETE! 🎉", screenW / 2 - 160, screenH / 2 - 12, 32, DARKGREEN);
            DrawText("Press R to replay", screenW / 2 - 100, screenH / 2 + 20, 16, DARKGRAY);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

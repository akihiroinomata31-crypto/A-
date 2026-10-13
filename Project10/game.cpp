#include "game.h"
#include "main.h"
#include "player.h"
#include <math.h>
#include <string.h>
#include "../DXLib/DxLib.h"

int stagedata = -1;
int enemy_anim_attack = -1;
int redGoblinBaseModel = -1;
int red_goblin_anim_neutral = -1;
int red_goblin_anim_attack = -1;
int red_goblin_anim_walk = -1;
int weaponBaseModel = -1;

bool PlaceEnemyOnGround(SCharaInfo& enemy) {
    if (stagedata < 0) return false;
    const auto hit = MV1CollCheck_Line(stagedata, -1,
        VGet(enemy.pos.x, 2000.0f, enemy.pos.z), VGet(enemy.pos.x, -1000.0f, enemy.pos.z));
    if (!hit.HitFlag) return false;
    enemy.pos.y = hit.HitPosition.y;
    enemy.move.y = 0.0f;
    enemy.charahitinfo.CenterPosition = enemy.pos;
    if (enemy.model1 >= 0) MV1SetPosition(enemy.model1, enemy.pos);
    return true;
}

void GameManager::Init() {
    mainTimer = 100 * 120; // 120FPS基準
    goblinSpawnTimer = 0;
    redSpawnTimer = 0;
    p1Score = 0;
    p2Score = 0;
    gameState = 0;
    deathCount[0] = 0;
    deathCount[1] = 0;
    killCount[0] = 0;
    killCount[1] = 0;
    specialUseCount[0] = 0;
    specialUseCount[1] = 0;

    for (int i = 0; i < 20; i++) {
        popups[i].active = false;
    }
}

void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // ==========================================
    // 1. タイマー更新 ＆ 勝敗判定（3段階ルール）
    // ==========================================
    if (mainTimer > 0) {
        mainTimer--;
    }
    else if (gameState == 0) {
        // ① スコア比較
        if (p1Score > p2Score) {
            gameState = 1;
        }
        else if (p2Score > p1Score) {
            gameState = 2;
        }
        else {
            // ② 死亡数比較（少ない方が勝ち）
            if (deathCount[0] < deathCount[1]) {
                gameState = 1;
            }
            else if (deathCount[1] < deathCount[0]) {
                gameState = 2;
            }
            else {
                // ③ 撃破数比較（多い方が勝ち）
                if (killCount[0] > killCount[1]) {
                    gameState = 1;
                }
                else if (killCount[1] > killCount[0]) {
                    gameState = 2;
                }
                else {
                    gameState = 3; // 完全同一なら引き分け
                }
            }
        }
    }

    // ==========================================
    // 2. 敵スポーン処理（mainTimer とは無関係に動作）
    // ==========================================
    int activeEnemyCount = 0;
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode != NONE) {
            activeEnemyCount++;
        }
    }

    // 同時生存数の上限
    const int MAX_FIELD_ENEMIES = 200;

    if (activeEnemyCount < MAX_FIELD_ENEMIES) {

        // --- 通常ゴブリン ---
        goblinSpawnTimer++;
        if (goblinSpawnTimer >= 280) {
            goblinSpawnTimer = 0;

            for (int k = 0; k < 15; k++) {
                float spawnX = (float)(GetRand(4500) - 2250);
                float spawnZ = (float)(GetRand(4500) - 2250);
                ActivateEnemy(enemyList, spawnX, spawnZ);
            }
        }

        // --- 赤ゴブリン ---
        redSpawnTimer++;
        if (redSpawnTimer >= 500) {
            redSpawnTimer = 0;
            for (int j = 0; j < 3; j++) {
                float spawnX = (float)(GetRand(3000) - 1500);
                float spawnZ = (float)(GetRand(3000) - 1500);
                ActivateRedGoblin(enemyList, spawnX, spawnZ);
            }
        }
    }

    // 特殊ゴーレムの出現条件（残り約50秒以下）
    int seconds = mainTimer / 120;
    if (seconds <= 50 && gameState == 0) {
        if (enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
            enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 30.0f, -150.0f);
            enemyList[TEST_ENEMY_GOLEM].mode = STAND;
            enemyList[TEST_ENEMY_GOLEM].enemyHP = 10;
            enemyList[TEST_ENEMY_GOLEM].playtime = 0.0f;

            MV1SetPosition(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].pos);
            MV1SetVisible(enemyList[TEST_ENEMY_GOLEM].model1, TRUE);

            if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                MV1DetachAnim(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
            }
            enemyList[TEST_ENEMY_GOLEM].attachidx = MV1AttachAnim(enemyList[TEST_ENEMY_GOLEM].model1, 0, enemy_anim_neutral);
            if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                enemyList[TEST_ENEMY_GOLEM].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
            }
        }
    }
}

void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }
            if (enemyList[i].model1 == -1) continue;

            enemyList[i].pos = VGet(x, 0.0f, z);

            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);
            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            enemyList[i].pos.y = baseFloorY + 600.0f;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 5;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }
            enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, red_goblin_anim_neutral);
            if (enemyList[i].attachidx != -1) {
                enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
            }
            break;
        }
    }
}

void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);

            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);
            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            enemyList[i].pos.y = baseFloorY + 600.0f;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 2;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }
            enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, enemy_anim_neutral);
            if (enemyList[i].attachidx != -1) {
                enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
            }
            break;
        }
    }
}

void GameManager::RecordFrame(VECTOR p1, VECTOR p2, int act) {
    ReplayFrame frame;
    frame.pos[0] = p1;
    frame.pos[1] = p2;
    frame.action = act;
    replayData.push_back(frame);
}

void GameManager::DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex, int crownGraphHandle) {
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int halfW = sw / 2;
    SetFontSize(20);

    // 決着テキスト
    if (gameState != 0) {
        SetFontSize(60);
        int color = GetColor(255, 255, 0);
        if (gameState == 1)      DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        else if (gameState == 2) DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
        else                     DrawString(xOffset + 100, sh / 2 - 30, "DRAW", color);
        SetFontSize(20);
    }

    // ==========================================
    // 画面左上：プレイヤー名 ＆ HPバー（幅300に拡大）
    // ==========================================
    int startY = 20;
    unsigned int pColor = (pIdx == 0) ? GetColor(100, 200, 255) : GetColor(255, 150, 100);
    DrawFormatString(xOffset + 20, startY, pColor, " PLAYER %d ", pIdx + 1);

    int frameDrawW = 300; // ★ 横幅を300に拡大
    int frameDrawH = 24;
    int drawX = xOffset + 20;
    int drawY = startY + 30;

    float hpRate = (float)players[pIdx].HP / (float)MAX_HP;
    if (hpRate < 0.0f) hpRate = 0.0f;
    if (hpRate > 1.0f) hpRate = 1.0f;

    int innerMargin = 2;
    DrawExtendGraph(drawX, drawY, drawX + frameDrawW, drawY + frameDrawH, hpBarTex, TRUE);
    DrawBox(drawX + innerMargin, drawY + innerMargin,
        drawX + frameDrawW - innerMargin, drawY + frameDrawH - innerMargin,
        GetColor(200, 0, 0), TRUE);

    int innerMaxWidth = frameDrawW - (innerMargin * 2);
    int currentGreenWidth = (int)(innerMaxWidth * hpRate);

    if (currentGreenWidth > 0) {
        if (hpRate <= 0.3f) SetDrawBright(255, 100, 100);
        else                SetDrawBright(100, 255, 100);

        DrawBox(drawX + innerMargin, drawY + innerMargin,
            drawX + innerMargin + currentGreenWidth, drawY + frameDrawH - innerMargin,
            GetColor(0, 255, 0), TRUE);

        SetDrawBright(255, 255, 255);
    }

    int currentDeaths = (pIdx == 0) ? deathCount[0] : deathCount[1];
    int infoY = drawY + 32;
    DrawFormatString(xOffset + 20, infoY, GetColor(255, 100, 200), "DEATHS: %d", currentDeaths);

    // 残り15秒以下のフィーバー表示
    int seconds = mainTimer / 120;
    if (gameState == 0 && seconds <= 15 && seconds > 0) {
        int feverColor = ((mainTimer / 15) % 2 == 0) ? GetColor(255, 50, 50) : GetColor(255, 220, 0);
        SetFontSize(16);
        DrawString(xOffset + frameDrawW + 30, startY + 4, "FEVER! x2", feverColor);
        SetFontSize(20);
    }

    // ==========================================
    // ★ スコア表示（黒色文字・1P左下／2P右下・30秒以下で「???」）
    // ==========================================
    int currentScore = (pIdx == 0) ? p1Score : p2Score;
    char scoreStr[32];
    if (gameState == 0 && seconds <= 30 && seconds > 0) {
        sprintf_s(scoreStr, "Score: ???");
    }
    else {
        sprintf_s(scoreStr, "Score: %d", currentScore);
    }

    int scoreTextW = 0;
    if (fontScore != -1) {
        scoreTextW = GetDrawStringWidthToHandle(scoreStr, (int)strlen(scoreStr), fontScore);
    }
    else {
        scoreTextW = GetDrawStringWidth(scoreStr, (int)strlen(scoreStr));
    }

    int scoreX = 0;
    int scoreY = sh - 65; // 画面底から約65px上

    if (pIdx == 0) {
        scoreX = xOffset + 25;                       // 1P: 左下
    }
    else {
        scoreX = xOffset + halfW - scoreTextW - 30;  // 2P: 右下
    }

    // 黒色文字で描画（視認性確保のため輪郭を白に指定）
    if (fontScore != -1) {
        DrawStringToHandle(scoreX, scoreY, scoreStr, GetColor(0, 0, 0), fontScore, GetColor(255, 255, 255));
    }
    else {
        SetFontSize(24);
        DrawString(scoreX, scoreY, scoreStr, GetColor(0, 0, 0));
        SetFontSize(20);
    }

    // クラウン（1位マーク）：1Pはスコア右、2Pはスコア左
    if (p1Score >= 1000 || p2Score >= 1000) {
        int leaderIdx = (p2Score > p1Score) ? 1 : 0;
        if (pIdx == leaderIdx) {
            int crownX = (pIdx == 0) ? (scoreX + scoreTextW + 25) : (scoreX - 25);
            int crownY = scoreY + 14;
            SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
            DrawRotaGraph(crownX, crownY, 0.3f, 0.0f, crownGraphHandle, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    // ==========================================
    // スコア加算ポップアップ（各スコア位置の上へ浮上）
    // ==========================================
    for (int i = 0; i < 20; i++) {
        if (popups[i].active && popups[i].playerIdx == pIdx) {
            int popupBaseX = scoreX + 30;
            int popupBaseY = scoreY - 10 - (int)popups[i].offsetY;

            int alpha = 255;
            if (popups[i].timer < 20) {
                alpha = (popups[i].timer * 255) / 20;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            SetFontSize(22);

            if (gameState == 0 && seconds <= 30 && seconds > 0) {
                DrawFormatString(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+?");
            }
            else {
                if (popups[i].score > 0) {
                    DrawFormatString(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+%d", popups[i].score);
                }
                else {
                    DrawFormatString(popupBaseX, popupBaseY, GetColor(255, 0, 0), "%d", popups[i].score);
                }
            }

            SetFontSize(20);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            popups[i].offsetY += 0.5f;
            popups[i].timer--;
            if (popups[i].timer <= 0) {
                popups[i].active = false;
            }
        }
    }
}

// ==========================================
// タイマー専用描画（文字のみ・中央配置）
// ==========================================
void GameManager::DrawTimer(int sw, int sh) {
    int seconds = mainTimer / 120;
    if (seconds > 200) seconds = 200;
    if (seconds < 0)   seconds = 0;

    unsigned int timerColor;
    if (seconds <= 10 && (mainTimer / 15) % 2 == 0) {
        timerColor = GetColor(255, 50, 50); // 10秒以下で点滅
    }
    else if (seconds <= 30) {
        timerColor = GetColor(255, 80, 80);
    }
    else {
        timerColor = GetColor(255, 255, 0);
    }

    SetFontSize(30);

    char timerStr[32];
    if (seconds > 0) {
        sprintf_s(timerStr, "%02d", seconds);
    }
    else {
        sprintf_s(timerStr, "FINISH!");
    }

    int textWidth = GetDrawStringWidth(timerStr, (int)strlen(timerStr));
    int drawX = sw / 2 - textWidth / 2;
    int drawY = 15;

    DrawString(drawX, drawY, timerStr, timerColor);
    SetFontSize(20);
}

void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;
        if (p1Score < 0) p1Score = 0;
    }
    else {
        p2Score += score;
        if (p2Score < 0) p2Score = 0;
    }
}

void GameManager::AddScorePopup(int playerIdx, int score) {
    for (int i = 0; i < 20; i++) {
        if (!popups[i].active) {
            popups[i].score = score;
            popups[i].playerIdx = playerIdx;
            popups[i].offsetY = 0.0f;
            popups[i].timer = 60;
            popups[i].active = true;
            break;
        }
    }
}

// ============================================================================
// メニュー・リソース管理処理（game.cpp 組み込み用）
// ============================================================================

static int MenuFont(int size) {
    static int fontSmall = -1, label = -1, heading = -1, title = -1;
    int* handle = size <= 17 ? &fontSmall : size <= 25 ? &label : size <= 40 ? &heading : &title;
    if (*handle < 0) *handle = CreateFontToHandle("Yu Gothic", size <= 17 ? 16 : size <= 25 ? 24 : size <= 40 ? 36 : 54, 2, DX_FONTTYPE_ANTIALIASING_8X8);
    return *handle;
}

static void MenuLabel(int x, int y, int size, const char* text, unsigned int color) {
    DrawStringToHandle(x, y, text, color, MenuFont(size));
}

static void MenuText(int y, int size, const char* text, unsigned int color) {
    const int font = MenuFont(size);
    DrawStringToHandle((900 - GetDrawStringWidthToHandle(text, (int)strlen(text), font)) / 2, y, text, color, font);
}

void DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot) {
    SetDrawArea(0, 0, 900, 600);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    ClearDrawScreen();

    const auto gold = GetColor(222, 194, 137), white = GetColor(235, 236, 229), muted = GetColor(127, 145, 153);

    for (int y = 0; y < 600; ++y) {
        const float t = y / 600.0f;
        DrawLine(0, y, 900, y, GetColor(8 + (int)(6 * t), 17 + (int)(4 * t), 25 + (int)(5 * t)));
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 22);
    for (int r = 295; r > 20; r -= 14) DrawCircle(675, 240, r, GetColor(41, 73, 78), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 65);
    DrawCircle(675, 240, 145, gold, FALSE);
    DrawCircle(675, 240, 157, GetColor(72, 109, 116), FALSE);
    DrawLine(675, 62, 675, 418, GetColor(68, 105, 111));
    DrawLine(500, 240, 850, 240, GetColor(68, 105, 111));
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawTriangle(675, 92, 653, 271, 675, 292, GetColor(165, 184, 183), TRUE);
    DrawTriangle(675, 92, 675, 292, 697, 271, GetColor(75, 104, 112), TRUE);
    DrawLine(675, 95, 675, 289, GetColor(220, 231, 223), 2);
    DrawLine(630, 293, 675, 283, gold, 5);
    DrawLine(675, 283, 720, 293, gold, 5);
    DrawBox(669, 292, 681, 345, GetColor(46, 59, 65), TRUE);
    for (int y = 302; y < 342; y += 10) DrawLine(670, y, 680, y, gold);
    DrawCircle(675, 351, 8, gold, TRUE);

    const float drift = GetNowCount() * 0.00015f;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 65);
    for (int i = 0; i < 22; ++i) {
        const int x = 440 + (i * 83 % 440);
        const int y = 60 + (int)fmodf(i * 47.0f + drift * 15, 450.0f);
        DrawCircle(x, y, i % 4 == 0 ? 2 : 1, gold, TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    if (!results) {
        MenuLabel(108, 167, 54, "限界", white);
        MenuLabel(108, 230, 54, "ラッシュ", white);
        DrawLine(112, 320, 165, 320, gold, 2);
    }
    else {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
        DrawBox(80, 65, 820, 416, GetColor(10, 21, 29), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        MenuText(85, 36, "試合結果", gold);
        MenuText(139, 36, game.gameState == 1 ? "プレイヤー１の勝利" : game.gameState == 2 ? "プレイヤー２の勝利" : "引き分け", white);
        DrawLine(120, 205, 780, 205, GetColor(53, 71, 78));
        MenuLabel(180, 229, 24, "プレイヤー１", GetColor(131, 187, 207));
        MenuLabel(525, 229, 24, "プレイヤー２", GetColor(219, 165, 140));
        char text[80];
        for (int p = 0; p < 2; ++p) {
            const int x = p == 0 ? 180 : 525;
            const int earned = p == 0 ? game.p1Score : game.p2Score;
            const int score = game.FinalScore(p);
            const int deathCost = game.deathCount[p] * GameManager::DeathPenalty;
            const int specialCost = game.specialUseCount[p] * GameManager::SpecialPenalty;
            sprintf_s(text, "獲得スコア   %d", earned); MenuLabel(x, 271, 16, text, muted);
            sprintf_s(text, "死亡 %d回   -%d", game.deathCount[p], deathCost); MenuLabel(x, 301, 16, text, muted);
            sprintf_s(text, "必殺技 %d回   -%d", game.specialUseCount[p], specialCost); MenuLabel(x, 331, 16, text, muted);
            sprintf_s(text, "合計   %d", score); MenuLabel(x, 366, 24, text, white);
        }
    }

    const int left = results ? 290 : 110, width = 320;
    for (int row = 0; row < 2; ++row) {
        const int y = (results ? 429 : 387) + row * 65;
        if (row == selected) {
            for (int x = 0; x < width; ++x) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 - x * 75 / width);
                DrawLine(left + x, y, left + x, y + 49, GetColor(81, 99, 107));
            }
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawBox(left, y, left + 3, y + 49, gold, TRUE);
            DrawLine(left, y + 49, left + width, y + 49, GetColor(77, 90, 92));
        }
        MenuLabel(left + 25, y + 8, 24, row == 1 ? "終了" : results ? "メニューに戻る" : "ゲーム開始", row == selected ? gold : muted);
        if (row == selected) MenuLabel(left + width - 32, y + 8, 24, ">", gold);
    }
    DrawLine(110, 548, 790, 548, GetColor(34, 51, 60));
    MenuText(566, 16, "↑↓ 選択     A / Enter 決定", muted);
    if (snapshot) SaveDrawScreenToPNG(0, 0, 900, 600, snapshot);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetFontSize(20);
    ScreenFlip();
}

void GameManager::DrawSpecialGauge(int pIdx, int startX, int endX, int screenHeight, int currentGauge, int maxGauge, const int* spHandles, int imageCount) {
    if (!spHandles || imageCount <= 0) return;

    const int frameW = 210;
    const int frameH = 22;

    int gaugeX = endX - frameW - 20;
    int gaugeY = screenHeight - frameH - 25;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 85);
    DrawBox(gaugeX + 6, gaugeY + 3, gaugeX + frameW - 6, gaugeY + frameH - 3, GetColor(5, 10, 15), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    float rate = (float)currentGauge / (float)maxGauge;
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    int imgIdx = (int)(rate * (imageCount - 1));
    if (imgIdx < 0) imgIdx = 0;
    if (imgIdx >= imageCount) imgIdx = imageCount - 1;

    if (spHandles[imgIdx] != -1) {
        SetDrawBlendMode(DX_BLENDMODE_PMA_ALPHA, 240);
        DrawExtendGraph(gaugeX, gaugeY, gaugeX + frameW, gaugeY + frameH, spHandles[imgIdx], TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    if (currentGauge >= maxGauge) {
        int alpha = 180 + (int)(sinf(GetNowCount() * 0.007f) * 75);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        SetFontSize(15);
        DrawString(gaugeX + 80, gaugeY - 18, "READY!", GetColor(255, 210, 30));
        SetFontSize(20);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}
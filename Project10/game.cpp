#include "game.h"
#include "main.h"
#include "player.h"
#include <math.h>
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
    mainTimer = 99 * 60;
    goblinSpawnTimer = 0;
    redSpawnTimer = 0;
    p1Score = 0;
    p2Score = 0;
    gameState = 0;
    deathCount[0] = 0;
    deathCount[1] = 0;
    specialUseCount[0] = specialUseCount[1] = 0;

    for (int i = 0; i < 20; i++) {
        popups[i].active = false;
    }
}

void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // ==========================================
    // 1. メイン制限時間タイマー（他から完全に独立）
    // ==========================================
    if (mainTimer > 0) {
        mainTimer--;
    }
    else if (gameState == 0) {
        if (FinalScore(0) > FinalScore(1))      gameState = 1;
        else if (FinalScore(1) > FinalScore(0)) gameState = 2;
        else                        gameState = 3;
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

    // 同時生存数の上限（無双向けに35体程度をキープ）
    const int MAX_FIELD_ENEMIES = 200;

    if (activeEnemyCount < MAX_FIELD_ENEMIES) {

        // Normal goblins: three per second at 60 FPS.
        goblinSpawnTimer++;
        if (goblinSpawnTimer >= 60) {
            goblinSpawnTimer = 0;

            for (int k = 0; k < 3 && activeEnemyCount < MAX_FIELD_ENEMIES; k++, activeEnemyCount++) {
                float spawnX = (float)(GetRand(4500) - 2250);
                float spawnZ = (float)(GetRand(4500) - 2250);
                ActivateEnemy(enemyList, spawnX, spawnZ);
            }
        }

        // Red goblins: one every three seconds at 60 FPS.
        redSpawnTimer++;
        if (redSpawnTimer >= 180 && activeEnemyCount < MAX_FIELD_ENEMIES) {
            redSpawnTimer = 0;

            float spawnX = (float)(GetRand(3000) - 1500);
            float spawnZ = (float)(GetRand(3000) - 1500);
            ActivateRedGoblin(enemyList, spawnX, spawnZ);
        }
    }

    // 特殊ゴーレムの出現条件（残り約50秒以下）
    int seconds = mainTimer / 60;
    if (seconds <= 50 && gameState == 0) {
        if (enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
            enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 30.0f, -150.0f);
            if (!PlaceEnemyOnGround(enemyList[TEST_ENEMY_GOLEM])) return;
            enemyList[TEST_ENEMY_GOLEM].mode = STAND;
            enemyList[TEST_ENEMY_GOLEM].enemyHP = 10;
            enemyList[TEST_ENEMY_GOLEM].playtime = 0.0f;

            MV1SetPosition(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].pos);
            MV1SetVisible(enemyList[TEST_ENEMY_GOLEM].model1, TRUE);

            if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                MV1DetachAnim(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
            }
            enemyList[TEST_ENEMY_GOLEM].attachidx = MV1AttachAnim(enemyList[TEST_ENEMY_GOLEM].model1, 0, enemyList[TEST_ENEMY_GOLEM].enemyIdleAnimation);
            if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                enemyList[TEST_ENEMY_GOLEM].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
            }
        }
    }
}

void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
        if (i == TEST_ENEMY_GOLEM) continue;
        if (enemyList[i].mode == NONE) {
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }
            if (enemyList[i].model1 == -1) continue;

            enemyList[i].pos = VGet(x, 0.0f, z);

            if (!PlaceEnemyOnGround(enemyList[i])) continue;
            enemyList[i].enemyWalking = false;
            enemyList[i].currentAnimType = 1;
            enemyList[i].invincibleTimer = 0;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 5;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }
            enemyList[i].enemyIdleAnimation = red_goblin_anim_neutral;
            enemyList[i].enemyWalkAnimation = red_goblin_anim_walk;
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
        if (i == TEST_ENEMY_GOLEM) continue;
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);

            if (!PlaceEnemyOnGround(enemyList[i])) continue;
            enemyList[i].enemyWalking = false;
            enemyList[i].currentAnimType = 1;
            enemyList[i].invincibleTimer = 0;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 2;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }
            enemyList[i].enemyIdleAnimation = enemy_anim_neutral;
            enemyList[i].enemyWalkAnimation = enemy_anim_walk;
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
    SetFontSize(20);

    if (gameState != 0) {
        SetFontSize(60);
        int color = GetColor(255, 255, 0);
        if (gameState == 1)      DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        else if (gameState == 2) DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
        else                     DrawString(xOffset + 100, sh / 2 - 30, "DRAW", color);
        SetFontSize(20);
    }

    int startY = 20;
    unsigned int pColor = (pIdx == 0) ? GetColor(100, 200, 255) : GetColor(255, 150, 100);
    DrawFormatString(xOffset + 20, startY, pColor, " PLAYER %d ", pIdx + 1);

    int frameDrawW = 180;
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

    int currentScore = (pIdx == 0) ? p1Score : p2Score;
    int currentDeaths = (pIdx == 0) ? deathCount[0] : deathCount[1];

    int infoY = drawY + 32;
    DrawFormatString(xOffset + 20, infoY, GetColor(255, 215, 0), "Score: %d", currentScore);
    DrawFormatString(xOffset + 140, infoY, GetColor(255, 100, 200), "DEATHS: %d", currentDeaths);

    if (p1Score >= 1000 || p2Score >= 1000) {
        int leaderIdx = (p2Score > p1Score) ? 1 : 0;
        if (pIdx == leaderIdx) {
            int crownX = xOffset + 220;
            int crownY = drawY + 12;
            SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
            DrawRotaGraph(crownX, crownY, 0.3f, 0.0f, crownGraphHandle, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    for (int i = 0; i < 20; i++) {
        if (popups[i].active && popups[i].playerIdx == pIdx) {
            int popupBaseX = xOffset + 20 + 180 + 70;
            int popupBaseY = startY + 30 + 2 - (int)popups[i].offsetY;

            int alpha = 255;
            if (popups[i].timer < 20) {
                alpha = (popups[i].timer * 255) / 20;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            SetFontSize(24);

            if (popups[i].score > 0) {
                DrawFormatString(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+%d", popups[i].score);
            }
            else {
                DrawFormatString(popupBaseX, popupBaseY, GetColor(255, 0, 0), "%d", popups[i].score);
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
    // 60fps固定なら 60 で割る
    int seconds = mainTimer / 120;
    if (seconds > 200) seconds = 200;
    if (seconds < 0)  seconds = 0;

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

    // 下地ボックスなし、文字のみ描画
    DrawString(drawX, drawY, timerStr, timerColor);

    SetFontSize(50);
}

void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;

    }
    else {
        p2Score += score;

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



#include "game.h"
#include "main.h"
#include "player.h"

int stagedata;
int enemy_anim_attack;
int enemy_anim_neutral;
int red_goblin_anim_neutral;
int red_goblin_anim_attack;
int redGoblinSpawnTimer;
int redGoblinBaseModel;

void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    if (timeLimit > 0) {
        timeLimit--;
    }
    else if (gameState == 0) {
        if (p1Score > p2Score) {
            gameState = 1;
        }
        else if (p2Score > p1Score) {
            gameState = 2;
        }
        else {
            gameState = 3;
        }
    }

    int seconds = timeLimit / 150;

    static int totalFrames = 0;
    totalFrames++;
    int elapsedSeconds = totalFrames / 60;

    // 生存中の敵の数をカウント
    int activeEnemyCount = 0;
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode != NONE) {
            activeEnemyCount++;
        }
    }

    // タイマーを加算
    spawnTimer++;
    redGoblinSpawnTimer++;

    // 生存上限（25体制限）
    if (activeEnemyCount < 100) {

        // ---【通常ゴブリン：2秒ごと（120フレーム）】---
        if (spawnTimer >= 120) {
            spawnTimer = 0;

            int spawnCount = (elapsedSeconds < 100) ? 2 : 3;
            for (int i = 0; i < spawnCount; i++) {
                float centeX = (float)(GetRand(3000) - 1500);
                float centeZ = (float)(GetRand(3000) - 1500);

                for (int j = 0; j < 3; j++) {
                    float offsetX = (float)(GetRand(400) - 200);
                    float offsetZ = (float)(GetRand(400) - 200);

                    ActivateEnemy(enemyList, centeX + offsetX, centeZ + offsetZ);
                }
            }
        }

        if (redGoblinSpawnTimer >= 300) {
            redGoblinSpawnTimer = 0;
            printfDx("Red Goblin Spawn Triggered! BaseHandle: %d\n", redGoblinBaseModel);

            float spawnX = (float)(GetRand(3000) - 1500);
            float spawnZ = (float)(GetRand(3000) - 1500);

            ActivateRedGoblin(enemyList, spawnX, spawnZ);
        }
    }

    // 制限時間に応じた特殊ゴーレムの出現
    if (seconds <= 75 && gameState == 0) {
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
    bool spawned = false;

    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {

            // モデルのチェック
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }

            if (enemyList[i].model1 == -1) {
                printfDx("[Red] Model Duplicate Failed! (index:%d)\n", i);
                continue;
            }

            enemyList[i].pos = VGet(x, 0.0f, z);

            extern int stagedata;
            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);

            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            float heightOffset = 600.0f;
            enemyList[i].pos.y = baseFloorY + heightOffset;

            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 5;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }

            // アニメーション設定
            extern int red_goblin_anim_neutral;
            enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, red_goblin_anim_neutral);
            if (enemyList[i].attachidx != -1) {
                enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
            }

            printfDx("[Red] Successfully Activated! index:%d, Pos:(%.1f, %.1f, %.1f)\n",
                i, enemyList[i].pos.x, enemyList[i].pos.y, enemyList[i].pos.z);

            spawned = true;
            break;
        }
    }

    if (!spawned) {
        printfDx("[Red] No Empty Slot! (TEST_ENEMY_RED=%d, MAX_CHARA=%d)\n", TEST_ENEMY_RED, MAX_CHARA);
    }
}

// ==========================================
// 通常ゴブリン生成：TEST_ENEMY_INDEX 〜 TEST_ENEMY_RED 手前のみを使用
// ==========================================
void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
   for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);

            extern int stagedata;
            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);

            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            float heightOffset = 600.0f;
            enemyList[i].pos.y = baseFloorY + heightOffset;

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
    int seconds = timeLimit / 150;
    SetFontSize(20);

    if (pIdx == 0) {
        unsigned int timerColor;
        if (seconds <= 10 && (timeLimit / 10) % 2 == 0) {
            timerColor = GetColor(255, 0, 0);
        }
        else if (seconds <= 25) {
            timerColor = GetColor(255, 0, 0);
        }
        else if (seconds <= 45) {
            timerColor = GetColor(255, 0, 0);
        }
        else if (seconds <= 75) {
            timerColor = GetColor(0, 200, 200);
        }
        else {
            timerColor = GetColor(255, 255, 0);
        }

        SetDrawArea(0, 0, sw, sh);

        SetFontSize(28);
        if (seconds > 0) {
            DrawFormatString(sw / 2 - 48, 22, GetColor(0, 0, 0), "LIMIT : %d", seconds);
            DrawFormatString(sw / 2 - 50, 20, timerColor, "LIMIT : %d", seconds);
        }
        else {
            DrawString(sw / 2 - 38, 22, "FINISH!", GetColor(0, 0, 0));
            DrawString(sw / 2 - 40, 20, "FINISH!", GetColor(255, 0, 0));
        }
        SetFontSize(20);
    }

    if (gameState != 0) {
        SetFontSize(60);
        int color = GetColor(255, 255, 0);

        if (gameState == 1) {
            DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        }
        else if (gameState == 2) {
            DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
        }
        else {
            DrawString(xOffset + 100, sh / 2 - 30, "DRAW", color);
        }
        SetFontSize(20);
    }

    int startY = 20;
    unsigned int pColor = (pIdx == 0) ? GetColor(100, 200, 255) : GetColor(255, 150, 100);
    DrawFormatString(xOffset + 20, startY, pColor, "--- PLAYER %d ---", pIdx + 1);

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
        if (hpRate <= 0.3f) {
            SetDrawBright(255, 100, 100);
        }
        else {
            SetDrawBright(100, 255, 100);
        }

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
}

void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;
    }
    else {
        p2Score += score;
    }
}
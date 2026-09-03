#include "game.h"
#include "main.h"
#include "player.h"

int enemy_anim_attack;
 int enemy_anim_neutral;
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

     // ゴブリンの自動出現（スポーン）
     spawnTimer++;
     if (spawnTimer >= 300) {
         float baseX = (float)(GetRand(1000) - 500);
         float baseZ = (float)(GetRand(1000) - 500);
         ActivateEnemy(enemyList, baseX, baseZ);
         spawnTimer = 0;
     }

     // 制限時間に応じた特殊ゴレムの出現
     if (seconds <= 75 && gameState == 0) {
         if (enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
             enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 0.0f, -150.0f);
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
 void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
     for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
         if (enemyList[i].mode == NONE) {
             enemyList[i].pos = VGet(x, 0.0f, z);
             enemyList[i].mode = STAND;
             enemyList[i].enemyHP = 6;
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

void GameManager::DrawUI(int pIdx, int sw, int sh, SCharaInfo* players) {
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int seconds = timeLimit / 150;
    SetFontSize(20);

    // 色の判定
    unsigned int timerColor;
    if (seconds <= 10 && (timeLimit / 10) % 2 == 0) {
        timerColor = GetColor(255, 0, 0); // 点滅（赤）
    }
    else if (seconds <= 75) {
        timerColor = GetColor(0, 200, 200);
    }
    else if (seconds <= 45) {
        timerColor = GetColor(255, 0, 0);
    }
    else {
        timerColor = GetColor(255, 255, 0);
    }

    // 表示切り替え
    if (seconds > 0) {
        DrawFormatString(400, 20, timerColor, "LIMIT : %d", seconds);
    }
    else {
        DrawString(400, 20, "FINISH!", GetColor(255, 0, 0));
    }

    // 結果表示
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

    // スコア表示
    DrawFormatString(100, 50, GetColor(0, 255, 100), "P1 Score: %d", p1Score);
    DrawFormatString(700, 50, GetColor(0, 255, 100), "P2 Score: %d", p2Score);

    DrawFormatString(100, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount[0]);
    DrawFormatString(700, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount[1]);

    // P1 HPバー
    DrawString(30, 10, "P1", GetColor(255, 255, 255));
    DrawBox(60, 10, 260, 30, GetColor(80, 80, 80), TRUE);
    int p1Width = (200 * players[0].HP) / MAX_HP;
    if (p1Width < 0) p1Width = 0;
    DrawBox(60, 10, 60 + p1Width, 30, GetColor(0, 255, 0), TRUE);

    // P2 HPバー
    DrawString(660, 10, "P2", GetColor(255, 255, 255));
    DrawBox(690, 10, 890, 30, GetColor(80, 80, 80), TRUE);
    int p2Width = (200 * players[1].HP) / MAX_HP;
    if (p2Width < 0) p2Width = 0;
    DrawBox(690, 10, 690 + p2Width, 30, GetColor(0, 255, 0), TRUE);
}

void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;
    }
    else {
        p2Score += score;
    }
}
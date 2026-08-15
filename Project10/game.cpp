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

    spawnTimer++;
    if (spawnTimer >= 300) {
        float baseX = (float)(GetRand(1000) - 500);
        float baseZ = (float)(GetRand(1000) - 500);
        ActivateEnemy(enemyList, baseX, baseZ);
        spawnTimer = 0;
    }

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

    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) continue;

        if (enemyList[i].mode == DOWNMODE) {
            // ダウン中のアニメーションを進める
            enemyList[i].playtime += 0.5f;
            if (enemyList[i].attachidx != -1) {
                if (enemyList[i].playtime >= enemyList[i].anim_totaltime) {
                    enemyList[i].playtime = enemyList[i].anim_totaltime; // 最後のフレームで固定
                }
                MV1SetAttachAnimTime(enemyList[i].model1, enemyList[i].attachidx, enemyList[i].playtime);
            }
        }
        else {
            UpdateEnemyAI(enemyList[i], players);
        }
    }
}

void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 1;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
                enemyList[i].attachidx = -1;
            }

            enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, enemy_anim_neutral);
            if (enemyList[i].attachidx != -1) {
                enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
            }
            return;
        }
    }
}

void GameManager::UpdateEnemyAI(SCharaInfo& enemy, SCharaInfo* players) {
    if (enemy.mode == DOWNMODE || enemy.mode == NONE) {
        return;
    }
    float distP1 = VSize(VSub(players[0].pos, enemy.pos));
    float distP2 = VSize(VSub(players[1].pos, enemy.pos));
    SCharaInfo& target = (distP1 < distP2) ? players[0] : players[1];

    VECTOR dir = VSub(target.pos, enemy.pos);
    dir.y = 0;
    float dist = VSize(dir);

    if (enemy.mode != ATTACK) {
        // --- 1. 通常移動および攻撃開始判定 ---
        if (dist > 150.0f) {
            // プレイヤーが離れたら追尾
            enemy.mode = STAND;
            VECTOR moveDir = VNorm(dir);
            enemy.pos = VAdd(enemy.pos, VScale(moveDir, 2.0f));
            MV1SetPosition(enemy.model1, enemy.pos);

            float angle = atan2f(dir.x, dir.z);
            MV1SetRotationXYZ(enemy.model1, VGet(0.0f, angle, 0.0f));
        }
        else {
            // 攻撃範囲内に入ったら攻撃に移行
            enemy.mode = ATTACK;
            enemy.playtime = 0.0f;
            enemy.isHit = false;

            // 向きをプレイヤーに向ける
            float angle = atan2f(dir.x, dir.z);
            MV1SetRotationXYZ(enemy.model1, VGet(0.0f, angle, 0.0f));

            // 攻撃アニメーションに切り替え
            if (enemy.attachidx != -1) {
                MV1DetachAnim(enemy.model1, enemy.attachidx);
            }
            enemy.attachidx = MV1AttachAnim(enemy.model1, 0, enemy_anim_attack);
            if (enemy.attachidx != -1) {
                enemy.anim_totaltime = MV1GetAttachAnimTotalTime(enemy.model1, enemy.attachidx);
            }
        }
    }
    else {
        // --- 2. 攻撃中の処理 ---
        if (enemy.playtime >= enemy.anim_totaltime * 0.1f && enemy.isHit == false) {
            for (int p = 0; p < PLAYER_COUNT; p++) {
                if (players[p].mode == DOWNMODE) continue;

                if (HitCheck_Capsule_Capsule(
                    enemy.pos, VAdd(enemy.pos, VGet(0, 50, 0)), 60.0f,
                    players[p].pos, VAdd(players[p].pos, VGet(0, players[p].charahitinfo.Height, 0)), players[p].charahitinfo.Width / 2))
                {
                    players[p].HP -= 1;
                    enemy.isHit = true;
                    break;
                }
            }
        }

        // アニメーションの進行
        enemy.playtime += 0.2f;

        // 攻撃アニメーションが最後まで再生し終わったら STAND に戻す
        if (enemy.playtime >= enemy.anim_totaltime) {
            enemy.mode = STAND;
            enemy.playtime = 0.0f;
            enemy.isHit = false;

            if (enemy.attachidx != -1) {
                MV1DetachAnim(enemy.model1, enemy.attachidx);
            }
            // 待機アニメーションに戻す
            enemy.attachidx = MV1AttachAnim(enemy.model1, 0, enemy_anim_neutral);
            if (enemy.attachidx != -1) {
                enemy.anim_totaltime = MV1GetAttachAnimTotalTime(enemy.model1, enemy.attachidx);
            }
        }
    }

    // --- 3. 待機中（STAND）のアニメーションループ処理 ---
    if (enemy.mode == STAND) {
        enemy.playtime += 0.5f;
        if (enemy.playtime >= enemy.anim_totaltime) {
            enemy.playtime = 0.0f;
        }
    }

    // アニメーション時間をモデルに反映
    if (enemy.attachidx != -1) {
        MV1SetAttachAnimTime(enemy.model1, enemy.attachidx, enemy.playtime);
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
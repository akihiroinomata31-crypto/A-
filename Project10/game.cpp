#include "game.h"
#include "main.h"
#include "player.h"
#include <math.h>
#include "taiki.h"
int stagedata = -1;
int enemy_anim_attack = -1;
int redGoblinBaseModel = -1;
int red_goblin_anim_neutral = -1;
int red_goblin_anim_attack = -1;
int weaponBaseModel = -1;
int specialGauge[2] = { 0, 0 };
int gh_gauge_frame = -1;

EnemyManager g_enemyMgr;
void GameManager::Init() {
    mainTimer = 99 * 60;
    goblinSpawnTimer = 0;
    redSpawnTimer = 0;
    p1Score = 0;
    p2Score = 0;
    gameState = 0;
    deathCount[0] = 0;
    deathCount[1] = 0;
    g_enemyMgr.Init(TEST_ENEMY_INDEX, TEST_ENEMY_RED, 60);
    for (int i = 0; i < 20; i++) {
        popups[i].active = false;
    }
}

void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // ==========================================
    // 1. メイン制限時間タイマー
    // ==========================================
    if (mainTimer > 0) {
        mainTimer--;
    }
    else if (gameState == 0) {
        if (p1Score > p2Score)      gameState = 1;
        else if (p2Score > p1Score) gameState = 2;
        else                        gameState = 3;
    }

    // ==========================================
    // 2. 待機キューの自動補充（メイン枠が空いていれば即出撃）
    // ==========================================
    g_enemyMgr.UpdateQueue(enemyList);

    // ==========================================
    // 3. 敵スポーン要求（タイマー制御）
    // ==========================================
    // --- 通常ゴブリン：約0.5秒（30フレーム）ごとにP1/P2周辺へスポーン要求 ---
    goblinSpawnTimer++;
    if (goblinSpawnTimer >= 30) {
        goblinSpawnTimer = 0;

        // プレイヤー1または2の周辺から新鮮な座標を計算（交互またはランダム）
        int targetPlayer = GetRand(1);
        float angle = (float)GetRand(360) * DX_PI_F / 180.0f;
        float dist = 500.0f + (float)GetRand(300); // プレイヤーから500〜800離れた位置

        float sx = players[targetPlayer].pos.x + cosf(angle) * dist;
        float sz = players[targetPlayer].pos.z + sinf(angle) * dist;
        VECTOR spawnPos = VGet(sx, 0.0f, sz); // 地面の高さ(0.0f)

        // 空いていれば即出現、満杯なら待機キューへ自動退避
        g_enemyMgr.RequestSpawn(enemyList, spawnPos, 2, 0);
    }

    // --- 赤ゴブリン：約5秒（300フレーム）ごとに1体要求 ---
    redSpawnTimer++;
    if (redSpawnTimer >= 300) {
        redSpawnTimer = 0;

        float spawnX = (float)(GetRand(3000) - 1500);
        float spawnZ = (float)(GetRand(3000) - 1500);
        VECTOR redSpawnPos = VGet(spawnX, 0.0f, spawnZ);

        // 赤ゴブリン（HP: 5, タイプ: 1）
        g_enemyMgr.RequestSpawn(enemyList, redSpawnPos, 5, 1);
    }

    // ==========================================
    // 4. 生きている敵の更新＆自動回収（成仏・リサイクル）
    // ==========================================
    for (int a = 0; a < g_enemyMgr.GetActiveCount(); a++) {
        int i = g_enemyMgr.GetActiveSlot(a); // 実体インデックス

        // プレイヤーとの距離判定（離れすぎ回収用）
        float distP1 = VSize(VSub(enemyList[i].pos, players[0].pos));
        float distP2 = VSize(VSub(enemyList[i].pos, players[1].pos));

        // ① 撃破された（HP<=0）または ② プレイヤーから遠すぎる（1500以上）
        if (enemyList[i].enemyHP <= 0 || (distP1 > 1500.0f && distP2 > 1500.0f)) {
            // 撃破時のスコア加算などはここで行う
            if (enemyList[i].enemyHP <= 0) {
                // AddScore(...);
            }

            // メイン配列から外し、空き枠へ戻す（待機キューの敵が入れるようになる）
            g_enemyMgr.Despawn(enemyList, a);
            a--; // 要素が詰められたので1つ戻す
            continue;
        }

        // --- ここに敵の移動AIやアニメーション更新処理 ---
        // 例: プレイヤーへ近づく処理など
    }

    // ==========================================
    // 5. 特殊ゴーレムの出現条件（残り約50秒以下）
    // ==========================================
    int seconds = mainTimer / 60;
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
// ==========================================
// 通常ゴブリンのスポーン（軽量版）
// ==========================================
void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
        if (enemyList[i].mode == NONE) {
            // ★ポリゴン判定を全カットし、床の高さ（例: 800.0f や 30.0f）に直接配置
            enemyList[i].pos = VGet(x, 600.0f, z);

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

// ==========================================
// 赤ゴブリンのスポーン（軽量版）
// ==========================================
void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }
            if (enemyList[i].model1 == -1) continue;

            // ★ポリゴン判定を全カット
            enemyList[i].pos = VGet(x, 600.0f, z);

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
        //else                     DrawString(xOffset + 100, sh / 2 - 30, "DRAW", color);
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

    // ==========================================
    // 必殺技ゲージ描画
    // ==========================================
    int dispGaugeW = 220; // ゲージ幅
    int dispGaugeH = 22;  // ゲージ高さ
    int halfW = sw / 2;

    int gaugeX = 0;
    int gaugeY = sh - dispGaugeH - 25; // 画面下部

    // P1は左画面の右下、P2は右画面の左下に配置
    if (pIdx == 0) {
        gaugeX = halfW - dispGaugeW - 20; // 左画面の右下
    }
    else {
        gaugeX = halfW + 20;              // 右画面の左下
    }

    // 1. 中身（青バー）の幅計算
    float rate = (float)specialGauge[pIdx] / (float)MAX_SPECIAL_GAUGE;
    if (rate > 1.0f) rate = 1.0f;
    int fillW = (int)((dispGaugeW - 8) * rate);

    // 2. 青ゲージ描画（枠の内側）
    if (fillW > 0) {
        int barColor = (specialGauge[pIdx] >= MAX_SPECIAL_GAUGE)
            ? GetColor(0, 220, 255)
            : GetColor(30, 120, 255);
        DrawBox(gaugeX + 4, gaugeY + 3, gaugeX + 4 + fillW, gaugeY + dispGaugeH - 3, barColor, TRUE);
    }

    // 3. 上から枠画像を描画（透過ON）
    DrawExtendGraph(gaugeX, gaugeY, gaugeX + dispGaugeW, gaugeY + dispGaugeH, gh_gauge_frame, TRUE);

    // 4. 文字表示
    SetFontSize(16);
    if (specialGauge[pIdx] >= MAX_SPECIAL_GAUGE) {
        char keyText[32];
        sprintf_s(keyText, "SPECIAL READY! [%s]", (pIdx == 0) ? "U" : "N");
        DrawString(gaugeX, gaugeY - 18, keyText, GetColor(255, 255, 0));
    }
    else {
        DrawFormatString(gaugeX, gaugeY - 18, GetColor(200, 200, 255), "SPECIAL %d%%", specialGauge[pIdx]);
    }
    SetFontSize(20);

    // 描画前にブレンドモードを正常化しておく
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // 画像ハンドルが正常（!= -1）なときだけ描画
    if (gh_gauge_frame != -1) {
        DrawExtendGraph(gaugeX, gaugeY, gaugeX + dispGaugeW, gaugeY + dispGaugeH, gh_gauge_frame, TRUE);
    }
    else {
        // 読み込み失敗時の保険：とりあえず白い枠線を自前で描く
        DrawBox(gaugeX, gaugeY, gaugeX + dispGaugeW, gaugeY + dispGaugeH, GetColor(255, 255, 255), FALSE);
    }

    if (gh_gauge_frame != -1) {
        SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
        DrawExtendGraph(gaugeX, gaugeY, gaugeX + dispGaugeW, gaugeY + dispGaugeH, gh_gauge_frame, FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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






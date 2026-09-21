#include "game.h"
#include "main.h"
#include "player.h"
int stagedata;
int enemy_anim_attack;
int enemy_anim_neutral;
int red_goblin_anim_neutral;
int red_goblin_anim_attack;

 
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
     static int redGoblinSpawnTimer = 0;
     redGoblinSpawnTimer++;

     // 例：ゲーム開始から一定フレーム（または制限時間が特定の時間）になったら出現
     if (redGoblinSpawnTimer >= 600) { // 10秒に1回など
         redGoblinSpawnTimer = 0;

         float rx = (float)(GetRand(1000) - 500);
         float rz = (float)(GetRand(1000) - 500);

         // ※引数に使うモデルのハンドルは main.cpp 側から渡すか、グローバル変数として共有してください
         // ActivateRedGoblin(enemyList, rx, rz, redGoblinBaseModel, red_goblin_anim_neutral);
     }
     int seconds = timeLimit / 150;

     // ゲーム開始からの経過フレームを計測する静的変数
     static int totalFrames = 0;
     totalFrames++;
     int elapsedSeconds = totalFrames / 60; // 60FPS想定（1秒 = 60フレーム）

     // ゴブリンの自動出現（スポーン）タイマー
     spawnTimer++;

     // 2秒ごと（60FPS × 2秒 = 120フレーム）に判定
     if (spawnTimer >= 120) {
         spawnTimer = 0;

         // 25秒未満は5体、25秒以降は10体を同時にスポーンさせる
         int spawnCount = (elapsedSeconds < 25) ? 3 : 5;
         for (int i = 0; i < spawnCount; i++) {
             float baseX = (float)(GetRand(14000) - 7000); // -1000 ～ +1000 の範囲
             float baseZ = (float)(GetRand(14000) - 7000);
             ActivateEnemy(enemyList, baseX, baseZ);
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


 // game.cpp の適当な場所に追加
 void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z, int redGoblinBaseModel, int red_goblin_anim_neutral) {
     for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
         if (enemyList[i].mode == NONE) {
             // モデルがまだ割り当てられていない場合は複製してセットする
             if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                 enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
             }

             if (enemyList[i].model1 == -1) continue;

             // 初期位置の設定（ステージの床高さを取得して合わせる）
             enemyList[i].pos = VGet(x, 0.0f, z);

             extern int stagedata;
             VECTOR cal_pos1 = VGet(x, 2000.0f, z);
             VECTOR cal_pos2 = VGet(x, -1000.0f, z);
             MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

             float baseFloorY = 0.0f;
             if (LineRes.HitFlag == 1) {
                 baseFloorY = LineRes.HitPosition.y;
             }
             enemyList[i].pos.y = baseFloorY + 0.0f; // 必要に応じて高さ調整

             // パラメータの設定（通常のゴブリンよりHPを高くするなど）
             enemyList[i].mode = STAND;
             enemyList[i].enemyHP = 5; // 例：赤ゴブリンはHP 5
             enemyList[i].playtime = 0.0f;
             enemyList[i].isHit = false;

             MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
             MV1SetVisible(enemyList[i].model1, TRUE);

             if (enemyList[i].attachidx != -1) {
                 MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
             }
             // 赤ゴブリン専用のニュートラルアニメーションをアタッチ
             enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, red_goblin_anim_neutral);
             if (enemyList[i].attachidx != -1) {
                 enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
             }
             break;
         }
     }
 }


 void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
     for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
         if (enemyList[i].mode == NONE) {
             // まず仮の位置を設定
             enemyList[i].pos = VGet(x, 0.0f, z);

             // ==========================================
             // ★ 正しい型（MV1_COLL_RESULT_POLY）で床の高さを取得する
             // ==========================================
             extern int stagedata;

             VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
             VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);

             // 戻り値の型を MV1_COLL_RESULT_POLY に合わせる
             MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

             float baseFloorY = 0.0f;
             if (LineRes.HitFlag == 1) { // ※MV1_COLL_RESULT_POLYのヒットフラグは 1 (または TRUE)
                 baseFloorY = LineRes.HitPosition.y; // ぶつかった正確なY座標
             }

             // ★高さを自由に変えたいときはここの数字を変更してください
             float heightOffset = 600.0f;

             enemyList[i].pos.y = baseFloorY + heightOffset;
             // ==========================================


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

void GameManager::DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex) {
    // 画面分割に対応するためのオフセット（P1なら0、P2なら画面半分右側にずれる）
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int seconds = timeLimit / 150;
    SetFontSize(20);

    // ==========================================
    // 1. 制限時間表示（P1側の処理のときだけ、画面中央上に描画する）
    // ==========================================
    if (pIdx == 0) {
        // 制限時間の色判定
        unsigned int timerColor;
        if (seconds <= 10 && (timeLimit / 10) % 2 == 0) {
            timerColor = GetColor(255, 0, 0); // 点滅（赤）
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

        // 描画エリアを画面全体に一時リセット（これで中央の文字が分割線で切られなくなる）
        SetDrawArea(0, 0, sw, sh);

        SetFontSize(28);
        if (seconds > 0) {
            DrawFormatString(sw / 2 - 48, 22, GetColor(0, 0, 0), "LIMIT : %d", seconds); // 黒い影
            DrawFormatString(sw / 2 - 50, 20, timerColor, "LIMIT : %d", seconds);       // 本体の文字
        }
        else {
            DrawString(sw / 2 - 38, 22, "FINISH!", GetColor(0, 0, 0));                 // 黒い影
            DrawString(sw / 2 - 40, 20, "FINISH!", GetColor(255, 0, 0));                 // 本体の文字
        }
        SetFontSize(20);
    }

    // ==========================================
    // 2. 結果表示（ゲーム終了時）
    // ==========================================
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

    // ==========================================
    // 3. プレイヤーごとのUI（名前・HPバー・スコア・デス数）
    // ==========================================
    int startY = 20;

    // 【一番上】プレイヤー名（P1 / P2）
    unsigned int pColor = (pIdx == 0) ? GetColor(100, 200, 255) : GetColor(255, 150, 100);
    DrawFormatString(xOffset + 20, startY, pColor, "--- PLAYER %d ---", pIdx + 1);

    // 【その下】HPバー
    int frameDrawW = 180;
    int frameDrawH = 24;
    int drawX = xOffset + 20;
    int drawY = startY + 30;

    float hpRate = (float)players[pIdx].HP / (float)MAX_HP;
    if (hpRate < 0.0f) hpRate = 0.0f;
    if (hpRate > 1.0f) hpRate = 1.0f;

    int innerMargin = 2;

    // 枠画像を下に描画
    DrawExtendGraph(drawX, drawY, drawX + frameDrawW, drawY + frameDrawH, hpBarTex, TRUE);

    // 赤ゲージ（背景）
    DrawBox(drawX + innerMargin, drawY + innerMargin,
        drawX + frameDrawW - innerMargin, drawY + frameDrawH - innerMargin,
        GetColor(200, 0, 0), TRUE);

    // 緑バー（現在HP）
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

    // 【さらにその下】スコア ＆ デスカウント
    int currentScore = (pIdx == 0) ? p1Score : p2Score;
    int currentDeaths = (pIdx == 0) ? deathCount[0] : deathCount[1];

    int infoY = drawY + 32;
    // スコアは豪華な金色（ゴールド）で表示
    DrawFormatString(xOffset + 20, infoY, GetColor(255, 215, 0), "Score: %d", currentScore);
    DrawFormatString(xOffset + 140, infoY, GetColor(255, 100, 200), "DEATHS: %d", currentDeaths);
}
void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;
    }
    else {
        p2Score += score;
    }
}


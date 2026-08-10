#include "game.h"
#include "main.h"



#include "player.h"



void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // --- 1. 制限時間とタイマーの処理 ---
    if (timeLimit > 0) {
        timeLimit--;
    }
    else if (gameState == 0) { // 制限時間が切れた瞬間に一度だけ判定
        if (p1Score > p2Score) gameState = 1;      // P1の勝ち
        else if (p2Score > p1Score) gameState = 2; // P2の勝ち
        else gameState = 3;                        // 引き分け
    }

    // 現在の残り秒数を計算（150フレームで1秒の仕様）
    int seconds = timeLimit / 150;

    // --- 2. 残り75秒以下になったときの処理 ---
    if (seconds <= 75 && gameState == 0) {

        // 【重要】もし75秒になった瞬間に一度だけゴーレムを出現させたい場合
        // （まだゴーレムが NONE 状態のときに出現させる）
        if (enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
            enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 0.0f, -150.0f); // 出現位置
            enemyList[TEST_ENEMY_GOLEM].mode = STAND;
            enemyList[TEST_ENEMY_GOLEM].enemyHP = 10; // ゴーレムのHPなど
            MV1SetPosition(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].pos);
            MV1SetVisible(enemyList[TEST_ENEMY_GOLEM].model1, TRUE);
        }

        // 通常のゴブリンの定時スポーン処理（3秒に1回など）
        spawnTimer++;
        if (spawnTimer >= 300) {
            float baseX = (float)(GetRand(10000) - 200);
            float baseZ = (float)(GetRand(-10000) - 200);
            ActivateEnemy(enemyList, baseX, baseZ);
            spawnTimer = 0;
        }
    }

    // --- 3. AI更新処理・アニメーション進行（敵全体） ---
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode != NONE && enemyList[i].mode != DOWNMODE) {
            // AIと移動の更新
            UpdateEnemyAI(enemyList[i], players);

            // ★追加：ここでアニメーションの再生時間を進める
            enemyList[i].playtime += 0.5f; // スピードはお好みで調整してください

            // アニメーションが最後のフレームを超えたらループさせる
            if (enemyList[i].playtime >= enemyList[i].anim_totaltime) {
                enemyList[i].playtime = 0.0f;
            }

            // ★追加：モデルにアニメーションの時間を反映させる
            MV1SetAttachAnimTime(enemyList[i].model1, enemyList[i].attachidx, enemyList[i].playtime);
        }
    }
}
void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {

    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {

            // 1. 位置やモードの設定
            enemyList[i].pos = VGet(x, 0.0f, z);
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 1;
            enemyList[i].playtime = 0.0f;

            // 2. モデルの位置と表示を更新
            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            // 3. ★出現した瞬間にアニメーションを確実に再アタッチする
            MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            // ※ enemy_anim_neutral は extern 等で GameManager から参照できるようにするか、
            // または初期化時にアタッチした状態が維持されるようにします。

            return;
        }
    }
}
// 敵AIの本体
void GameManager::UpdateEnemyAI(SCharaInfo& enemy, SCharaInfo* players) {
    float distP1 = VSize(VSub(players[0].pos, enemy.pos));
    float distP2 = VSize(VSub(players[1].pos, enemy.pos));
    SCharaInfo& target = (distP1 < distP2) ? players[0] : players[1];

    // --- 【修正ここから】オフセットの計算 ---
    // 敵の現在位置を使って「プレイヤーの周りのどの位置を狙うか」を固定する
    // これにより、全員がバラバラの位置を目指すようになります
    float offsetX = fmod(enemy.pos.x, 100.0f) - 50.0f; // -50〜+50 のズレ
    float offsetZ = fmod(enemy.pos.z, 100.0f) - 50.0f;

    VECTOR targetPos = VAdd(target.pos, VGet(offsetX, 0.0f, offsetZ));
    // --- 【修正ここまで】 ---

    // 向きを変える（ターゲットを修正後の targetPos に変更）
    VECTOR dir = VSub(targetPos, enemy.pos);
    dir.y = 0;
    float dist = VSize(dir); // 修正後の距離を使う

    float angle = atan2f(dir.x, dir.z);
    MV1SetRotationXYZ(enemy.model1, VGet(0.0f, angle, 0.0f));

    // 移動処理
    if (dist > 150.0f) {
        if (enemy.mode != ATTACK) {
            enemy.mode = STAND;
            VECTOR moveDir = VNorm(dir);
            enemy.pos = VAdd(enemy.pos, VScale(moveDir, 2.0f));
            MV1SetPosition(enemy.model1, enemy.pos);
        }
    }
    else {
        // 攻撃範囲内
        if (enemy.mode == STAND) {
            enemy.mode = ATTACK;
            enemy.playtime = 0.0f;
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
    // 1. secondsの宣言は1回だけにする（60で割るのが正しいです）
   
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int seconds = timeLimit / 150;
    SetFontSize(20);
    // 2. 色の判定
    unsigned int timerColor;
    if (seconds <= 10 && (timeLimit / 10) % 2 == 0) {
        timerColor = GetColor(255, 0, 0); // 点滅（赤）
    }
    else if (seconds <= 75) {
        timerColor = GetColor(0, 200, 200); // 赤
    }
    else if (seconds <= 45) {
        timerColor = GetColor(255, 0, 0); // 赤
    }
    else {
        timerColor = GetColor(255, 255, 0); // 最初は黄色
    }

    // 3. 表示の切り替え（重ならないように if-else で完全に分ける）
    if (seconds > 0) {
        DrawFormatString(400, 20, timerColor, "LIMIT : %d", seconds);
    }
    else {
        DrawString(400, 20, "FINISH!", GetColor(255, 0, 0));
    }

    // 結果表示（プレイ中以外なら表示）
    if (gameState != 0) {
        SetFontSize(60); // 文字を大きくする
        int color = GetColor(255, 255, 0);

        if (gameState == 1) { // P1勝利
            DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        }
        else if (gameState == 2) { // P2勝利
            DrawString(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
        }
        else { // 引き分け
            DrawString(xOffset + 100, sh / 2 - 30, "DRAW", color);
        }
        SetFontSize(20); // フォントサイズを戻す
    }

    // スコア表示
    DrawFormatString(100, 50, GetColor(0, 255, 100), "P1 Score: %d", p1Score);
    DrawFormatString(700, 50, GetColor(0, 255, 100), "P2 Score: %d", p2Score);

    DrawFormatString(100, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount[0]);

    // プレイヤー2の死亡回数
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



// game.cpp



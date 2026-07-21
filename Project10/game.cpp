#include "game.h"
#include "main.h"

void GameManager::Update() {
    if (timeLimit > 0) {
        timeLimit--;
    }
    else {
        timeLimit = 0;
    }
}

void GameManager::Update(SCharaInfo* enemyList) {
    if (timeLimit > 0) {
        timeLimit--;

        spawnTimer++;
        // 120フレームごとに1体出現させる（5体一気出しを防ぐ）
        if (spawnTimer >= 120) {

            // ランダムな位置を決定
            float randX = (float)(GetRand(1600) - 800);
            float randZ = (float)(GetRand(1600) - 800);

            // 敵をアクティブにする（1体のみ生成）
            ActivateEnemy(enemyList, randX, randZ);

            spawnTimer = 0;
        }
    }
    else {
        timeLimit = 0;
    }
}

void GameManager::RecordFrame(VECTOR p1, VECTOR p2, int act) {
    ReplayFrame frame;
    frame.pos[0] = p1;
    frame.pos[1] = p2;
    frame.action = act;
    replayData.push_back(frame);
}

void GameManager::DrawUI() {
    // 1. secondsの宣言は1回だけにする（60で割るのが正しいです）
    int seconds = timeLimit / 100;
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

    // スコア表示
    DrawFormatString(100, 50, GetColor(0, 255, 100), "P1 Score: %d", p1Score);
    DrawFormatString(700, 50, GetColor(0, 255, 100), "P2 Score: %d", p2Score);

    DrawFormatString(100, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount);
    DrawFormatString(700, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount);
}

void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
    // 1番目からMAX_CHARA-1まで探す
    for (int i = 1; i < MAX_CHARA; i++) {
        // mode が NONE ならその枠は空いているとみなす
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);
            enemyList[i].mode = STAND; // 待機状態へ
            enemyList[i].enemyHP = 1;

            // モデル位置更新と表示
            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            break; // 1体見つけたらループを抜ける
        }
    }
}

#include "game.h"
#include "main.h"

void GameManager::Update() {
    if (timeLimit > 0) {
        timeLimit--;

        spawnTimer++;
        if (spawnTimer >= 120) {
            for (int i = 0; i < 5; i++) {
                // 敵配列 charainfo[1]以降を敵専用として管理する場合
                // ここで敵の出現位置をランダムに決定して初期化する
                float randX = (float)(GetRand(1600) - 800); // マップ範囲に合わせて調整
                float randZ = (float)(GetRand(1600) - 800);

                // 敵をアクティブにする処理をここで行う
                //ActivateEnemy(enemyList,randX, randZ);
            }
            spawnTimer = 0;
        }
    }
    else {
        // 0になった時の処理（ここを後で「試合終了」にする）
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
    // 1番目(配列の添え字1)以降の枠を探す
    for (int i = 1; i < MAX_CHARA; i++) {
        // もしその敵が「未出現(NONE)」の状態なら、そこに新しい敵を割り当てる
        if (enemyList[i].mode == NONE) {
            enemyList[i].pos = VGet(x, 0.0f, z);
            enemyList[i].mode = STAND; // 出現して待機状態にする
            enemyList[i].enemyHP = 1;  // HPを初期化

            // モデルの位置を更新
            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);

            break; // 1体生成したらループを抜ける
        }
    }
}

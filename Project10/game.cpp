#include "game.h"
#include "main.h"



void GameManager::Update(SCharaInfo* enemyList) {
    if (timeLimit > 0) {
        timeLimit--;

        spawnTimer++;
        // 120フレーム（2秒）ごとに処理を行う
        if (spawnTimer >= 120) {

            // 5体出現させるためのループ
            for (int i = 0; i < 5; i++) {
                // ループの中で毎回 GetRand を呼ぶので、
                // i = 0, 1, 2, 3, 4 でそれぞれ異なるランダムな値が入ります
                float randX = (float)(GetRand(1600) - 800);
                float randZ = (float)(GetRand(1600) - 800);

                // 個別の座標を渡してスポーンさせる
                ActivateEnemy(enemyList, randX, randZ);
            }

            // タイマーをリセットして次の2秒へ
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

    DrawFormatString(100, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount[0]);

    // プレイヤー2の死亡回数
    DrawFormatString(700, 100, GetColor(255, 100, 200), "DEATHS: %d", deathCount[1]);
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

void GameManager::AddScore(int score)
{
    p1Score += score;
}

// game.cpp

// 引数に SCharaInfo* players を追加してください
void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // --- スポーン処理 ---
    if (timeLimit > 0) {
        timeLimit--;
        spawnTimer++;
        if (spawnTimer >= 120) {
            for (int i = 0; i < 5; i++) {
                float randX = (float)(GetRand(1600) - 800);
                float randZ = (float)(GetRand(1600) - 800);
                ActivateEnemy(enemyList, randX, randZ);
            }
            spawnTimer = 0;
        }
    }
    else {
        timeLimit = 0;
    }

    // --- AI更新処理 ---
    for (int i = 1; i < MAX_CHARA; i++) {
        // 生きている敵（NONE以外）に対してAIを実行
        if (enemyList[i].mode != NONE && enemyList[i].mode != DOWNMODE) {
            UpdateEnemyAI(enemyList[i], players);
        }
    }
}

// 敵AIの本体
void GameManager::UpdateEnemyAI(SCharaInfo& enemy, SCharaInfo* players) {
    // ターゲット検索（P1とP2で近い方を狙う）
    float distP1 = VSize(VSub(players[0].pos, enemy.pos));
    float distP2 = VSize(VSub(players[1].pos, enemy.pos));
    SCharaInfo& target = (distP1 < distP2) ? players[0] : players[1];
    float dist = (distP1 < distP2) ? distP1 : distP2;

    // 向きを変える
    VECTOR dir = VSub(target.pos, enemy.pos);
    dir.y = 0;
    float angle = atan2f(dir.x, dir.z);
    MV1SetRotationXYZ(enemy.model1, VGet(0.0f, angle, 0.0f));

    // 移動と攻撃
    if (dist > 150.0f) {
        if (enemy.mode != ATTACK) {
            enemy.mode = STAND;
            VECTOR moveDir = VNorm(dir);
            enemy.pos = VAdd(enemy.pos, VScale(moveDir, 2.0f)); // スピード調整
            MV1SetPosition(enemy.model1, enemy.pos);
        }
    }
    else {
        // 攻撃範囲内なら攻撃（ここではアニメーション切り替えはmain.cppにあるので注意が必要です）
        // GameManagerからSetCharacterAnimationを呼ぶなら、ここに記述してください
        if (enemy.mode == STAND) {
            enemy.mode = ATTACK;
            enemy.playtime = 0.0f;
            // 攻撃開始の処理...
        }
    }
}
#pragma once
#include "DxLib.h"
#include <vector>
struct SCharaInfo;
struct ReplayFrame {
    VECTOR pos[5];
    int action;
};

class GameManager {
public:

    int timeLimit = 99 * 150;
    int p1Score = 0, p2Score = 0;
    std::vector<ReplayFrame> replayData;
    int spawnTimer = 0;
    //void Update();
    void DrawUI(int pIdx, int sw, int sh, SCharaInfo* players);
    // プレイヤー1と2でそれぞれカウントするために配列にする
    int deathCount[2] = { 0, 0 };
    int gameState = 0;
    void AddDeath(int playerIndex) {
        if (playerIndex >= 0 && playerIndex < 2) {
            deathCount[playerIndex]++;
        }
    }
    //int p1Score = 0;

    void AddScore(int playerIndex, int score);
   
    // リプレイを記録する関数を追加
    void RecordFrame(VECTOR p1, VECTOR p2, int act);
   // void Update(SCharaInfo* enemyList);
    void ActivateEnemy(SCharaInfo* enemyList, float x, float z);
    void UpdateEnemyAI(SCharaInfo& enemy, SCharaInfo* players);
    void Update(SCharaInfo* enemyList, SCharaInfo* players);
};
extern int enemy_anim_attack;
extern int enemy_anim_neutral;
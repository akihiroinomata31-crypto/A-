#pragma once
#include "DxLib.h"
#include <vector>

struct SCharaInfo;

struct ReplayFrame {
    VECTOR pos[5];
    int action;
};

struct ScorePopup {
    int score;
    int playerIdx;
    float offsetY;
    int timer;
    bool active;
};

// 落ちている閃光弾アイテムの情報
struct FlashItem {
    VECTOR pos;        // 現在位置
    float baseGroundY; // 浮遊する基準の地面の高さ
    float bobbingAngle;// フワフワ上下用のアングル
    bool isFalling;    // 空中から落下中かどうか
    bool active;       // 存在フラグ
};

// 投げられた閃光弾の情報
struct Flashbang {
    VECTOR pos;
    VECTOR vel;
    int timer;
    int ownerIdx;
    bool active;
};

class GameManager {
public:
    // --- 制限時間管理（独立） ---
    // 1秒 = 60フレーム換算。99秒なら 99 * 60 = 5940
    // もし描画側で / 120 などで割る場合は 99 * 120 に合わせます
    int mainTimer = 99 * 120;

    // --- 敵スポーン管理（独立） ---
    int goblinSpawnTimer = 0; // 通常ゴブリン用カウンター
    int redSpawnTimer = 0;    // 赤ゴブリン用カウンター

    ScorePopup popups[20];

    int p1Score = 0;
    int p2Score = 0;
    std::vector<ReplayFrame> replayData;

    static constexpr int DeathPenalty = 3000;
    static constexpr int SpecialPenalty = 1000;
    int specialUseCount[2] = { 0, 0 };
    int deathCount[2] = { 0, 0 };
    int gameState = 0;

    void AddDeath(int playerIndex) {
        if (playerIndex >= 0 && playerIndex < 2) {
            deathCount[playerIndex]++;


        }
    }

    int FinalScore(int playerIndex) const {
        if (playerIndex < 0 || playerIndex >= 2) return 0;
        const int earned = playerIndex == 0 ? p1Score : p2Score;
        return earned - deathCount[playerIndex] * DeathPenalty - specialUseCount[playerIndex] * SpecialPenalty;
    }
    void RecordSpecialAttack(int playerIndex) {
        if (playerIndex < 0 || playerIndex >= 2) return;
        ++specialUseCount[playerIndex];


    }
    bool isHit = false;
    int anim_neutral = -1;
    int anim_walk = -1;
    int anim_attack = -1;
    int enemy_anim_neutral = -1;
    int enemy_anim_walk = -1;
    // --- 閃光弾・アイテム管理 ---
    FlashItem flashItems[3];             // フィールド上に同時に存在できるアイテム最大数
    Flashbang flashbangs[4];            // 投擲中の弾
    int itemDropTimer = 0;              // アイテムが降ってくるインターバル用
    int flashbangStock[2] = { 0, 0 };   // [0]:1Pの所持数, [1]:2Pの所持数
    int playerStunTimer[2] = { 0, 0 };  // スタン残り時間

    void SpawnFlashItem();
    void UpdateFlashItems(SCharaInfo* players);
    void DrawFlashItems();

    void ThrowFlashbang(int playerIdx, VECTOR pPos, VECTOR forwardVec);
    void UpdateFlashbangs(SCharaInfo* players);
    void DrawFlashbangs();
    void DrawFlashOverlay(int pIdx, int startX, int startY, int endX, int endY);
    void Init();
    void Update(SCharaInfo* enemyList, SCharaInfo* players);
    void DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex, int crownGraphHandle);
    void DrawTimer(int sw, int sh);
    void DrawCrownOnLeader(int pIdx, SCharaInfo* charainfo, int crownGraphHandle, GameManager& game);
    void AddScore(int playerIndex, int score);
    void AddScorePopup(int playerIdx, int score);
    void RecordFrame(VECTOR p1, VECTOR p2, int act);
    void ActivateEnemy(SCharaInfo* enemyList, float x, float z);
    void ActivateRedGoblin(SCharaInfo* enemyList, float x, float z);
    void UpdateEnemyAI(SCharaInfo& enemy, SCharaInfo* players);
};

extern int stagedata;
extern int enemy_anim_attack;
extern int redGoblinBaseModel;
extern int red_goblin_anim_neutral;
extern int red_goblin_anim_attack;
extern int weaponBaseModel;
extern int red_goblin_anim_walk;

bool PlaceEnemyOnGround(SCharaInfo& enemy);

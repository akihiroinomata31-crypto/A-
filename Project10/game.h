#pragma once
#include "Dxlib.h"
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


class EnemyPool {
private:
    // 生きている敵の「実際の配列インデックス」のリスト
    std::vector<int> activeIndices;
    // 出番を待っている「空きスロット」のインデックス一覧（スタック）
    std::vector<int> freeIndices;

public:
    // 初期化（ゲーム開始時に1度だけ呼ぶ）
    void Init(int startIndex, int endIndex) {
        activeIndices.clear();
        freeIndices.clear();
        // 使えるインデックスをすべて「出番待ち」に登録
        for (int i = startIndex; i < endIndex; i++) {
            freeIndices.push_back(i);
        }
    }

    // ① 新鮮な敵を1体出撃させる（探索ループなし！O(1)の瞬速処理）
    int Spawn(SCharaInfo* charaArray, VECTOR spawnPos, int defaultHP) {
        if (freeIndices.empty()) {
            return -1; // 空き枠がなければ安全に何もしない（溢れ防止）
        }

        // 待機リストの末尾から1つ取り出す
        int slot = freeIndices.back();
        freeIndices.pop_back();

        // 生存リストへ登録
        activeIndices.push_back(slot);

        // ★スロットを完璧に「新鮮な状態」に初期化
        SCharaInfo& e = charaArray[slot];
        e.mode = STAND;
        e.pos = spawnPos;
        e.move = VGet(0.0f, 0.0f, 0.0f);
        e.enemyHP = defaultHP;
        e.playtime = 0.0f;
        e.isHit = false;
        e.currentAnimType = 1;

        if (e.model1 != -1) {
            MV1SetPosition(e.model1, e.pos);
            MV1SetVisible(e.model1, TRUE);
            if (e.attachidx != -1) {
                MV1SetAttachAnimTime(e.model1, e.attachidx, 0.0f);
            }
        }

        return slot;
    }

    // ② 敵が死んだ／画面外で消す時の即時リサイクル（完全成仏処理）
    void Despawn(SCharaInfo* charaArray, int activeListIndex) {
        int slot = activeIndices[activeListIndex];

        // ★データを完全に白紙化
        SCharaInfo& e = charaArray[slot];
        e.mode = NONE;
        e.enemyHP = 0;
        e.playtime = 0.0f;
        e.isHit = false;
        e.pos = VGet(-99999.0f, -99999.0f, -99999.0f); // 画面外奈落へ退避
        if (e.model1 != -1) {
            MV1SetVisible(e.model1, FALSE);
            MV1SetPosition(e.model1, e.pos);
        }

        // 空き待機リストへ返却（これで次回最優先で再利用される）
        freeIndices.push_back(slot);

        // 生存リストから削除（末尾と入れ替えてpopすることで超高速削除: O(1)）
        activeIndices[activeListIndex] = activeIndices.back();
        activeIndices.pop_back();
    }

    // 生きている敵の数
    int GetActiveCount() const { return (int)activeIndices.size(); }

    // 生きている敵のスロット番号を取得
    int GetActiveSlot(int index) const { return activeIndices[index]; }
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

    int deathCount[2] = { 0, 0 };
    int gameState = 0;

    void AddDeath(int playerIndex) {
        if (playerIndex >= 0 && playerIndex < 2) {
            deathCount[playerIndex]++;
        }
    }

    bool isHit = false;
    int anim_neutral = -1;
    int anim_walk = -1;
    int anim_attack = -1;
    int enemy_anim_neutral = -1;
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
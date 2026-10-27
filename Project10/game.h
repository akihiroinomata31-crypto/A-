#pragma once
#include "../DXLib/DxLib.h"
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
private:
  
public:
    // --- 勝敗・キル数管理 ---
    int killCount[2] = { 0, 0 }; // 各プレイヤーの撃破数

    // 撃破数加算メソッド
    void AddKill(int pIdx) {
        if (pIdx >= 0 && pIdx < 2) {
            killCount[pIdx]++;
        }
    }

    // --- 制限時間管理（120FPS基準: 100秒なら 100 * 60 = 12000） ---
    int mainTimer = 100 * 60;

    // --- 敵スポーン管理 ---
    int goblinSpawnTimer = 0; // 通常ゴブリン用カウンター
    int redSpawnTimer = 0;    // 赤ゴブリン用カウンター

    ScorePopup popups[20];

    int p1Score = 0;
    int p2Score = 0;
    std::vector<ReplayFrame> replayData;

    static constexpr int DeathPenalty = 1000;
 
    int specialUseCount[2] = { 0, 0 };
    int deathCount[2] = { 0, 0 };
    int gameState = 0; // 0: 進行中, 1: 1P勝, 2: 2P勝, 3: 引き分け

    // ★ スコア表示用専用フォントハンドル（黒文字用）
    int fontScore = -1;

    void Reset() {
        gameState = 0;
        mainTimer = 100 * 60;
        p1Score = 0;
        p2Score = 0;
        deathCount[0] = 0;
        deathCount[1] = 0;
        killCount[0] = 0;
        killCount[1] = 0;
        specialUseCount[0] = 0;
        specialUseCount[1] = 0;
        goblinSpawnTimer = 0;
        redSpawnTimer = 0;
        for (int i = 0; i < 20; i++) {
            popups[i].active = false;
        }
    }

    void AddDeath(int playerIndex) {
        if (playerIndex >= 0 && playerIndex < 2) {
            deathCount[playerIndex]++;
        }
    }

    int FinalScore(int playerIndex) const {
        if (playerIndex < 0 || playerIndex >= 2) return 0;
        const int earned = playerIndex == 0 ? p1Score : p2Score;
        return earned - deathCount[playerIndex] * DeathPenalty;
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
    int enemyBaseModel = -1;
    int enemy_anim_walk = -1;

    bool SetGameResourceDirectory();
    // game.h 内
void DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot = nullptr, int rollTimer = 0);
    // ★ 敵の頭上HPバー描画関数を追加
    void DrawEnemyHPBars(int pIdx, const SCharaInfo* charainfo);
   // void DrawSingleEnemyHPBar(const VECTOR& enemyPos, int currentHp, int maxHp, float headOffset);
    // --- 閃光弾・アイテム管理 ---
    FlashItem flashItems[3];             // フィールド上に同時に存在できるアイテム最大数
    Flashbang flashbangs[4];             // 投擲中の弾
    int itemDropTimer = 0;               // アイテムが降ってくるインターバル用
    int flashbangStock[2] = { 0, 0 };    // [0]:1Pの所持数, [1]:2Pの所持数
    int playerStunTimer[2] = { 0, 0 };   // スタン残り時間
     // ★ 開幕カウントダウン変数（120FPS基準: 3秒カウント + 0.8秒START表示 = 460フレーム）
    int startCountdownTimer = 0;
   // ★ 1.5秒演出用（120FPS基準で180フレーム、60FPS基準なら90に調整）
    static const int BOSS_CUTSCENE_TOTAL_FRAMES = 90;
    int bossCutsceneTimer;
    bool bossCutsceneDone; // ★ 1回演出したら二度と再起動しないフラグ
    static void DrawLoadingScreen();
    // 演出判定用ゲッター
    bool IsBossCutsceneActive() const { return bossCutsceneTimer > 0; }

    // カメラ座標計算（true: 演出中 / false: 通常カメラに戻す）
    bool ApplyBossCamera(VECTOR& outCamPos, VECTOR& outCamTarget, const SCharaInfo* enemyList);
    void DrawStartCountdown(int sw, int sh, int img3, int img2, int img1);
    // ★ テロップ描画関数（DrawUI の中、または分割描画ループから呼ぶ）
    void DrawBossCutsceneUI(int startX, int endX, int screenHeight) const;
    void SpawnFlashItem();
    void UpdateFlashItems(SCharaInfo* players);
    void DrawFlashItems();
    void DrawSpecialGauge(int pIdx, int startX, int endX, int screenHeight, int currentGauge, int maxGauge, int frameGraphHandle);
    void ThrowFlashbang(int playerIdx, VECTOR pPos, VECTOR forwardVec);
    void UpdateFlashbangs(SCharaInfo* players);
    void DrawFlashbangs();
    void DrawFlashOverlay(int pIdx, int startX, int startY, int endX, int endY);

    void Init();
    void Update(SCharaInfo* enemyList, SCharaInfo* players);
    void DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex, bool isPvPActive, int pvpCooldown);
    void DrawTimer(int sw, int sh);
    void UpdateVisualTimers();
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
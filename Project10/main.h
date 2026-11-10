#pragma once


#include <vector>
#include "DxLib.h"
#include "EffekseerForDXLib.h"
// プレイヤーに当たる範囲の幅と高さ
#define PC_WIDTH 80.0f
#define PC_HEIGHT 180.0f
// プレイヤーと敵を合わせて入れられる数
#define MAX_CHARA 1200

#define CHARA_ENUM_DEFAULT_SIZE     200.0f      // 周囲のポリゴン検出に使用する球の初期サイズ
#define CHARA_MAX_HITCOLL           2048        // 処理するコリジョンポリゴンの最大数

// 移動の速さと落下の強さ
#define MOVE_SPEED                  5.0f
#define GRAVITY                     0.5f

#define SOUND_DIRECTORY_PATH "..\\Data\\Sound\\"
#define ATTACK_FIRST_ENDTIME 15.0f
#define ATTACK_SECOND_ENDTIME 15.0f
#define ATTACK_THIERD_ENDTIME 30.0f

//#ifndef MAIN_H
#define MAIN_H

class GameManager;
struct SCharaInfo;

// キャラの向き direction × 90度で回す
enum Direction
{
    DOWN = 0,
    LEFT = 1,
    UP = 2,
    RIGHT = 3
};

// --- キャラの状態
// 今、待機中か移動中か攻撃中かなど
enum CharaMode
{
    STAND,
    RUN,
    JUMPIN,
    JUMPLOOP,
    JUMPOUT,
    FALL,
    ATTACK,
    ATTACKOUT,
    DAMAGE,
    DOWNMODE,
    ANIM_RUN,
    NONE,
};

/* ------------------------------------------------------------------------
| 構造体宣言
+ -------------------------------------------------------------------------- */
typedef struct
{
    float Width, Height;
    VECTOR CenterPosition;
} SCharaHitInfo;

struct SCharaInfo
{
    SCharaInfo() : angle(0.0f), anim_time(0.0f), HP(0), isHit(false), model1(-1) {}
    // ダメージを受けない時間の残り
    int invincibleTimer = 0; // 無敵時間カウンター（フレーム数）
    // 吹き飛ぶ向きと速さ
    VECTOR knockbackVelocity = {};
    // あと何回吹き飛ばすか
    int knockbackFrames = 0;
    // 読み込んだモデルの番号 -1ならまだ読み込んでいない
    int model1;
    int timeLimit = 1000;    // ゲーム制限時間など
    int spawnTimer = 0;      // スポーンカウント用
    Direction direction;
    // 今再生しているアニメの番号
    int attachidx;
    // アニメが今どこまで進んだかと全体の長さ
    float playtime = 0, anim_totaltime;
    // ステージ内の位置
    VECTOR pos;
    float rot;
    // 1回の更新でどれだけ動くか
    VECTOR move;
    // 体に当たる範囲の大きさと中心位置
    SCharaHitInfo charahitinfo;
    int mode;                // キャラの状態
    // 敵の残りHP 0以下なら倒れた扱い
    int enemyHP;
    int enemyDamageAnimation = -1;
    // ダメージを受けてから復帰するまでのカウント
    int hurtRecoveryTicks = 0;
    int enemyIdleAnimation = -1;
    int enemyWalkAnimation = -1;
    bool enemyWalking = false;
    // 前に調べた地面の高さを使えるか
    bool groundCacheValid = false;
    VECTOR groundCachePosition = {};
    // プレイヤーの残りHP 0以下でダウン
    int HP;
    float angle;
    // もう攻撃が当たったか 同じ攻撃が何度も当たるのを防ぐ
    bool isHit;
    // 死亡回数
    int deaths;
    float anim_time;
    int popups;
    float anim_total;
    int weaponModel;         // 武器のモデルハンドル
    int weaponFrame;
    int currentAnimType = 1;
    float dir;
    //int invincibleTimer; // 無敵時間（被ダメージ無効）
    // 復活してから動けるまでの時間
    int respawnFreezeTimer; // ★ 復活直後の操作不能タイマー（フレーム数）


};

extern int anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down, enemy_anim_attack, enemy_anim_run, enemy_anim_neutral;

extern void CheckAttackHit(
    GameManager& game,
    SCharaInfo* charainfo,
    SCharaInfo* attacker,
    SCharaInfo* target,
    VECTOR start,
    VECTOR end,
    int SEdamageHandle,
    int anim_damage,
    bool* hitTargets = nullptr,
    float knockbackSpeed = 0.0f
);

extern void DrawCrownIcon(int pIdx, int xOffset, int startY, int p1Score, int p2Score, int crownGraphHandle);
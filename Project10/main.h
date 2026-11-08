#pragma once


#include <vector>
#include "DxLib.h"
#include "EffekseerForDXLib.h"
#define PC_WIDTH 80.0f
#define PC_HEIGHT 180.0f
#define MAX_CHARA 1200

#define CHARA_ENUM_DEFAULT_SIZE     200.0f      // 周囲のポリゴン検出に使用する球の初期サイズ
#define CHARA_MAX_HITCOLL           2048        // 処理するコリジョンポリゴンの最大数

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

enum Direction
{
    DOWN = 0,
    LEFT = 1,
    UP = 2,
    RIGHT = 3
};

// --- キャラの状態
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
    int invincibleTimer = 0; // 無敵時間カウンター（フレーム数）
    VECTOR knockbackVelocity = {};
    int knockbackFrames = 0;
    int model1;
    int timeLimit = 1000;    // ゲーム制限時間など
    int spawnTimer = 0;      // スポーンカウント用
    Direction direction;
    int attachidx;
    float playtime = 0, anim_totaltime;
    VECTOR pos;
    float rot;
    VECTOR move;
    SCharaHitInfo charahitinfo;
    int mode;                // キャラの状態
    int enemyHP;
    int enemyDamageAnimation = -1;
    int hurtRecoveryTicks = 0;
    int enemyIdleAnimation = -1;
    int enemyWalkAnimation = -1;
    bool enemyWalking = false;
    bool groundCacheValid = false;
    VECTOR groundCachePosition = {};
    int HP;
    float angle;
    bool isHit;
    int deaths;
    float anim_time;
    int popups;
    float anim_total;
    int weaponModel;         // 武器のモデルハンドル
    int weaponFrame;
    int currentAnimType = 1;
    float dir;
    //int invincibleTimer; // 無敵時間（被ダメージ無効）
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
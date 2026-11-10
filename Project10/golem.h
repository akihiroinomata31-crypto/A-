#pragma once

#include "main.h"
#include "game.h"
#include "player.h"

extern int golemShockwaveEffectResourceHandle;

enum GolemAttackState
{
    GOLEM_IDLE,
    GOLEM_SHOCKWAVE,
    GOLEM_SPIN,
    GOLEM_CHARGE
};

class GolemManager
{
private:
    GolemAttackState state = GOLEM_IDLE;

    int attackTimer = 0;
    int attackCooldown = 0;

    float shockwaveRadius = 0.0f;
    float knockbackVelocity[2][2] = { {0.0f, 0.0f}, {0.0f, 0.0f} };

    bool shockwaveHit[2] = { false, false };
    bool spinHit[2] = { false, false };

    int damageDealt[2] = { 0, 0 };
    int knockbackTimer[2] = { 0, 0 };

    bool scoreAwarded = false;

    int chargeTimer = 0;
    int chargeCooldown = 0;

    float chargeDirX = 0.0f;
    float chargeDirZ = 0.0f;

    float chargeSpeed = 0.0f;

    bool chargeHit[2] = { false, false };

    int appearanceTimer = 0;
    const int APPEARANCE_DURATION = 180;

public:

    void Update(
        SCharaInfo& golem,
        SCharaInfo* players,
        GameManager& game
    );

    void AddDamage(int playerIndex);

    void AwardScore(GameManager& game);

    GolemAttackState GetState() const
    {
        return state;
    }


    // アニメーション管理
    int animAttach = -1;
    float animTime = 0.0f;

    int idleAnimAttach = -1;
    float idleAnimTime = 0.0f;

    int shockwaveAnimAttach = -1;
    float shockwaveAnimTime = 0.0f;

    int spinAnimAttach = -1;
    float spinAnimTime = 0.0f;

    int chargeAnimAttach = -1;
    float chargeAnimTime = 0.0f;

    int walkAnimAttach = -1;
    float walkAnimTime = 0.0f;

    // アニメーション用モデルのハンドル
    int idleAnimModel = -1;
    int shockwaveAnimModel = -1;
    int spinAnimModel = -1;
    int chargeAnimModel = -1;
    int walkAnimModel = -1;

    // 現在再生中のアニメーション
    int currentAnimAttach = -1;

    // アニメーション再生速度
    float idleAnimSpeed = 1.0f;
    float walkAnimSpeed = 0.2f;
    float shockwaveAnimSpeed = 0.7f;
    float spinAnimSpeed = 0.7f;
    float chargeAnimSpeed = 0.4f;

    // アニメーション切り替え
    void ChangeAnimation(SCharaInfo& golem, int attach);

    // 衝撃波エフェクト
    int shockwaveEffect = -1;
    int shockwaveEffectHandle = -1;
    int shockwaveEffectPlayingHandle = -1;

    // 行動選択用タイマー
    int decisionTimer = 0;

    // 次の行動を決める間隔
    int decisionInterval = 60;


};


void InitGolemAnimations(SCharaInfo& golem);

extern GolemManager golemManager;

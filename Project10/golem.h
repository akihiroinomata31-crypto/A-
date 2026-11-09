#pragma once

#include "main.h"
#include "game.h"
#include "player.h"

extern int golemChargePrepareSE;
extern int golemChargeSE;
extern int golemSpinSE;

enum GolemAttackState
{
    GOLEM_IDLE,
    GOLEM_SPIN,
    GOLEM_CHARGE,
    GOLEM_DEATH
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

    void AddDamage(int playerIndex, int damage);

    GolemAttackState GetState() const
    {
        return state;
    }

    // アニメーション管理
    int animAttach = -1;
    float animTime = 0.0f;

    int idleAnimAttach = -1;
    float idleAnimTime = 0.0f;

    int spinAnimAttach = -1;
    float spinAnimTime = 0.0f;

    int chargeAnimAttach = -1;
    float chargeAnimTime = 0.0f;

    int chargePrepareAnimAttach = -1;
    float chargePrepareAnimTime = 0.0f;

    int walkAnimAttach = -1;
    float walkAnimTime = 0.0f;

    int deathAnimAttach = -1;
    float deathAnimTime = 0.0f;

    // アニメーション用モデルのハンドル
    int idleAnimModel = -1;
    int spinAnimModel = -1;
    int chargeAnimModel = -1;
    int chargePrepareAnimModel = -1;
    int walkAnimModel = -1;
    int deathAnimModel = -1;

    // 現在再生中のアニメーション
    int currentAnimAttach = -1;

    // アニメーション再生速度
    float idleAnimSpeed = 1.0f;
    float walkAnimSpeed = 0.5f;
    float spinAnimSpeed = 0.7f;
    float chargeAnimSpeed = 0.4f;
    float chargePrepareAnimSpeed = 2.0f;
    float deathAnimSpeed = 1.0f;

    // アニメーション切り替え
    void ChangeAnimation(SCharaInfo& golem, int attach);

    // 時間帯ごとの行動モード 
    int currentTimePhase = -1;

    // 退場アニメーションの再生状態
    bool deathAnimationFinished = false;

    int chargeCount = 0;

    // 行動選択用タイマー
    int decisionTimer = 0;

    int recoveryTimer = 0;

    int chargeTargetCount = 0;

    // 次の行動を決める間隔
    int decisionInterval = 60;


};


void InitGolemAnimations(SCharaInfo& golem);

extern GolemManager golemManager;


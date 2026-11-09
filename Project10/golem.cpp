#include "golem.h"
#include <math.h>
#include <EffekseerForDXLib.h>

GolemManager golemManager;


// ゴーレムをプレイヤーの方向に向ける
void FaceGolemToPlayer(SCharaInfo& golem, SCharaInfo& player)
{
    float dx = player.pos.x - golem.pos.x;
    float dz = player.pos.z - golem.pos.z;

    float angle = atan2f(dx, dz);

    // モデルの正面が逆向きの場合は180度回転
    angle += DX_PI_F;

    golem.dir = angle;

    MV1SetRotationXYZ(
        golem.model1,
        VGet(0.0f, golem.dir, 0.0f)
    );
}

void InitGolemAnimations(SCharaInfo& golem) {

    // アニメーション
    golemManager.idleAnimModel = MV1LoadModel("..\\Data\\Golem\\Idle.mv1");

    golemManager.spinAnimModel = MV1LoadModel("..\\Data\\Golem\\Spin.mv1");

    golemManager.chargeAnimModel = MV1LoadModel("..\\Data\\Golem\\charge.mv1");

    golemManager.chargePrepareAnimModel = MV1LoadModel("..\\Data\\Golem\\ChargePrepare.mv1");

    golemManager.walkAnimModel = MV1LoadModel("..\\Data\\Golem\\Walk.mv1");

    golemManager.deathAnimModel = MV1LoadModel("..\\Data\\Golem\\Death.mv1");

    // 効果音
    golemChargePrepareSE = LoadSoundMem("..\\Data\\Golem\\");

    golemChargeSE = LoadSoundMem("..\\Data\\Golem\\");

    golemSpinSE = LoadSoundMem("..\\Data\\Golem\\");

    if (golemManager.idleAnimModel == -1 ||
        golemManager.spinAnimModel == -1 ||
        golemManager.chargeAnimModel == -1 ||
        golemManager.chargePrepareAnimModel == -1)

    {

        //    printfDx("Golem animation model load failed\n");
        return;
    }

    golemManager.idleAnimAttach =
        MV1AttachAnim(
            golem.model1, 0,
            golemManager.idleAnimModel, -1
        );

    golemManager.spinAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.spinAnimModel, -1
        );

    golemManager.chargeAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.chargeAnimModel, -1
        );

    golemManager.chargePrepareAnimAttach =
        MV1AttachAnim(
            golem.model1, 0,
            golemManager.chargePrepareAnimModel, -1
        );

    golemManager.walkAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.walkAnimModel, -1
        );

    golemManager.deathAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.deathAnimModel, -1
        );

    if (golemManager.idleAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.idleAnimAttach, 0.0f);

    if (golemManager.spinAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.spinAnimAttach, 0.0f);

    if (golemManager.chargeAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.chargeAnimAttach, 0.0f);

    if (golemManager.chargePrepareAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.chargePrepareAnimAttach, 0.0f);

    if (golemManager.walkAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.walkAnimAttach, 0.0f);

    if (golemManager.deathAnimAttach != -1) { MV1SetAttachAnimBlendRate(golem.model1, golemManager.deathAnimAttach, 0.0f); }


    // 待機アニメーションだけ有効にする
    golemManager.currentAnimAttach = -1;

    golemManager.idleAnimTime = 0.0f;
    golemManager.spinAnimTime = 0.0f;
    golemManager.chargeAnimTime = 0.0f;
    golemManager.walkAnimTime = 0.0f;

    golemManager.ChangeAnimation(golem, golemManager.idleAnimAttach);

    //  printfDx("Golem animations initialized\n");

}

void GolemManager::ChangeAnimation(
    SCharaInfo& golem,
    int attach)
{
    if (attach == -1)
        return;

    if (currentAnimAttach == attach)
        return;

    if (currentAnimAttach != -1)
    {
        MV1SetAttachAnimBlendRate(
            golem.model1,
            currentAnimAttach,
            0.0f
        );
    }

    currentAnimAttach = attach;

    if (attach == idleAnimAttach)
        idleAnimTime = 0.0f;
    else if (attach == spinAnimAttach)
        spinAnimTime = 0.0f;
    else if (attach == chargeAnimAttach)
        chargeAnimTime = 0.0f;
    else if (attach == chargePrepareAnimAttach)
        chargePrepareAnimTime = 0.0f;
    else if (attach == walkAnimAttach)
        walkAnimTime = 0.0f;

    MV1SetAttachAnimTime(
        golem.model1,
        attach,
        0.0f
    );

    MV1SetAttachAnimBlendRate(
        golem.model1,
        attach,
        1.0f
    );
}


void UpdateGolemAnimation(SCharaInfo& golem, float speedMultiplier)
{
    int attach = golemManager.currentAnimAttach;

    if (attach == -1)
        return;

    // 退場アニメーションはループさせない
    if (attach == golemManager.deathAnimAttach)
    {
        float totalTime = MV1GetAttachAnimTotalTime(
            golem.model1,
            attach
        );

        if (totalTime <= 0.0f)
            return;

        if (!golemManager.deathAnimationFinished)
        {
            golemManager.deathAnimTime +=
                golemManager.deathAnimSpeed;

            if (golemManager.deathAnimTime >= totalTime)
            {
                golemManager.deathAnimTime = totalTime;
                golemManager.deathAnimationFinished = true;
            }

            MV1SetAttachAnimTime(
                golem.model1,
                attach,
                golemManager.deathAnimTime
            );
        }

        return;
    }

    float* animTime = nullptr;
    float animSpeed = 1.0f;

    if (attach == golemManager.idleAnimAttach)
    {
        animTime = &golemManager.idleAnimTime;
        animSpeed = golemManager.idleAnimSpeed;
    }
    else if (attach == golemManager.spinAnimAttach)
    {
        animTime = &golemManager.spinAnimTime;
        animSpeed = golemManager.spinAnimSpeed;
    }
    else if (attach == golemManager.chargeAnimAttach)
    {
        animTime = &golemManager.chargeAnimTime;
        animSpeed = golemManager.chargeAnimSpeed;
    }
    else if (attach == golemManager.chargePrepareAnimAttach)
    {
        animTime = &golemManager.chargePrepareAnimTime;
        animSpeed = golemManager.chargePrepareAnimSpeed;
    }
    else if (attach == golemManager.walkAnimAttach)
    {
        animTime = &golemManager.walkAnimTime;
        animSpeed = golemManager.walkAnimSpeed;
    }

    if (animTime == nullptr)
        return;

    float totalTime = MV1GetAttachAnimTotalTime(
        golem.model1,
        attach
    );

    if (totalTime <= 0.0f)
        return;

    // アニメーション再生時間を進める
    *animTime += animSpeed * speedMultiplier;

    // ループ再生
    while (*animTime >= totalTime)
    {
        *animTime -= totalTime;
    }

    MV1SetAttachAnimTime(
        golem.model1,
        attach,
        *animTime
    );
}

int golemChargePrepareSE = -1;
int golemChargeSE = -1;
int golemSpinSE = -1;


// ダメージ記録
void GolemManager::AddDamage(int playerIndex, int damage)
{
    if (playerIndex < 0 || playerIndex >= PLAYER_COUNT)
        return;

    damageDealt[playerIndex] += damage;
}


void GolemManager::Update(SCharaInfo& golem, SCharaInfo* players, GameManager& game)
{
    // ゴーレムがすでに消えている場合
    if (golem.mode == NONE)
    {
        appearanceTimer = 0;
        return;
    }
    int remainingSeconds = game.mainTimer / 60;

    // 退場の秒数
    if ((golem.enemyHP <= 0 || remainingSeconds <= 14) &&
        state != GOLEM_DEATH)
    {
        state = GOLEM_DEATH;
        attackTimer = 0;

        deathAnimTime = 0.0f;
        deathAnimationFinished = false;

        if (deathAnimAttach != -1)
        {
            ChangeAnimation(golem, deathAnimAttach);
        }
        else
        {
            // アニメーションを読み込めなかった場合
            deathAnimationFinished = true;
        }
    }

    // 退場中は通常の攻撃処理を実行しない
    if (state == GOLEM_DEATH)
    {
        if (deathAnimAttach != -1 &&
            !deathAnimationFinished)
        {
            UpdateGolemAnimation(golem, 1.0f);
        }

        if (deathAnimationFinished)
        {
            MV1SetVisible(golem.model1, FALSE);
            golem.mode = NONE;
        }

        return;
    }

    if (golem.mode == NONE) {
        appearanceTimer = 0;
        return;
    }

    // 登場演出中は待機アニメーションを再生
    if (appearanceTimer < APPEARANCE_DURATION) {
        appearanceTimer++;

        if (currentAnimAttach != idleAnimAttach) {
            ChangeAnimation(golem, idleAnimAttach);
        }

        UpdateGolemAnimation(golem, 1.0f);
        return;
    }

    // 登場後は通常処理
    UpdateGolemAnimation(golem, 1.0f);

    if (game.gameState != 0)
    {
        return;
    }

    // ノックバック処理
    for (int i = 0; i < PLAYER_COUNT; i++)
    {
        if (knockbackTimer[i] > 0)
        {
            players[i].pos.x += knockbackVelocity[i][0];
            players[i].pos.z += knockbackVelocity[i][1];

            knockbackVelocity[i][0] *= 0.90f;
            knockbackVelocity[i][1] *= 0.90f;

            knockbackTimer[i]--;
        }
    }

    if (attackCooldown > 0)
        attackCooldown--;

    if (attackCooldown < 0)
        attackCooldown = 0;

    float nearestDistance = 999999.0f;
    int nearestPlayer = -1;

    for (int i = 0; i < PLAYER_COUNT; i++)
    {
        if (players[i].mode == NONE ||
            players[i].mode == DOWNMODE)
        {
            continue;
        }

        float dx = players[i].pos.x - golem.pos.x;
        float dz = players[i].pos.z - golem.pos.z;

        float distance = sqrtf(dx * dx + dz * dz);

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestPlayer = i;
        }
    }

    // 近接攻撃 
    if (state == GOLEM_SPIN)
    {
        attackTimer++;

        if (attackTimer >= 50 && attackTimer <= 90) {
            if (attackTimer >= 30 && attackTimer <= 90)
            {
                for (int i = 0; i < PLAYER_COUNT; i++)
                {
                    if (players[i].mode == DOWNMODE)
                        continue;

                    if (spinHit[i])
                        continue;

                    float dx = players[i].pos.x - golem.pos.x;
                    float dz = players[i].pos.z - golem.pos.z;

                    float distanceSq = dx * dx + dz * dz;

                    const float SPIN_RANGE = 500.0f;

                    if (distanceSq <= SPIN_RANGE * SPIN_RANGE)
                    {
                        spinHit[i] = true;

                        // HPを1減らす
                        if (players[i].HP > 0)
                        {
                            players[i].HP--;
                        }


                        float distance = sqrtf(distanceSq);

                        if (distance > 0.001f)
                        {
                            // ノックバック量
                            float knockback = 80.0f;

                            knockbackVelocity[i][0] =
                                (dx / distance) * knockback;

                            knockbackVelocity[i][1] =
                                (dz / distance) * knockback;

                            knockbackTimer[i] = 12;
                        }
                    }
                }
            }
        }
        // 攻撃判定


        // 近接攻撃終了
        if (attackTimer >= 120)
        {
            state = GOLEM_IDLE;
            attackTimer = 0;

            ChangeAnimation(golem, idleAnimAttach);

            attackCooldown = 120;

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                spinHit[i] = false;
            }
        }

        return;
    }

    // 突進攻撃
    if (state == GOLEM_CHARGE)
    {
        // 突進攻撃は強化モードでも通常速度
        attackTimer++;

        const int CHARGE_PREPARE_TIME = 50;
        const int CHARGE_DURATION = 200;
        const float CHARGE_SPEED = 25.0f;

        // 最初の突進だけタメアニメーションを再生
        if (chargeCount == 0 &&
            attackTimer <= CHARGE_PREPARE_TIME)
        {
            if (currentAnimAttach != chargePrepareAnimAttach)
            {
                ChangeAnimation(golem, chargePrepareAnimAttach);
                // タメ開始SE
                PlaySoundMem(golemChargePrepareSE, DX_PLAYTYPE_BACK);
            }
        }

        // タメ終了後、突進アニメーションに切り替える
        if (chargeCount == 0 &&
            attackTimer == CHARGE_PREPARE_TIME + 1)
        {
            ChangeAnimation(golem, chargeAnimAttach);
            // 突進開始SE
            PlaySoundMem(golemChargeSE, DX_PLAYTYPE_BACK);
        }

        // タメが終わったら突進
        if (attackTimer > CHARGE_PREPARE_TIME ||
            chargeCount > 0)
        {
            chargeSpeed = CHARGE_SPEED;

            golem.pos.x += chargeDirX * chargeSpeed;
            golem.pos.z += chargeDirZ * chargeSpeed;

            MV1SetPosition(golem.model1, golem.pos);

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                if (players[i].mode == NONE ||
                    players[i].mode == DOWNMODE)
                    continue;

                if (chargeHit[i])
                    continue;

                float dx = players[i].pos.x - golem.pos.x;
                float dz = players[i].pos.z - golem.pos.z;
                float distanceSq = dx * dx + dz * dz;

                const float CHARGE_RANGE = 300.0f;

                if (distanceSq <= CHARGE_RANGE * CHARGE_RANGE)
                {
                    chargeHit[i] = true;

                    if (players[i].HP > 0)
                        players[i].HP--;

                    knockbackVelocity[i][0] = chargeDirX * 200.0f;
                    knockbackVelocity[i][1] = chargeDirZ * 200.0f;
                    knockbackTimer[i] = 15;
                }
            }
        }

        // 1回の突進が終了
        if (attackTimer >= CHARGE_PREPARE_TIME + CHARGE_DURATION)
        {
            chargeCount++;

            int maxChargeCount = 1;

            if (chargeCount < maxChargeCount)
            {
                attackTimer = 0;

                // 次の突進前にプレイヤーの方向を取り直す
                if (nearestPlayer != -1)
                {
                    float dx = players[nearestPlayer].pos.x - golem.pos.x;
                    float dz = players[nearestPlayer].pos.z - golem.pos.z;
                    float length = sqrtf(dx * dx + dz * dz);

                    if (length > 0.001f)
                    {
                        chargeDirX = dx / length;
                        chargeDirZ = dz / length;
                    }
                }

                for (int i = 0; i < PLAYER_COUNT; i++)
                    chargeHit[i] = false;
            }
            else
            {
                // 突進終了
                chargeCount = 0;
                state = GOLEM_IDLE;
                attackTimer = 0;

                ChangeAnimation(golem, idleAnimAttach);

                chargeSpeed = 0.0f;
                attackCooldown = 120;
                recoveryTimer = 60;

                for (int i = 0; i < PLAYER_COUNT; i++)
                    chargeHit[i] = false;
            }

        }


        return;
    }

    // 突進後の硬直
    if (recoveryTimer > 0)
    {
        recoveryTimer--;
        return;
    }

    // 待機中
    if (state == GOLEM_IDLE)
    {
        if (nearestPlayer == -1)
            return;

        FaceGolemToPlayer(golem, players[nearestPlayer]);

        // 移動速度
        const float GOLEM_MOVE_SPEED = 6.0f;
        const float GOLEM_STOP_DISTANCE = 500.0f;

        // 移動・アニメーション切り替え
        if (nearestDistance > GOLEM_STOP_DISTANCE &&
            nearestDistance <= 1000.0f)
        {
            float dx = players[nearestPlayer].pos.x - golem.pos.x;
            float dz = players[nearestPlayer].pos.z - golem.pos.z;

            float distance = sqrtf(dx * dx + dz * dz);

            if (distance > GOLEM_STOP_DISTANCE && distance > 0.001f)
            {
                // 歩行アニメーションに切り替え
                ChangeAnimation(golem, walkAnimAttach);

                golem.pos.x += dx / distance * GOLEM_MOVE_SPEED;
                golem.pos.z += dz / distance * GOLEM_MOVE_SPEED;

                MV1SetPosition(golem.model1, golem.pos);
            }
        }
        else
        {
            ChangeAnimation(golem, idleAnimAttach);
        }

        decisionTimer++;

        if (decisionTimer < decisionInterval)
            return;

        decisionTimer = 0;

        if (attackCooldown > 0)
            return;

        int action = GetRand(99);

        // 確率で何もしない
        if (action < 10)
        {
            return;
        }

        // 距離によって攻撃を使い分ける
        if (nearestDistance >= 700.0f || GetRand(99) < 30)
        {
            // 中・遠距離、または近距離に確率で突進攻撃
            state = GOLEM_CHARGE;
            attackTimer = 0;
            chargeCount = 0;

            ChangeAnimation(golem, chargePrepareAnimAttach);

            chargeTargetCount++;

            // 3回に1回は必ずプレイヤーを狙う
            // それ以外は50%の確率でプレイヤーを狙う
            bool targetPlayer = false;

            if (chargeTargetCount % 3 == 0)
            {
                targetPlayer = true;
            }
            else
            {
                targetPlayer = (GetRand(99) < 50);
            }

            if (targetPlayer)
            {
                // プレイヤーの方向
                float dx = players[nearestPlayer].pos.x - golem.pos.x;
                float dz = players[nearestPlayer].pos.z - golem.pos.z;
                float length = sqrtf(dx * dx + dz * dz);

                if (length > 0.001f)
                {
                    chargeDirX = dx / length;
                    chargeDirZ = dz / length;
                }
            }
            else
            {
                // ランダムな方向
                float angle = GetRand(6283) / 1000.0f;
                chargeDirX = sinf(angle);
                chargeDirZ = cosf(angle);
            }

            golem.dir = atan2f(chargeDirX, chargeDirZ) + DX_PI_F;

            MV1SetRotationXYZ(
                golem.model1,
                VGet(0.0f, golem.dir, 0.0f)
            );
        }
        else
        {
            // 近距離：近接攻撃
            state = GOLEM_SPIN;
            attackTimer = 0;

            spinHit[0] = false;
            spinHit[1] = false;

            ChangeAnimation(golem, spinAnimAttach);
            // 近接攻撃SE
            PlaySoundMem(golemSpinSE, DX_PLAYTYPE_BACK);

        }
    }
}

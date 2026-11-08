#include "golem.h"
#include <math.h>

GolemManager golemManager;


// ゴーレムをプレイヤーの方向に向ける
void FaceGolemToPlayer(SCharaInfo& golem, SCharaInfo& player)
{
    float dx = player.pos.x - golem.pos.x;
    float dz = player.pos.z - golem.pos.z;

    float angle = atan2f(dx, dz);

    // モデルの正面が反対向きの場合に備えて180度補正
    angle += DX_PI_F;

    // ゴーレムの向きを保存
    golem.dir = angle;

    // モデルを回転させる
    MV1SetRotationXYZ(
        golem.model1,
        VGet(0.0f, angle, 0.0f)
    );
}


void InitGolemAnimations(SCharaInfo& golem) {

    golemManager.idleAnimModel =
        MV1LoadModel("..\\Data\\Golem\\Idle.mv1");

    golemManager.shockwaveAnimModel =
        MV1LoadModel("..\\Data\\Golem\\Shockwave.mv1");

    golemManager.spinAnimModel =
        MV1LoadModel("..\\Data\\Golem\\Spin.mv1");

    golemManager.chargeAnimModel =
        MV1LoadModel("..\\Data\\Golem\\charge.mv1");

    golemManager.walkAnimModel =
        MV1LoadModel("..\\Data\\Golem\\Walk.mv1");

    if (golemManager.idleAnimModel == -1 ||
        golemManager.shockwaveAnimModel == -1 ||
        golemManager.spinAnimModel == -1 ||
        golemManager.chargeAnimModel == -1 ||
        golemManager.walkAnimModel == -1)
    {
        printfDx("Golem animation model load failed\n");
        return;
    }

    golemManager.idleAnimAttach =
        MV1AttachAnim(
            golem.model1, 0,
            golemManager.idleAnimModel, -1
        );

    golemManager.shockwaveAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.shockwaveAnimModel, -1
        );

    golemManager.spinAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.spinAnimModel, -1
        );

    golemManager.chargeAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.chargeAnimModel, -1
        );

    golemManager.walkAnimAttach =
        MV1AttachAnim(golem.model1, 0,
            golemManager.walkAnimModel, -1
        );

    if (golemManager.idleAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.idleAnimAttach, 0.0f);

    if (golemManager.shockwaveAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.shockwaveAnimAttach, 0.0f);

    if (golemManager.spinAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.spinAnimAttach, 0.0f);

    if (golemManager.chargeAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.chargeAnimAttach, 0.0f);

    if (golemManager.walkAnimAttach != -1)
        MV1SetAttachAnimBlendRate(golem.model1, golemManager.walkAnimAttach, 0.0f);


    // 待機アニメーションだけ有効にする
    golemManager.currentAnimAttach = -1;
    golemManager.ChangeAnimation(
        golem,
        golemManager.idleAnimAttach
    );

    golemManager.idleAnimTime = 0.0f;
    golemManager.shockwaveAnimTime = 0.0f;
    golemManager.spinAnimTime = 0.0f;
    golemManager.chargeAnimTime = 0.0f;
    golemManager.walkAnimTime = 0.0f;
    golemManager.currentAnimAttach = -1;


    golemManager.ChangeAnimation(golem, golemManager.idleAnimAttach
    );

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
    else if (attach == shockwaveAnimAttach)
        shockwaveAnimTime = 0.0f;
    else if (attach == spinAnimAttach)
        spinAnimTime = 0.0f;
    else if (attach == chargeAnimAttach)
        chargeAnimTime = 0.0f;
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


void UpdateGolemAnimation(SCharaInfo& golem)
{
    int attach = golemManager.currentAnimAttach;

    if (attach == -1)
        return;

    float* animTime = nullptr;
    float animSpeed = 1.0f;

    if (attach == golemManager.idleAnimAttach)
    {
        animTime = &golemManager.idleAnimTime;
        animSpeed = golemManager.idleAnimSpeed;
    }
    else if (attach == golemManager.shockwaveAnimAttach)
    {
        animTime = &golemManager.shockwaveAnimTime;
        animSpeed = golemManager.shockwaveAnimSpeed;
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

    *animTime += animSpeed;

    // アニメーションループ
    if (*animTime >= totalTime)
        *animTime = fmodf(*animTime, totalTime);

    MV1SetAttachAnimTime(
        golem.model1,
        attach,
        *animTime
    );
}

// ダメージ記録
void GolemManager::AddDamage(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= PLAYER_COUNT)
        return;

    damageDealt[playerIndex]++;
}


void GolemManager::Update(SCharaInfo& golem, SCharaInfo* players, GameManager& game)
{

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

        UpdateGolemAnimation(golem);
        return;
    }

    // 登場後は通常処理
    UpdateGolemAnimation(golem);

    if (game.gameState != 0)
    {
        AwardScore(game);
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

    // 衝撃波
    if (state == GOLEM_SHOCKWAVE)
    {
        attackTimer++;

        // 衝撃波が発生するまで
        const int SHOCKWAVE_DELAY = 120;

        if (attackTimer <= SHOCKWAVE_DELAY)
        {
            shockwaveRadius = 0.0f;
            return;
        }

        const float SHOCKWAVE_MAX_RADIUS = 9999.0f;
        const int SHOCKWAVE_DURATION = 100;

        int elapsed = attackTimer - SHOCKWAVE_DELAY;

        shockwaveRadius =
            SHOCKWAVE_MAX_RADIUS *
            ((float)elapsed / (float)SHOCKWAVE_DURATION);

        // ジャンプに合わせて前へ
        const int JUMP_MOVE_START = 20;  // 移動開始
        const int JUMP_MOVE_END = 45;    // 移動終了
        const float JUMP_MOVE_SPEED = 40.0f;

        if (attackTimer >= JUMP_MOVE_START &&
            attackTimer <= JUMP_MOVE_END)
        {
            float forwardX = -sinf(golem.dir);
            float forwardZ = -cosf(golem.dir);

            golem.pos.x += forwardX * JUMP_MOVE_SPEED;
            golem.pos.z += forwardZ * JUMP_MOVE_SPEED;

            MV1SetPosition(golem.model1, golem.pos);

            MV1SetPosition(golem.model1, golem.pos);
        }

        shockwaveRadius =
            SHOCKWAVE_MAX_RADIUS *
            ((float)attackTimer / (float)SHOCKWAVE_DURATION);

        for (int i = 0; i < PLAYER_COUNT; i++)
        {
            if (players[i].mode == DOWNMODE)
                continue;

            if (shockwaveHit[i])
                continue;

            float dx = players[i].pos.x - golem.pos.x;
            float dz = players[i].pos.z - golem.pos.z;

            float distance = sqrtf(dx * dx + dz * dz);

            if (distance <= shockwaveRadius)
            {


                // ジャンプ中なら回避
                if (players[i].mode == JUMPIN ||
                    players[i].mode == JUMPLOOP ||
                    players[i].mode == JUMPOUT)
                {
                    shockwaveHit[i] = true;
                    continue;
                }

                shockwaveHit[i] = true;

                float length = sqrtf(dx * dx + dz * dz);

                if (length > 0.001f)
                {
                    float knockback = 60.0f;

                    knockbackVelocity[i][0] =
                        (dx / length) * knockback;

                    knockbackVelocity[i][1] =
                        (dz / length) * knockback;

                    knockbackTimer[i] = 20;
                }
            }
        }

        // 衝撃波終了
        if (attackTimer >= SHOCKWAVE_DURATION)
        {
            state = GOLEM_IDLE;
            attackTimer = 0;

            ChangeAnimation(golem, idleAnimAttach);

            shockwaveRadius = 0.0f;

            attackCooldown = 100;

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                shockwaveHit[i] = false;
            }
        }

        return;
    }

    // 回転攻撃
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

                    const float SPIN_RANGE = 450.0f;

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
                            float knockback = 40.0f;

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


        // 回転終了
        if (attackTimer >= 120)
        {
            state = GOLEM_IDLE;
            attackTimer = 0;

            ChangeAnimation(golem, idleAnimAttach);

            attackCooldown = 240;

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
        attackTimer++;

        // 予備動作
        const int CHARGE_PREPARE_TIME = 20;

        // 突進時間
        const int CHARGE_DURATION = 35;

        // 突進速度
        const float CHARGE_SPEED = 15.0f;

        // 予備動作が終わったら突進
        if (attackTimer > CHARGE_PREPARE_TIME &&
            attackTimer <= CHARGE_PREPARE_TIME + CHARGE_DURATION)
        {
            chargeSpeed = CHARGE_SPEED;

            golem.pos.x += chargeDirX * chargeSpeed;
            golem.pos.z += chargeDirZ * chargeSpeed;

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                if (players[i].mode == DOWNMODE)
                    continue;

                if (chargeHit[i])
                    continue;

                float dx = players[i].pos.x - golem.pos.x;
                float dz = players[i].pos.z - golem.pos.z;

                float distanceSq = dx * dx + dz * dz;

                const float CHARGE_RANGE = 250.0f;

                if (distanceSq <= CHARGE_RANGE * CHARGE_RANGE)
                {
                    chargeHit[i] = true;

                    // HPを-1
                    if (players[i].HP > 0)
                    {
                        players[i].HP--;
                    }

                    // ノックバック
                    knockbackVelocity[i][0] = chargeDirX * 40.0f;
                    knockbackVelocity[i][1] = chargeDirZ * 40.0f;

                    knockbackTimer[i] = 15;

                    //           printfDx("GOLEM CHARGE HIT PLAYER %d\n", i);
                }
            }
        }

        // 突進終了
        if (attackTimer >= CHARGE_PREPARE_TIME + CHARGE_DURATION)
        {
            state = GOLEM_IDLE;
            attackTimer = 0;

            ChangeAnimation(golem, idleAnimAttach);

            chargeSpeed = 0.0f;
            attackCooldown = 240;

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                chargeHit[i] = false;
            }

            //    printfDx("GOLEM CHARGE END\n");
        }

        return;
    }

    // 待機中
    if (state == GOLEM_IDLE)
    {
        if (nearestPlayer == -1)
            return;

        FaceGolemToPlayer(golem, players[nearestPlayer]);

        const float GOLEM_MOVE_SPEED = 0.7f;
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
        if (action < 40)
        {
            return;
        }

        // 遠距離なら衝撃波
        if (nearestDistance > 1000.0f)
        {
            // 確率で衝撃波
            if (GetRand(99) < 50)
            {
                state = GOLEM_SHOCKWAVE;
                attackTimer = 0;

                ChangeAnimation(golem, shockwaveAnimAttach);

                shockwaveRadius = 0.0f;

                for (int i = 0; i < PLAYER_COUNT; i++)
                {
                    shockwaveHit[i] = false;
                }

                printfDx("衝撃波スタート\n");
            }

            return;
        }

        // 遠距離・中距離・近距離で攻撃を使い分ける
        if (nearestDistance >= 1000.0f)
        {
            // 遠距離：衝撃波
            state = GOLEM_SHOCKWAVE;
            attackTimer = 0;

            ChangeAnimation(golem, shockwaveAnimAttach);

            shockwaveRadius = 0.0f;

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                shockwaveHit[i] = false;
            }

            printfDx("衝撃波スタート\n");
        }
        else if (nearestDistance >= 700.0f)
        {
            // 中距離：突進
            state = GOLEM_CHARGE;
            attackTimer = 0;

            ChangeAnimation(golem, chargeAnimAttach);

            // プレイヤーの方向を記録
            float dx = players[nearestPlayer].pos.x - golem.pos.x;
            float dz = players[nearestPlayer].pos.z - golem.pos.z;

            float length = sqrtf(dx * dx + dz * dz);

            if (length > 0.001f)
            {
                chargeDirX = dx / length;
                chargeDirZ = dz / length;
            }
            else
            {
                chargeDirX = 0.0f;
                chargeDirZ = 0.0f;
            }

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                chargeHit[i] = false;
            }

        }
        else
        {
            // 近距離：前方攻撃
            state = GOLEM_SPIN;
            attackTimer = 0;

            ChangeAnimation(golem, spinAnimAttach);

            for (int i = 0; i < PLAYER_COUNT; i++)
            {
                spinHit[i] = false;
            }
        }
    }
}


//ゴーレムへのダメージ割合で10000点を分配
void GolemManager::AwardScore(GameManager& game)
{
    if (scoreAwarded)
        return;

    scoreAwarded = true;

    int p1Damage = damageDealt[0];
    int p2Damage = damageDealt[1];

    int totalDamage = p1Damage + p2Damage;

    //誰も攻撃していない
    if (totalDamage <= 0)
        return;

    //P1が多く攻撃
    if (p1Damage > p2Damage)
    {
        int score = p1Damage * 10000 / totalDamage;

        game.AddScore(PLAYER1_INDEX, score);
        game.AddScorePopup(PLAYER1_INDEX, score);
    }

    //P2が多く攻撃
    else if (p2Damage > p1Damage)
    {
        int score = p2Damage * 10000 / totalDamage;

        game.AddScore(PLAYER2_INDEX, score);
        game.AddScorePopup(PLAYER2_INDEX, score);
    }

    //同じなら両方に5000点
    else
    {
        game.AddScore(PLAYER1_INDEX, 5000);
        game.AddScorePopup(PLAYER1_INDEX, 5000);

        game.AddScore(PLAYER2_INDEX, 5000);
        game.AddScorePopup(PLAYER2_INDEX, 5000);
    }
}
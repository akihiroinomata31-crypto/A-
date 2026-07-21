#pragma once

#include "DxLib.h"

#include <algorithm>
#include <cmath>

namespace CharacterCombat {

// キャラクター用の基本パラメータ。
// UI、マップ、ゲーム進行ではなく、キャラクターの戦闘数値だけを管理する。
constexpr int kPlayerAttackPower = 150;
constexpr int kPlayerSpecialPower = 300;
constexpr int kMobHp = 200;
constexpr int kBossHp = 1000;

// モデル表示用の倍率。
// プレイヤーは雑兵より少し大きく、Boss は高得点目標として大きめにする。
constexpr float kMobScale = 1.0f;
constexpr float kPlayerScale = 1.1f;
constexpr float kBossScale = 2.0f;

// 攻撃判定用の仮パラメータ。
// モデル完成前でも、攻撃範囲と受け判定を先にテストできるようにする。
constexpr float kBaseHitRadius = 35.0f;
constexpr float kBaseHitHeight = 120.0f;
constexpr float kNormalAttackRange = 120.0f;
constexpr float kNormalAttackHalfWidth = 45.0f;
constexpr float kSpecialAttackRadius = 170.0f;

// キャラクターの種類。
// 現時点ではプレイヤー、雑兵、Boss の3種類を扱う。
enum class CharacterType {
    Player,
    Mob,
    Boss,
};

// 攻撃の種類。
// 通常攻撃と必殺技でダメージと攻撃判定を分ける。
enum class AttackType {
    Normal,
    Special,
};

// キャラクターの現在状態。
// HP、最大HP、モデル倍率、生存状態をまとめて持つ。
struct CharacterStatus {
    CharacterType type = CharacterType::Mob;
    int hp = kMobHp;
    int maxHp = kMobHp;
    float scale = kMobScale;
    bool alive = true;
};

// 攻撃アニメーション中の状態。
// isHitDone は「この攻撃でダメージ処理をもう行ったか」を表すフラグ。
struct AttackState {
    bool active = false;
    bool isHitDone = false;
    AttackType attackType = AttackType::Normal;
};

// 攻撃判定に使うキャラクターの当たり判定。
// position は足元中心、height は上方向の高さ、radius は体の太さとして扱う。
struct HitBody {
    VECTOR position;
    float radius = kBaseHitRadius;
    float height = kBaseHitHeight;
};

// 攻撃処理の結果。
// 命中したか、倒したか、与えたダメージ、残りHPを返す。
struct AttackResult {
    bool hit = false;
    bool defeated = false;
    int damage = 0;
    int remainingHp = 0;
};

// プレイヤー用の初期ステータスを作る。
// プレイヤーHPはまだ仕様未確定なので、仮値として1を入れている。
inline CharacterStatus MakePlayerStatus() {
    CharacterStatus status;
    status.type = CharacterType::Player;
    status.hp = 1;
    status.maxHp = 1;
    status.scale = kPlayerScale;
    return status;
}

// 雑兵用の初期ステータスを作る。
// HP200なので、通常攻撃150では2回で倒せる。
inline CharacterStatus MakeMobStatus() {
    CharacterStatus status;
    status.type = CharacterType::Mob;
    status.hp = kMobHp;
    status.maxHp = kMobHp;
    status.scale = kMobScale;
    return status;
}

// Boss用の初期ステータスを作る。
// HP1000なので、通常攻撃150では7回で倒せる。
inline CharacterStatus MakeBossStatus() {
    CharacterStatus status;
    status.type = CharacterType::Boss;
    status.hp = kBossHp;
    status.maxHp = kBossHp;
    status.scale = kBossScale;
    return status;
}

// 攻撃種別からダメージ値を取得する。
inline int GetAttackDamage(AttackType attackType) {
    return attackType == AttackType::Special ? kPlayerSpecialPower : kPlayerAttackPower;
}

// 指定HPの敵を倒すために必要な攻撃回数を計算する。
inline int GetHitsToDefeat(int hp, AttackType attackType) {
    const int damage = GetAttackDamage(attackType);
    if (hp <= 0 || damage <= 0) {
        return 0;
    }
    return (hp + damage - 1) / damage;
}

// 攻撃開始時に呼ぶ。
// 新しい攻撃なので isHitDone を false に戻す。
inline void BeginAttack(AttackState& attackState, AttackType attackType) {
    attackState.active = true;
    attackState.isHitDone = false;
    attackState.attackType = attackType;
}

// 攻撃終了時に呼ぶ。
// 次の攻撃に古い判定状態を持ち越さないようにする。
inline void FinishAttack(AttackState& attackState) {
    attackState.active = false;
    attackState.isHitDone = false;
}

// この攻撃でまだダメージ処理できるかを確認する。
inline bool CanApplyAttackHit(const AttackState& attackState) {
    return attackState.active && !attackState.isHitDone;
}

// 命中後に呼ぶ。
// 同じ攻撃アニメーション中の連続ダメージを防ぐ。
inline void MarkHitDone(AttackState& attackState) {
    attackState.isHitDone = true;
}

// キャラクターの現在状態から、攻撃判定用の体判定を作る。
// モデルの大きさに合わせて半径と高さも倍率をかける。
inline HitBody MakeHitBody(const CharacterStatus& status, const VECTOR& position) {
    HitBody body;
    body.position = position;
    body.radius = kBaseHitRadius * status.scale;
    body.height = kBaseHitHeight * status.scale;
    return body;
}

// 体判定の上端座標を取得する。
// DxLib のカプセル判定に渡すために使う。
inline VECTOR GetHitBodyTop(const HitBody& body) {
    return VAdd(body.position, VGet(0.0f, body.height, 0.0f));
}

// XZ平面だけで距離を計算する。
// 俯瞰視点の地上戦なので、攻撃範囲判定では高さより横方向を重視する。
inline float DistanceXZ(const VECTOR& a, const VECTOR& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// XZ平面上の向きを正規化する。
// 向きがゼロの場合は、仮にZ+方向を正面として扱う。
inline VECTOR NormalizeDirectionXZ(const VECTOR& direction) {
    const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (length <= 0.0001f) {
        return VGet(0.0f, 0.0f, 1.0f);
    }
    return VGet(direction.x / length, 0.0f, direction.z / length);
}

// 通常攻撃の命中判定。
// 攻撃者の正面に細長いカプセル判定を出し、対象の体判定と当たるか調べる。
inline bool IsNormalAttackHit(
    const HitBody& attackerBody,
    const VECTOR& facingDirection,
    const HitBody& targetBody,
    float range = kNormalAttackRange,
    float halfWidth = kNormalAttackHalfWidth
) {
    const VECTOR forward = NormalizeDirectionXZ(facingDirection);
    const VECTOR attackStart = VAdd(attackerBody.position, VGet(0.0f, attackerBody.height * 0.5f, 0.0f));
    const VECTOR attackEnd = VAdd(
        attackStart,
        VGet(forward.x * range, 0.0f, forward.z * range)
    );

    return HitCheck_Capsule_Capsule(
        attackStart,
        attackEnd,
        halfWidth,
        targetBody.position,
        GetHitBodyTop(targetBody),
        targetBody.radius
    ) == TRUE;
}

// 必殺技の命中判定。
// プレイヤーの周囲に円形範囲を出し、対象の体判定半径込みで当たるか調べる。
inline bool IsSpecialAttackHit(
    const HitBody& attackerBody,
    const HitBody& targetBody,
    float radius = kSpecialAttackRadius
) {
    return DistanceXZ(attackerBody.position, targetBody.position) <= radius + targetBody.radius;
}

// 攻撃種別に応じて、通常攻撃または必殺技の命中判定を行う。
inline bool IsAttackHit(
    const HitBody& attackerBody,
    const VECTOR& facingDirection,
    const HitBody& targetBody,
    AttackType attackType
) {
    if (attackType == AttackType::Special) {
        return IsSpecialAttackHit(attackerBody, targetBody);
    }
    return IsNormalAttackHit(attackerBody, facingDirection, targetBody);
}

// 攻撃が命中した後にHPを減らす。
// ここでは命中判定はせず、ダメージ計算と死亡判定だけを担当する。
inline AttackResult ApplyAttack(CharacterStatus& target, AttackType attackType) {
    AttackResult result;

    if (!target.alive) {
        return result;
    }

    result.hit = true;
    result.damage = GetAttackDamage(attackType);
    target.hp = std::max(0, target.hp - result.damage);
    result.remainingHp = target.hp;

    if (target.hp == 0) {
        target.alive = false;
        result.defeated = true;
    }

    return result;
}

// 命中判定とダメージ処理をまとめて行う。
// 攻撃アニメーションの「当たるフレーム」で呼ぶ想定。
inline AttackResult TryApplyAttack(
    const CharacterStatus& attacker,
    const VECTOR& attackerPosition,
    const VECTOR& facingDirection,
    CharacterStatus& target,
    const VECTOR& targetPosition,
    AttackType attackType
) {
    AttackResult result;

    const HitBody attackerBody = MakeHitBody(attacker, attackerPosition);
    const HitBody targetBody = MakeHitBody(target, targetPosition);
    if (!IsAttackHit(attackerBody, facingDirection, targetBody, attackType)) {
        return result;
    }

    return ApplyAttack(target, attackType);
}

// isHitDone を使って、1回の攻撃アニメーションでダメージを1回だけ通す。
// 通常攻撃を1体だけに当てたい場合や、連続ヒット防止に使う。
inline AttackResult TryApplyAttackOnce(
    AttackState& attackState,
    const CharacterStatus& attacker,
    const VECTOR& attackerPosition,
    const VECTOR& facingDirection,
    CharacterStatus& target,
    const VECTOR& targetPosition
) {
    AttackResult result;

    if (!CanApplyAttackHit(attackState)) {
        return result;
    }

    result = TryApplyAttack(
        attacker,
        attackerPosition,
        facingDirection,
        target,
        targetPosition,
        attackState.attackType
    );

    if (result.hit) {
        MarkHitDone(attackState);
    }

    return result;
}

// キャラクターが倒れているかを確認する。
inline bool IsCharacterDefeated(const CharacterStatus& status) {
    return !status.alive || status.hp <= 0;
}

// キャラクターを初期状態に戻す。
// リスポーンやテスト用のリセットで使う。
inline void ResetCharacter(CharacterStatus& status) {
    status.hp = status.maxHp;
    status.alive = true;
}

// DxLib の3Dモデルにキャラクター倍率を反映する。
// モデル読み込み後に一度呼べば、仮モデルでもサイズ確認ができる。
inline void ApplyModelScale(int modelHandle, const CharacterStatus& status) {
    MV1SetScale(modelHandle, VGet(status.scale, status.scale, status.scale));
}

} // namespace CharacterCombat


#include "DxLib.h"
#include <math.h>

static bool s_isScoreZoneActive = false;
static float s_zoneCurrentRadius = 0.0f;
static const float ZONE_MAX_RADIUS = 350.0f;

// ★ ステージ中央（ボス出現位置・コロシアムの中心）に完全に合わせる
static const VECTOR ZONE_CENTER = { 750.0f, 0.5f, -150.0f };

// 3D空間の地面に扇状ポリゴンで円盤を描く
static void DrawCircle3D_Solid(VECTOR center, float radius, unsigned int color, int alpha, int divNum = 48) {
    float step = 2.0f * DX_PI_F / (float)divNum;

    SetUseLighting(FALSE);
    SetUseZBuffer3D(FALSE);
    SetWriteZBuffer3D(FALSE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);

    // 中心点（地面から少し浮かせてチラつき防止）
    VECTOR cPos = VGet(center.x, center.y + 1.2f, center.z);

    for (int i = 0; i < divNum; ++i) {
        float a1 = step * (float)i;
        float a2 = step * (float)(i + 1);

        VECTOR p1 = VGet(center.x + radius * sinf(a1), center.y + 1.2f, center.z + radius * cosf(a1));
        VECTOR p2 = VGet(center.x + radius * sinf(a2), center.y + 1.2f, center.z + radius * cosf(a2));

        // 円盤の内側を塗りつぶし
        DrawTriangle3D(cPos, p1, p2, color, TRUE);
    }

    // 外枠のラインを強調（加算ブレンドで発光）
    SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
    for (int i = 0; i < divNum; ++i) {
        float a1 = step * (float)i;
        float a2 = step * (float)(i + 1);

        VECTOR p1 = VGet(center.x + radius * sinf(a1), center.y + 1.8f, center.z + radius * cosf(a1));
        VECTOR p2 = VGet(center.x + radius * sinf(a2), center.y + 1.8f, center.z + radius * cosf(a2));

        DrawLine3D(p1, p2, GetColor(255, 230, 80));
        DrawLine3D(VAdd(p1, VGet(0, 1.0f, 0)), VAdd(p2, VGet(0, 1.0f, 0)), GetColor(255, 60, 60));
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetWriteZBuffer3D(TRUE);
    SetUseZBuffer3D(TRUE);
    SetUseLighting(TRUE);
}

void InitStageScoreZone() {
    s_isScoreZoneActive = false;
    s_zoneCurrentRadius = 0.0f;
}

void UpdateStageScoreZone(int remainingTimeFrames) {
    // 75秒以下で陣地展開を開始
    if (remainingTimeFrames <= 75 * 60 && !s_isScoreZoneActive) {
        s_isScoreZoneActive = true;
        s_zoneCurrentRadius = 0.0f;
    }

    if (s_isScoreZoneActive) {
        if (s_zoneCurrentRadius < ZONE_MAX_RADIUS) {
            s_zoneCurrentRadius += 6.0f;
            if (s_zoneCurrentRadius > ZONE_MAX_RADIUS) {
                s_zoneCurrentRadius = ZONE_MAX_RADIUS;
            }
        }
    }
}

void DrawStageScoreZone() {
    if (s_isScoreZoneActive && s_zoneCurrentRadius > 0.0f) {
        // 半透明の赤い魔法陣風エリア（内側）＋外枠発光
        DrawCircle3D_Solid(ZONE_CENTER, s_zoneCurrentRadius, GetColor(255, 40, 40), 90);
    }
}

bool IsPlayerInScoreZone(VECTOR playerPos) {
    if (!s_isScoreZoneActive) {
        return true;
    }

    float dx = playerPos.x - ZONE_CENTER.x;
    float dz = playerPos.z - ZONE_CENTER.z;
    float distSq = dx * dx + dz * dz;

    return (distSq <= s_zoneCurrentRadius * s_zoneCurrentRadius);
}
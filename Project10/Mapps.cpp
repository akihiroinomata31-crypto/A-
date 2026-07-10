#include "DxLib.h"
#include <cmath>

class Map {
public:
    const float radius = 300.0f; // マップの半径

    void Draw() {
        // 1. 外周の青い円を描画
        for (int i = 0; i < 360; i += 5) {
            float angle1 = (float)i * DX_PI / 180.0f;
            float angle2 = (float)(i + 5) * DX_PI / 180.0f;

            DrawLine3D(
                VGet(radius * cos(angle1), 0.0f, radius * sin(angle1)),
                VGet(radius * cos(angle2), 0.0f, radius * sin(angle2)),
                GetColor(100, 100, 255) // 青
            );
        }

        // 2. 地面のグリッド線（戦場を網目状にする）
        for (float i = -radius; i <= radius; i += 50.0f) {
            // 縦の線
            DrawLine3D(VGet(i, 0.1f, -radius), VGet(i, 0.1f, radius), GetColor(50, 50, 80));
            // 横の線
            DrawLine3D(VGet(-radius, 0.1f, i), VGet(radius, 0.1f, i), GetColor(50, 50, 80));
        }
    }
};
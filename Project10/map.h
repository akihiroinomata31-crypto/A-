#pragma once
#include "DxLib.h"
#include <cmath> // Šp“xŒvZ‚É•K—v

class Map {
public:
    const float radius = 300.0f;       // ŠO‘¤‚Ì”¼Œa
    const float innerRadius = 150.0f;  // “à‘¤‚Ì”¼Œa

    // ‰~‚ğ•`‰æ‚·‚é•â•ŠÖ”
    void DrawCircle3D(float r, unsigned int color) {
        for (int i = 0; i < 360; i += 5) {
            float a1 = (float)i * DX_PI / 180.0f;
            float a2 = (float)(i + 5) * DX_PI / 180.0f;

            // y‚ğ0.1f‚É­‚µ•‚‚©‚¹‚é‚Æ’n–Ê‚Æd‚È‚Á‚Äƒ`ƒ‰‚Â‚­‚Ì‚ğ–h‚°‚Ü‚·
            DrawLine3D(
                VGet(r * cos(a1), 0.1f, r * sin(a1)),
                VGet(r * cos(a2), 0.1f, r * sin(a2)),
                color
            
            );
        }
    }

    void Draw() {
        // 1. ŠO‘¤‚ÌÂ‚¢‰~
        DrawCircle3D(radius, GetColor(100, 100, 255));

        // 2. “à‘¤‚ÌÔ‚¢‰~
        DrawCircle3D(innerRadius, GetColor(255, 50, 50));

        // 3. \š‚Ì•â•üi3D‹óŠÔã‚ÅŒ´“_‚ğ’Ê‚éüj
        DrawLine3D(VGet(-radius, 0.1f, 0.0f), VGet(radius, 0.1f, 0.0f), GetColor(50, 50, 80));
        DrawLine3D(VGet(0.0f, 0.1f, -radius), VGet(0.0f, 0.1f, radius), GetColor(50, 50, 80));
    }
};
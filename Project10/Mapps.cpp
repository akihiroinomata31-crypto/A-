#include "DxLib.h"
#include <cmath>

class Map {
public:
    int stagedata;
    int sky;

    float skyRot = 0.0f;
    const float radius = 300.0f;

    void Draw() {
        // 1. ƒ‚ƒfƒ‹‚Ì•`‰æ
        MV1DrawModel(stagedata);

        skyRot += 0.001f;
        MV1SetRotationXYZ(sky, VGet(0, skyRot, 0));
        MV1DrawModel(sky);

    }
};
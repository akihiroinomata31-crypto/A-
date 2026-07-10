#include "DxLib.h"
#include "main.h" // GameManagerの定義が含まれている前提
#include "map.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);

    if (DxLib_Init() == -1) return -1; // "もし" -> "if", "ならば" -> 削除
    SetDrawScreen(DX_SCREEN_BACK);

    GameManager game;// カメラを斜め上から見下ろす位置に設定
    SetCameraPositionAndTarget_UpVecY(VGet(0, 400, -500), VGet(0, 0, 0));
    Map stage;
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0) {
        ClearDrawScreen();
        SetupCamera_Perspective(500.0f);
        game.Update();
        game.DrawUI();
        stage.Draw();
        ScreenFlip();
    }

    DxLib_End();
    return 0; // "返す" -> "return"
}
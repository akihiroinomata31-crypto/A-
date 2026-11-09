#include "Sound.h"
#include "../DXLib/DxLib.h"
#include <stdio.h>
#include "main.h"

// このファイルの中だけで使う変数（main.cppを汚さない！）
namespace {
    int SEmenuDecideHandle = -1;
    int BGMSoundHandle = -1;
    int BGMInGameHandle = -1;
    bool isBGMInGameStarted = false;
}

// 初期化と読み込み
void InitSound() {
    // 決定音のロード
    SEmenuDecideHandle = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\click.wav"); // ファイル名は実際のパスに合わせる

    // BGMのロード
    char String[256];
    sprintf_s(String, "..\\Data\\Sound\\BGM\\BGM_stg1.wav");
    BGMSoundHandle = LoadSoundMem(String);
    if (BGMSoundHandle != -1) {
        PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP);
    }

    // ステージBGMのロード
    BGMInGameHandle = LoadSoundMem("..\\Data\\Sound\\BGM\\Stage_1.wav");
}

// タイトルBGMを流す（もし必要なら）
void LoadAndPlayTitleBGM() {
    char String[256];
    sprintf_s(String, "..\\Data\\Sound\\BGM\\BGM_title.wav");
    BGMSoundHandle = LoadSoundMem(String);
    if (BGMSoundHandle != -1) {
        PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP);
    }
}

// タイトルBGMを止める
void StopTitleBGM() {
    if (BGMSoundHandle != -1) {
        StopSoundMem(BGMSoundHandle);
    }
}

// ステージBGMを再生（重複再生防止つき）
void PlayInGameBGM() {
    if (!isBGMInGameStarted && BGMInGameHandle != -1) {
        PlaySoundMem(BGMInGameHandle, DX_PLAYTYPE_LOOP);
        isBGMInGameStarted = true;
    }
}
// 決定音を鳴らす
void PlayMenuDecideSE() {
    if (SEmenuDecideHandle >= 0) {
        PlaySoundMem(SEmenuDecideHandle, DX_PLAYTYPE_BACK);
    }
}
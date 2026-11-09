#include "Sound.h"
#include "DxLib.h"
#include "main.h"

// このファイルの中だけで使う変数
static int SEmenu = -1;
static int SEFlash = -1;
static int SEDram = -1;
static int SEStun = -1;
static int SEStart = -1;
static int SEristart = -1;
static int SEUP1 = -1;
static int SEUP2 = -1;

// BGM用ハンドル
static int titleBgmHandle = -1; // タイトル用BGM
static int gameBgmHandle = -1;  // ステージ・戦闘用BGM
static bool isBGMInGameStarted = false;

// 初期化と読み込み
void InitSound() {
    // 1. 効果音（SE）のロード
    SEmenu = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\click.wav");
    SEFlash = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\flash.wav");
    SEDram = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\dram.wav");
    SEStun = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\pkopko.wav");
    SEStart = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\start.wav");
    SEristart = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\suuuu.wav");
    SEUP1 = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\up1.wav");
    SEUP2 = LoadSoundMem("..\\Data\\Sound\\SE\\Game\\up2.mp3");

    // 2. タイトルBGMのロード（パスは実際の拡張子・フォルダ構成に合わせてね）
    titleBgmHandle = LoadSoundMem("..\\Data\\Sound\\BGM\\BGM_stg0.ogg");
    if (titleBgmHandle != -1) {
        PlaySoundMem(titleBgmHandle, DX_PLAYTYPE_LOOP);
    }

    // 3. ステージBGMのロード
    gameBgmHandle = LoadSoundMem("..\\Data\\Sound\\BGM\\Stage_1.wav");
}

// タイトルBGMを流す（メニューやリザルトから戻ってきたとき用）
void TiTle() {
    if (titleBgmHandle != -1 && CheckSoundMem(titleBgmHandle) == 0) {
        PlaySoundMem(titleBgmHandle, DX_PLAYTYPE_LOOP);
    }
}

// タイトルBGMを止める（ゲーム開始のロード画面に入る瞬間）
void StopTitleBGM() {
    if (titleBgmHandle != -1) {
        StopSoundMem(titleBgmHandle);
    }
    if (gameBgmHandle != -1) {
        StopSoundMem(gameBgmHandle);
    }
    isBGMInGameStarted = false;
}

// ステージBGMを再生（重複再生防止つき）
void PlayInGameBGM() {
    if (!isBGMInGameStarted && gameBgmHandle != -1) {
        PlaySoundMem(gameBgmHandle, DX_PLAYTYPE_LOOP);
        isBGMInGameStarted = true;
    }
}

// 各種SE関数
void PlayFlash() { if (SEFlash >= 0) PlaySoundMem(SEFlash, DX_PLAYTYPE_BACK); }
void PlayMenuDecideSE() { if (SEmenu >= 0) PlaySoundMem(SEmenu, DX_PLAYTYPE_BACK); }
void Dram() { if (SEDram >= 0) PlaySoundMem(SEDram, DX_PLAYTYPE_BACK); }
void Stun() { if (SEStun >= 0) PlaySoundMem(SEStun, DX_PLAYTYPE_BACK); }
void Start() { if (SEStart >= 0) PlaySoundMem(SEStart, DX_PLAYTYPE_BACK); }
void Ristart() { if (SEristart >= 0) PlaySoundMem(SEristart, DX_PLAYTYPE_BACK); }
void UP1() { if (SEUP1 >= 0) PlaySoundMem(SEUP1, DX_PLAYTYPE_BACK); }
void UP2() { if (SEUP2 >= 0) PlaySoundMem(SEUP2, DX_PLAYTYPE_BACK); }
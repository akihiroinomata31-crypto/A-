#pragma once

// サウンドの初期化・読み込み
void InitSound();

// 決定音を鳴らす関数
void PlayMenuDecideSE();

// BGM関連
void LoadAndPlayBGM();
void PlayInGameBGM();
void LoadAndPlayTitleBGM(); // ← これがあるか
void StopTitleBGM();
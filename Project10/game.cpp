#include "game.h"
#include "main.h"
#include "player.h"
#include "HudText.h"
#include <math.h>
#include <string.h>
#include "../DXLib/DxLib.h"
#include "golem.h"
#include "Sound.h"

int stagedata = -1;
int enemy_anim_attack = -1;
int redGoblinBaseModel = -1;
int red_goblin_anim_neutral = -1;
int red_goblin_anim_attack = -1;
int red_goblin_anim_walk = -1;
int weaponBaseModel = -1;

bool PlaceEnemyOnGround(SCharaInfo& enemy) {
    if (stagedata < 0) return false;
    const float dx = enemy.pos.x - enemy.groundCachePosition.x;
    const float dz = enemy.pos.z - enemy.groundCachePosition.z;
    if (enemy.groundCacheValid && dx * dx + dz * dz < 64.0f && fabsf(enemy.pos.y - enemy.groundCachePosition.y) < 0.1f) {
        enemy.pos.y = enemy.groundCachePosition.y;
        enemy.move.y = 0;
        enemy.charahitinfo.CenterPosition = enemy.pos;
        if (enemy.model1 >= 0) MV1SetPosition(enemy.model1, enemy.pos);
        return true;
    }
    const auto hit = MV1CollCheck_Line(stagedata, -1,
        VGet(enemy.pos.x, 2000.0f, enemy.pos.z), VGet(enemy.pos.x, -1000.0f, enemy.pos.z));
    if (!hit.HitFlag) { enemy.groundCacheValid = false; return false; }
    enemy.pos.y = hit.HitPosition.y;
    enemy.groundCachePosition = enemy.pos;
    enemy.groundCacheValid = true;
    enemy.move.y = 0.0f;
    enemy.charahitinfo.CenterPosition = enemy.pos;
    if (enemy.model1 >= 0) MV1SetPosition(enemy.model1, enemy.pos);
    return true;
}

void GameManager::Init() {
    mainTimer = 99 * 60; // 60 Hz simulation
    startCountdownTimer = 0;
    bossSpawned = false;
    bossCutsceneTimer = 0;
    bossCutsceneDone = false;
    goblinSpawnTimer = 0;
    redSpawnTimer = 0;
    p1Score = 0;
    p2Score = 0;
    gameState = 0;
    deathCount[0] = 0;
    deathCount[1] = 0;
    killCount[0] = 0;
    killCount[1] = 0;
    specialUseCount[0] = 0;
    specialUseCount[1] = 0;

    for (int i = 0; i < 20; i++) {
        popups[i].active = false;
    }
}

void GameManager::Update(SCharaInfo* enemyList, SCharaInfo* players) {
    // ==========================================
    // 1. タイマー更新 ＆ 勝敗判定（開幕カウントダウン中は減算停止）
    // ==========================================
    if (startCountdownTimer <= 0) {
        if (mainTimer > 0) {
            mainTimer--;
        }
    }

    if (mainTimer <= 0 && gameState == 0) {
        // ① スコア比較
        if (FinalScore(0) > FinalScore(1)) {
            gameState = 1;
        }
        else if (FinalScore(1) > FinalScore(0)) {
            gameState = 2;
        }
        else {
            // ② 死亡数比較（少ない方が勝ち）
            if (deathCount[0] < deathCount[1]) {
                gameState = 1;
            }
            else if (deathCount[1] < deathCount[0]) {
                gameState = 2;
            }
            else {
                // ③ 撃破数比較（多い方が勝ち）
                if (killCount[0] > killCount[1]) {
                    gameState = 1;
                }
                else if (killCount[1] > killCount[0]) {
                    gameState = 2;
                }
               
            }
        }
    }
    // ==========================================
    // 2. 敵スポーン処理（同時生存上限100体）
    // ==========================================
    int activeEnemyCount = 0;
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode != NONE && enemyList[i].enemyHP > 0) {
            activeEnemyCount++;
        }
    }

    const int MAX_FIELD_ENEMIES = 100;

    if (activeEnemyCount < MAX_FIELD_ENEMIES) {
        // --- 通常ゴブリン ---
        goblinSpawnTimer++;
        if (goblinSpawnTimer >= 120) {
            goblinSpawnTimer = 0;

            for (int k = 0; k < 15; k++) {
                float spawnX = (float)(GetRand(4000) - 2000);
                float spawnZ = (float)(GetRand(4000) - 2000);
                ActivateEnemy(enemyList, spawnX, spawnZ);
            }
        }

        // --- 赤ゴブリン ---
        redSpawnTimer++;
        if (redSpawnTimer >= 200) {
            redSpawnTimer = 0;
            for (int j = 0; j < 2; j++) {
                float spawnX = (float)(GetRand(3000) - 1500);
                float spawnZ = (float)(GetRand(3000) - 1500);
                ActivateRedGoblin(enemyList, spawnX, spawnZ);
            }
        }
    }

    // ==========================================
   // 3. 残り75秒以下でゴーレム（ボス）を1体出現
   // ==========================================
    int seconds = mainTimer / 60;
    if (seconds <= 70 && gameState == 0) {
        if (!bossSpawned && enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
            bossSpawned = true;

            if (enemyList[TEST_ENEMY_GOLEM].model1 == -1) {
                enemyList[TEST_ENEMY_GOLEM].model1 =
                    MV1LoadModel("..\\Data\\Golem\\Boss.mv1");

                // 阿井追加
                if (enemyList[TEST_ENEMY_GOLEM].model1 != -1) {
                    InitGolemAnimations(enemyList[TEST_ENEMY_GOLEM]);
                }
            }
            enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 30.0f, -150.0f);
            enemyList[TEST_ENEMY_GOLEM].mode = STAND;
            enemyList[TEST_ENEMY_GOLEM].enemyHP = 20;
            enemyList[TEST_ENEMY_GOLEM].playtime = 0.0f;
            enemyList[TEST_ENEMY_GOLEM].isHit = false;

            // 阿井追加
            // ゴーレムの大きさ
            MV1SetScale(enemyList[TEST_ENEMY_GOLEM].model1, VGet(2.5f, 2.5f, 2.5f));

            if (enemyList[TEST_ENEMY_GOLEM].model1 != -1) {
                MV1SetPosition(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].pos);
                MV1SetVisible(enemyList[TEST_ENEMY_GOLEM].model1, TRUE);

                if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                    MV1DetachAnim(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
                }
                enemyList[TEST_ENEMY_GOLEM].attachidx = MV1AttachAnim(enemyList[TEST_ENEMY_GOLEM].model1, 0, enemyList[TEST_ENEMY_GOLEM].enemyIdleAnimation);
                if (enemyList[TEST_ENEMY_GOLEM].attachidx != -1) {
                    enemyList[TEST_ENEMY_GOLEM].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[TEST_ENEMY_GOLEM].model1, enemyList[TEST_ENEMY_GOLEM].attachidx);
                }
            }
            PlaceEnemyOnGround(enemyList[TEST_ENEMY_GOLEM]);

            if (!bossCutsceneDone) {
                bossCutsceneTimer = BOSS_CUTSCENE_TOTAL_FRAMES;
                bossCutsceneDone = true;
            }

        }
        if (bossCutsceneTimer > 0) {
            bossCutsceneTimer--;
        }
    }
}

void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
        if (enemyList[i].mode == NONE) {
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }
            if (enemyList[i].model1 == -1) continue;

            enemyList[i].pos = VGet(x, 0.0f, z);

            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);
            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            enemyList[i].pos.y = baseFloorY;
            enemyList[i].groundCacheValid = false;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 4;
            enemyList[i].playtime = (float)GetRand(30) * 0.1f;
            enemyList[i].isHit = false;
            enemyList[i].currentAnimType = 1;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
                enemyList[i].attachidx = -1;
            }

            int useNeutralAnim = (red_goblin_anim_neutral != -1) ? red_goblin_anim_neutral : enemy_anim_neutral;
            if (useNeutralAnim != -1) {
                enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, useNeutralAnim);
                if (enemyList[i].attachidx != -1) {
                    enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
                    MV1SetAttachAnimTime(enemyList[i].model1, enemyList[i].attachidx, enemyList[i].playtime);
                }
            }
            break;
        }
    }
}

void GameManager::ActivateEnemy(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
        if (i == TEST_ENEMY_GOLEM) continue;
        if (enemyList[i].mode == NONE) {
            if (enemyList[i].model1 < 0 && enemyBaseModel >= 0) enemyList[i].model1 = MV1DuplicateModel(enemyBaseModel);
            if (enemyList[i].model1 < 0) continue;
            enemyList[i].currentAnimType = 1;
            enemyList[i].enemyWalking = false;
            enemyList[i].invincibleTimer = 0;
            enemyList[i].pos = VGet(x, 0.0f, z);

            VECTOR cal_pos1 = VGet(enemyList[i].pos.x, 2000.0f, enemyList[i].pos.z);
            VECTOR cal_pos2 = VGet(enemyList[i].pos.x, -1000.0f, enemyList[i].pos.z);
            MV1_COLL_RESULT_POLY LineRes = MV1CollCheck_Line(stagedata, -1, cal_pos1, cal_pos2);

            float baseFloorY = 0.0f;
            if (LineRes.HitFlag == 1) {
                baseFloorY = LineRes.HitPosition.y;
            }

            enemyList[i].pos.y = baseFloorY;
            enemyList[i].groundCacheValid = false;
            enemyList[i].mode = STAND;
            enemyList[i].enemyHP = 2;
            enemyList[i].playtime = 0.0f;
            enemyList[i].isHit = false;

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
            }
            enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, enemy_anim_neutral);
            if (enemyList[i].attachidx != -1) {
                enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
            }
            break;
        }
    }
}

void GameManager::RecordFrame(VECTOR p1, VECTOR p2, int act) {
    ReplayFrame frame;
    frame.pos[0] = p1;
    frame.pos[1] = p2;
    frame.action = act;
    replayData.push_back(frame);
}

void GameManager::DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex,  bool isPvPActive, int pvpCooldown) {
    HudText hud;
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int halfW = sw / 2;
    hud.SetSize(20);

    // 決着テキスト
    if (gameState != 0) {
        hud.SetSize(60);
        int color = GetColor(255, 255, 0);
        if (gameState == 1)      hud.Draw(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        else if  (gameState == 2) hud.Draw(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
       // else                     hud.Draw(xOffset + 100, sh / 2 - 30, "DRAW", color);
        hud.SetSize(20);
    }

    
    int startY = 20;
   

    int frameDrawW = 300;
    int frameDrawH = 24;
    int drawX = xOffset + 20;
    int drawY = startY + 30;

    float hpRate = (float)players[pIdx].HP / (float)MAX_HP;
    if (hpRate < 0.0f) hpRate = 0.0f;
    if (hpRate > 1.0f) hpRate = 1.0f;

    int innerMargin = 2;
    DrawExtendGraph(drawX, drawY, drawX + frameDrawW, drawY + frameDrawH, hpBarTex, TRUE);

    DrawBox(drawX + innerMargin, drawY + innerMargin,
        drawX + frameDrawW - innerMargin, drawY + frameDrawH - innerMargin,
        GetColor(200, 0, 0), TRUE);

    int innerMaxWidth = frameDrawW - (innerMargin * 2);
    int currentGreenWidth = (int)(innerMaxWidth * hpRate);

    if (currentGreenWidth > 0) {
        if (hpRate <= 0.3f) SetDrawBright(255, 100, 100);
        else                SetDrawBright(100, 255, 100);

        DrawBox(drawX + innerMargin, drawY + innerMargin,
            drawX + innerMargin + currentGreenWidth, drawY + frameDrawH - innerMargin,
            GetColor(0, 255, 0), TRUE);

        SetDrawBright(255, 255, 255);
    }

    // 残り15秒以下のフィーバー表示
    int seconds = mainTimer / 60;
    if (gameState == 0 && seconds <= 15 && seconds > 0) {
        int feverColor = ((mainTimer / 15) % 2 == 0) ? GetColor(255, 50, 50) : GetColor(255, 220, 0);
        hud.SetSize(16);
        hud.Draw(xOffset + frameDrawW + 30, startY + 4, "FEVER! x2", feverColor);
        hud.SetSize(20);
    }

   
    



  
    // ========================================================
    // スコア表示
    // ========================================================
    int currentScore = (pIdx == 0) ? p1Score : p2Score;
    char scoreStr[32];

    // ① 残り30秒以下の通常時、または
    // ② 試合終了後であっても、リザルト画面のロールタイマーが 120（2秒）未満の間は "???" にする
    if ((gameState == 0 && seconds <= 30 && seconds > 0) ||
        (gameState != 0 && GetResultRollTimer(false) < 120)) {
        sprintf_s(scoreStr, sizeof(scoreStr), "???");
    }
    else {
        // それ以降（リザルトの演出が進んでから）は実際のスコアを表示
        sprintf_s(scoreStr, sizeof(scoreStr), "%d", currentScore);
    }

    int scoreTextW = 0;
    if (fontScore != -1) {
        scoreTextW = GetDrawStringWidthToHandle(scoreStr, (int)strlen(scoreStr), fontScore);
    }
    else {
        scoreTextW = hud.Width(scoreStr, (int)strlen(scoreStr));
    }

    // ★ プレイヤー1・プレイヤー2ともに各分割画面の右下に配置
    int scoreX = xOffset + halfW - scoreTextW - 30;
    int scoreY = sh - 55;

    if (fontScore != -1) {
        DrawStringToHandle(scoreX, scoreY, scoreStr, GetColor(0, 0, 0), fontScore, GetColor(255, 255, 255));
    }
    else {
        hud.SetSize(24);
        hud.Draw(scoreX, scoreY, scoreStr, GetColor(0, 0, 0));
        hud.SetSize(20);
    }



    // スコア加算ポップアップ
    for (int i = 0; i < 20; i++) {
        if (!popups[i].active || popups[i].playerIdx != pIdx) continue;

        int popupBaseX = scoreX + 30;
        int popupBaseY = scoreY - 10 - (int)popups[i].offsetY;

        // フェードアウト計算
        int alpha = 255;
        if (popups[i].timer < 20) {
            alpha = (popups[i].timer * 255) / 20;
        }
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        hud.SetSize(22);

        // 文字の選択と描画
        if (gameState == 0 && seconds <= 30 && seconds > 0) {
            hud.Format(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+?");
        }
        else {
            unsigned int col = (popups[i].score > 0) ? GetColor(255, 60, 60) : GetColor(255, 0, 0);
            const char* fmt = (popups[i].score > 0) ? "+%d" : "%d";
            hud.Format(popupBaseX, popupBaseY, col, fmt, popups[i].score);
        }

        hud.SetSize(20);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

void GameManager::DrawTimer(int sw, int sh) {
    HudText hud;
    int seconds = mainTimer / 60;
    if (seconds > 200) seconds = 200;
    if (seconds < 0)   seconds = 0;

    unsigned int timerColor;
    if (seconds <= 10 && (mainTimer / 15) % 2 == 0) {
        timerColor = GetColor(255, 50, 50);
    }
    else if (seconds <= 30) {
        timerColor = GetColor(255, 80, 80);
    }
    else {
        timerColor = GetColor(255, 255, 0);
    }

    hud.SetSize(30);

    char timerStr[32];
    if (seconds > 0) {
        sprintf_s(timerStr, "%02d", seconds);
    }
    else {
        sprintf_s(timerStr, "FINISH!");
    }

    int textWidth = hud.Width(timerStr, (int)strlen(timerStr));
    int drawX = sw / 2 - textWidth / 2;
    int drawY = 15;

    hud.Draw(drawX, drawY, timerStr, timerColor);
    hud.SetSize(20);
}

void GameManager::AddScore(int playerIndex, int score) {
    if (playerIndex == 0) {
        p1Score += score;
        if (p1Score < 0) p1Score = 0;
    }
    else {
        p2Score += score;
        if (p2Score < 0) p2Score = 0;
    }
}

void GameManager::AddScorePopup(int playerIdx, int score) {
    for (int i = 0; i < 20; i++) {
        if (!popups[i].active) {
            popups[i].score = score;
            popups[i].playerIdx = playerIdx;
            popups[i].offsetY = 0.0f;
            popups[i].timer = 60;
            popups[i].active = true;
            break;
        }
    }
}

static int MenuFont(int size) {
    static int fontSmall = -1, label = -1, heading = -1, title = -1;
    int* handle = size <= 17 ? &fontSmall : size <= 25 ? &label : size <= 40 ? &heading : &title;
    if (*handle < 0) *handle = CreateFontToHandle("Yu Gothic", size <= 17 ? 16 : size <= 25 ? 24 : size <= 40 ? 36 : 54, 2, DX_FONTTYPE_ANTIALIASING_8X8);
    return *handle;
}

static void MenuLabel(int x, int y, int size, const char* text, unsigned int color) {
    if (text == nullptr) {
        return;
    }

    int fontHandle = MenuFont(size);

    // フォントハンドルが無効(<=0)の場合は標準フォントで代替描画して即死を防ぐ
    if (fontHandle <= 0) {
        DrawString(x, y, text, color);
        return;
    }

    DrawStringToHandle(x, y, text, color, fontHandle);
}

static void MenuText(int y, int size, const char* text, unsigned int color) {
    const int font = MenuFont(size);
    DrawStringToHandle((900 - GetDrawStringWidthToHandle(text, (int)strlen(text), font)) / 2, y, text, color, font);
}



int& GetResultRollTimer(bool reset) {
    static int s_rollTimer = 0;
    if (reset) {
        s_rollTimer = 0; // true が送られてきたら強制リセット！
    }
    return s_rollTimer;
}
// ========================================================
// タイトル / リザルト画面描画
// ========================================================
void GameManager::DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot, int rollTimer) {
    SetDrawArea(0, 0, 900, 600);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    ClearDrawScreen();

    const auto gold = GetColor(222, 194, 137), white = GetColor(235, 236, 229), muted = GetColor(127, 145, 153);

    // ----------------------------------------------------
    // 背景画像（taitol.jpg）
    // ----------------------------------------------------
    static int titleBgHandle = -1;
    if (titleBgHandle == -1) {
        titleBgHandle = LoadGraph("..\\Data\\UI\\taitol.png");
        if (titleBgHandle == -1) {
            titleBgHandle = LoadGraph("..\\Data\\UI\\taitol.png");
        }
    }

    if (titleBgHandle != -1) {
        DrawExtendGraph(0, 0, 900, 600, titleBgHandle, FALSE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 70);
        DrawBox(0, 0, 900, 600, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    else {
        for (int y = 0; y < 600; ++y) {
            const float t = y / 600.0f;
            DrawLine(0, y, 900, y, GetColor(8 + (int)(6 * t), 17 + (int)(4 * t), 25 + (int)(5 * t)));
        }
    }

   

    // ========================================================
    // 1. タイトル画面（「スタート」のみ・点滅表示）
    // ========================================================
    if (!results) {
        static int titleLogoHandle = -1;
        if (titleLogoHandle == -1) {
            SetTransColor(0, 0, 0);
            titleLogoHandle = LoadGraph("..\\Data\\UI\\genkai.png");
           
        }

        const int logoW = 680;
        const int logoH = 340;
        const int logoX = (900 - logoW) / 2;
        const int logoY = 40;

        // メラメラ燃える炎オーラ
        const int now = GetNowCount();
      

       

        // 火の粉スパーク
        SetDrawBlendMode(DX_BLENDMODE_ADD, 210);
        const int SPARK_COUNT = 36;
      
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // メインロゴ本体
        if (titleLogoHandle != -1) {
            DrawExtendGraph(logoX, logoY, logoX + logoW, logoY + logoH, titleLogoHandle, TRUE);
        }
        else {
            MenuLabel(340, 150, 64, "限界ラッシュ", gold);
        }

        // ★ スタート表示（滑らかなサイン波点滅）
        float blinkRate = (sinf(now * 0.006f) + 1.0f) * 0.5f;
        int textAlpha = 70 + (int)(185.0f * blinkRate);

        const char* promptText = "- PRESS START / ENTER -";
        SetFontSize(30);
        int promptWidth = GetDrawStringWidth(promptText, (int)strlen(promptText));
        int promptX = (900 - promptWidth) / 2;
        int promptY = 440;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(140 * blinkRate));
        DrawBox(promptX - 25, promptY - 6, promptX + promptWidth + 25, promptY + 40, GetColor(0, 0, 0), TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, textAlpha);
        DrawString(promptX + 2, promptY + 2, promptText, GetColor(0, 0, 0));
        DrawString(promptX, promptY, promptText, gold);

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        SetFontSize(20);

        DrawLine(110, 548, 790, 548, GetColor(50, 45, 40));
        MenuText(566, 16, "A / Enter 決定", muted);
    }
    // ========================================================
     // 2. リザルト画面（項目を左固定・数値のみを各列に配置）
     // ========================================================
    else {
        int& s_rollTimer = GetResultRollTimer(false); // 普段は false で値を取得・インクリメント
        s_rollTimer++;

        // 背景暗幕
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 140);
        DrawBox(0, 0, 900, 600, GetColor(0, 0, 0), TRUE);

        // 中央メインパネル（幅740、高さ390）
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
        DrawBox(80, 50, 820, 440, GetColor(10, 15, 20), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 外枠線（ゴールド＆ブロンズ）
        DrawBox(80, 50, 820, 440, gold, FALSE);
        DrawBox(82, 52, 818, 438, GetColor(80, 70, 50), FALSE);

        // タイトル
        MenuText(65, 32, "試合結果", gold);

        // ★ 各項目の出現タイミング（60fps基準：約2秒 = 120フレーム刻み）
        const int STEP_EARNED = 120;  // 2.0秒後: 獲得スコア
        const int STEP_DEATH = 240;  // 4.0秒後: 死亡ペナルティ
        const int STEP_TOTAL = 360;  // 6.0秒後: 最終合計スコア
        const int STEP_WINNER = 480;  // 8.0秒後: 勝者判定
        const int STEP_MENU = 600;  // 10.0秒後: 全結果確定後にメニュー出現

        // 区切り線
        DrawLine(110, 110, 790, 110, GetColor(53, 71, 78));

        // ★ 列ヘッダー配置（左端: 項目 / 中央: プレイヤー1 / 右側: プレイヤー2）
        const int COL_LABEL = 130; // 左側の項目名
        const int COL_P1 = 380; // プレイヤー1の数値列
        const int COL_P2 = 600; // プレイヤー2の数値列

        MenuLabel(COL_P1, 125, 22, "プレイヤー１", GetColor(131, 187, 207));
        MenuLabel(COL_P2, 125, 22, "プレイヤー２", GetColor(219, 165, 140));

        char text[64];

        // ----------------------------------------------------
        // ① 獲得スコア（2秒後）
        // ----------------------------------------------------
        if (s_rollTimer >= STEP_EARNED) {
            if (s_rollTimer == STEP_EARNED) {
                Dram();
            }
            // 左端に項目名
            MenuLabel(COL_LABEL, 175, 20, "獲得スコア", white);

            // プレイヤー1
            sprintf_s(text, "%6d pt", game.p1Score);
            MenuLabel(COL_P1 + 15, 175, 20, text, white);

            // プレイヤー2
            sprintf_s(text, "%6d pt", game.p2Score);
            MenuLabel(COL_P2 + 15, 175, 20, text, white);
        }

        // ----------------------------------------------------
        // ② 死亡ペナルティ（4秒後）
        // ----------------------------------------------------
        if (s_rollTimer >= STEP_DEATH) {
            // 左端に項目名
            MenuLabel(COL_LABEL, 218, 20, "死亡ペナルティ", white);

            for (int p = 0; p < 2; ++p) {
                int posX = (p == 0) ? (COL_P1 + 15) : (COL_P2 + 15);
                int deaths = game.deathCount[p];
                int deathCost = deaths * GameManager::DeathPenalty;

                unsigned int deathColor = (deaths > 0) ? GetColor(255, 110, 110) : muted;
                sprintf_s(text, "-%d (%d回)", deathCost, deaths);
                MenuLabel(posX, 218, 18, text, deathColor);
            }
        }

        // ----------------------------------------------------
        // ③ 最終合計スコア（6秒後）
        // ----------------------------------------------------
        if (s_rollTimer >= STEP_TOTAL) {
            // 左端に項目名
            MenuLabel(COL_LABEL, 268, 22, "合計スコア", gold);

            for (int p = 0; p < 2; ++p) {
                int posX = (p == 0) ? (COL_P1 + 15) : (COL_P2 + 15);
                int score = game.FinalScore(p);
                unsigned int totalColor = (score > 0) ? gold : white;

                sprintf_s(text, "%6d pt", score);
                MenuLabel(posX, 268, 22, text, totalColor);
            }
        }

        // ----------------------------------------------------
         // ④ 勝敗判定（8秒後）- 引き分けなし・特大フォント版
         // ----------------------------------------------------
        if (s_rollTimer >= STEP_WINNER) {
            const char* winText = "";
            unsigned int winColor = gold;

            if (game.gameState == 1) {
                winText = "プレイヤー１の勝利！";
                winColor = GetColor(131, 187, 207); // 青系
            }
            else {
                winText = "プレイヤー２の勝利！";
                winColor = GetColor(219, 165, 140); // 赤系
            }

            // 特大サイズで描画
            SetFontSize(44);
            int tw = GetDrawStringWidth(winText, (int)strlen(winText));
            int posX = 450 - tw / 2;
            int posY = 325;

            // 黒縁（影）
            DrawString(posX + 2, posY + 2, winText, GetColor(0, 0, 0));
            DrawString(posX - 1, posY - 1, winText, GetColor(0, 0, 0));

            // 本文
            DrawString(posX, posY, winText, winColor);

            SetFontSize(20);
        }

        // ----------------------------------------------------
        // ⑤ メニュー操作（10秒後・全結果確定後にのみ出現）
        // ----------------------------------------------------
        if (s_rollTimer >= STEP_MENU) {
            const int left = 290;
            const int width = 320;

            for (int row = 0; row < 2; ++row) {
                const int y = 465 + row * 48;

                if (row == selected) {
                    for (int x = 0; x < width; ++x) {
                        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 - x * 75 / width);
                        DrawLine(left + x, y, left + x, y + 36, GetColor(81, 99, 107));
                    }
                    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                    DrawBox(left, y, left + 3, y + 36, gold, TRUE);
                    DrawLine(left, y + 36, left + width, y + 36, GetColor(77, 90, 92));
                }

                const char* itemLabel = (row == 0) ? "再戦する" : "ゲーム終了";
                unsigned int itemColor = (row == selected) ? gold : muted;

                MenuLabel(left + 25, y + 6, 20, itemLabel, itemColor);

                if (row == selected) {
                    MenuLabel(left + width - 32, y + 6, 20, ">", gold);
                }
            }

        }
    }

    if (snapshot) SaveDrawScreenToPNG(0, 0, 900, 600, snapshot);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetFontSize(20);
    ScreenFlip();
}

void GameManager::DrawSpecialGauge(int pIdx, int startX, int endX, int screenHeight, int currentGauge, int maxGauge, int frameGraphHandle) {
    if (maxGauge <= 0) return;
    HudText hud;

    int drawX = startX + 20;
    int drawY = 80;

    const int barW = 260;
    const int barH = 34;

    int innerX1 = drawX + (int)(barW * 0.125f);
    int innerX2 = drawX + barW - (int)(barW * 0.125f);
    int innerY1 = drawY + (int)(barH * 0.24f);
    int innerY2 = drawY + barH - (int)(barH * 0.24f);

    int innerMaxW = innerX2 - innerX1;
    int innerH = innerY2 - innerY1;

    float rate = (float)currentGauge / (float)maxGauge;
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    int currentFillW = (int)(innerMaxW * rate);

    DrawBox(innerX1, innerY1, innerX2, innerY2, GetColor(15, 20, 28), TRUE);

    if (currentFillW > 0) {
        int fillX2 = innerX1 + currentFillW;

        DrawBox(innerX1, innerY1, fillX2, innerY2, GetColor(0, 110, 230), TRUE);

        int midY = innerY1 + innerH / 2;
        DrawBox(innerX1, innerY1, fillX2, midY, GetColor(40, 180, 255), TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawBox(innerX1, innerY1, fillX2, innerY1 + 2, GetColor(200, 240, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        if (currentFillW < innerMaxW) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
            DrawLine(fillX2 - 1, innerY1, fillX2 - 1, innerY2, GetColor(220, 245, 255));
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    if (frameGraphHandle != -1) {
        DrawExtendGraph(drawX, drawY, drawX + barW, drawY + barH, frameGraphHandle, TRUE);
    }

    if (currentGauge >= maxGauge) {
        int pulseAlpha = 80 + (int)(sinf(GetNowCount() * 0.008f) * 40);
        SetDrawBlendMode(DX_BLENDMODE_ADD, pulseAlpha);
        DrawBox(innerX1, innerY1, innerX2, innerY2, GetColor(50, 150, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        int textAlpha = 180 + (int)(sinf(GetNowCount() * 0.008f) * 75);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, textAlpha);
        SetFontSize(15);
        DrawString(drawX + barW + 8, drawY + 9, "READY!", GetColor(100, 220, 255));
        SetFontSize(20);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

bool GameManager::ApplyBossCamera(VECTOR& outCamPos, VECTOR& outCamTarget, const SCharaInfo* enemyList) {
    if (bossCutsceneTimer <= 0) return false;
    if (enemyList[TEST_ENEMY_GOLEM].mode == NONE || enemyList[TEST_ENEMY_GOLEM].model1 == -1) return false;

    VECTOR bossPos = enemyList[TEST_ENEMY_GOLEM].pos;
    outCamTarget = VAdd(bossPos, VGet(0.0f, 130.0f, 0.0f));

    float progress = (float)bossCutsceneTimer / (float)BOSS_CUTSCENE_TOTAL_FRAMES;
    float zoomDist = 240.0f + (progress * 180.0f);
    float camHeight = 110.0f + (progress * 40.0f);

    outCamPos = VGet(bossPos.x, bossPos.y + camHeight, bossPos.z + zoomDist);
    return true;
}

void GameManager::DrawBossCutsceneUI(int startX, int endX, int screenHeight) const {
    if (bossCutsceneTimer <= 0) return;

    int screenCenterX = (startX + endX) / 2;
    int screenCenterY = screenHeight / 2;

    int bandAlpha = 190;
    if (bossCutsceneTimer < 30) {
        bandAlpha = (bossCutsceneTimer * 190) / 30;
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, bandAlpha);
    DrawBox(startX, screenCenterY - 45, endX, screenCenterY + 45, GetColor(0, 0, 0), TRUE);
    DrawLine(startX, screenCenterY - 45, endX, screenCenterY - 45, GetColor(230, 30, 30), 2);
    DrawLine(startX, screenCenterY + 45, endX, screenCenterY + 45, GetColor(230, 30, 30), 2);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    SetFontSize(36);
    const char* bossText = "BOSS戦開始";
    int textWidth = GetDrawStringWidth(bossText, (int)strlen(bossText));
    int textX = screenCenterX - (textWidth / 2);
    int textY = screenCenterY - 18 - 8;

    DrawString(textX - 2, textY, bossText, GetColor(0, 0, 0));
    DrawString(textX + 2, textY, bossText, GetColor(0, 0, 0));
    DrawString(textX, textY - 2, bossText, GetColor(0, 0, 0));
    DrawString(textX, textY + 2, bossText, GetColor(0, 0, 0));
    DrawString(textX, textY, bossText, GetColor(255, 60, 40));

    SetFontSize(14);
    const char* subText = "- WARNING: GOLEM APPEARED -";
    int subWidth = GetDrawStringWidth(subText, (int)strlen(subText));
    DrawString(screenCenterX - (subWidth / 2), screenCenterY + 18, subText, GetColor(240, 200, 50));

    SetFontSize(20);
}

void GameManager::DrawLoadingScreen() {
    SetDrawArea(0, 0, 900, 600);
    ClearDrawScreen();

    // 背景（闘技場らしいダークトーン）
    DrawBox(0, 0, 900, 600, GetColor(15, 18, 22), TRUE);

    // ==========================================
    // ゲームルール・説明表示
    // ==========================================
    SetFontSize(30);
    DrawString(310, 50, "【 遊び方・ルール 】", GetColor(255, 215, 0));

    SetFontSize(20);
    const int textY = 120;
    const int lineGap = 34;

    // ルール本文
    DrawString(120, textY + lineGap * 0, "◆ 制限時間は 99秒！ 相手より多くのスコアを稼げ！", GetColor(240, 240, 240));
    DrawString(120, textY + lineGap * 1, "◆ ゴブリンを撃破してポイント獲得", GetColor(200, 200, 200));
    DrawString(150, textY + lineGap * 2, "・緑ゴブリン: 100 pt", GetColor(120, 220, 120));
    DrawString(150, textY + lineGap * 3, "・赤ゴブリン: 300 pt", GetColor(240, 100, 100));
    DrawString(150, textY + lineGap * 4, "・ボスゴーレム: 1000 pt（残り時間で出現！）", GetColor(255, 180, 80));

   

    // 戦略・対戦ヒント
    DrawString(120, textY + lineGap * 6, "◆ 閃光弾で相手の視界を奪え！", GetColor(255, 230, 100));
    DrawString(150, textY + lineGap * 7, "相手の正面で爆発させると完全スタン＆視界ゼロ！", GetColor(180, 180, 180));
    DrawString(120, textY + lineGap * 8, "◆ 残り30秒でスコアは（???）になる！", GetColor(240, 140, 140));

    // ==========================================
    // 下部ローディングアニメーション
    // ==========================================
    const int posX = 50;
    const int posY = 530;

    int dotCount = (GetNowCount() / 250) % 5;
    char loadText[32];
    if (dotCount == 0)      sprintf_s(loadText, "NOW LOADING");
    else if (dotCount == 1) sprintf_s(loadText, "NOW LOADING .");
    else if (dotCount == 2) sprintf_s(loadText, "NOW LOADING . .");
    else if (dotCount == 3) sprintf_s(loadText, "NOW LOADING . . .");
    else                    sprintf_s(loadText, "NOW LOADING . . . .");

    SetFontSize(24);
    DrawString(posX, posY, loadText, GetColor(255, 255, 255));

    int textWidth = GetDrawStringWidth(loadText, (int)strlen(loadText));
    int dotsStartX = posX + textWidth + 12;
    float timeVal = GetNowCount() * 0.012f;

    for (int d = 0; d < 4; d++) {
        float wave = sinf(timeVal - d * 0.7f);
        int jumpY = (wave > 0.0f) ? (int)(-wave * 6.0f) : 0;

        int dotX = dotsStartX + d * 10;
        int dotY = posY + 16 + jumpY;

        DrawCircle(dotX, dotY, 2, GetColor(255, 255, 255), TRUE);
    }

    SetFontSize(20);
    ScreenFlip();
    ProcessMessage();
}

void GameManager::UpdateVisualTimers() {
    for (auto& popup : popups) {
        if (!popup.active) continue;
        popup.offsetY += 0.5f;
        if (--popup.timer <= 0) popup.active = false;
    }
}

void GameManager::DrawStartCountdown(int sw, int sh, int img3, int img2, int img1) {
    if (startCountdownTimer <= 0) return;

    int centerX = sw / 2;
    int centerY = sh / 2;

    int currentPhase = 0;
    int phaseTimer = 0;

    if (startCountdownTimer > 330) {
        currentPhase = 3;
        phaseTimer = 420 - startCountdownTimer;
    }
    else if (startCountdownTimer > 250) {
        currentPhase = 2;
        phaseTimer = 330 - startCountdownTimer;
    }
    else if (startCountdownTimer > 140) {
        currentPhase = 1;
        phaseTimer = 250 - startCountdownTimer;
    }
    else {
        currentPhase = 0;
        phaseTimer = 140 - startCountdownTimer;
    }

    if (currentPhase >= 1) {
        int handle = (currentPhase == 3) ? img3 : (currentPhase == 2) ? img2 : img1;
            if (handle != -1) {
                float scale = 1.0f;
                int alpha = 255;

                if (phaseTimer < 20) {
                    float t = (float)phaseTimer / 20.0f;
                    scale = 1.4f - 0.4f * t;
                    alpha = (int)(255 * t);
                }
                else if (phaseTimer > 100) {
                    float t = (float)(phaseTimer - 100) / 20.0f;
                    scale = 1.0f + 0.2f * t;
                    alpha = (int)(255 * (1.0f - t));
                }

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                DrawRotaGraph(centerX, centerY, (double)scale, 0.0, handle, TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            }
    }
   
    else {
        float scale = 1.0f;
        int alpha = 255;

        if (phaseTimer < 15) {
            float t = (float)phaseTimer / 15.0f;
            scale = 1.8f - 0.8f * t;
        }
        else if (phaseTimer > 60) {
            float t = (float)(phaseTimer - 60) / 30.0f;
            alpha = (int)(255 * (1.0f - t));
        }

        SetFontSize((int)(64 * scale));
        const char* startText = "START!";
        int tw = GetDrawStringWidth(startText, (int)strlen(startText));

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawString(centerX - tw / 2 + 3, centerY - (int)(32 * scale) + 3, startText, GetColor(0, 0, 0));
        DrawString(centerX - tw / 2, centerY - (int)(32 * scale), startText, GetColor(255, 220, 50));

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        SetFontSize(20);


    }
}

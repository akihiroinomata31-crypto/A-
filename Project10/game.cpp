#include "game.h"
#include "main.h"
#include "player.h"
#include "HudText.h"
#include <math.h>
#include <string.h>
#include "../DXLib/DxLib.h"

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
    mainTimer = 100 * 60; // 60 Hz simulation
    bossCutsceneTimer = 0;
    bossCutsceneDone = false;
    bossSpawned = false;
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
    // 1. タイマー更新 ＆ 勝敗判定（3段階ルール）
    // ==========================================
    if (mainTimer > 0) {
        mainTimer--;
    }
    else if (gameState == 0) {
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
                else {
                    gameState = 3; // 引き分け
                }
            }
        }
    }

    // ==========================================
    // 2. 敵スポーン処理（同時生存上限22体で激重化を防止）
    // ==========================================
    int activeEnemyCount = 0;
    for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
        if (enemyList[i].mode != NONE && enemyList[i].enemyHP > 0) {
            activeEnemyCount++;
        }
    }

    const int MAX_FIELD_ENEMIES = 50; // ★ 同時生存上限

    if (activeEnemyCount < MAX_FIELD_ENEMIES) {

        // --- 通常ゴブリン ---
        goblinSpawnTimer++;
        if (goblinSpawnTimer >= 100) {
            goblinSpawnTimer = 0;

            for (int k = 0; k < 10; k++) {
                float spawnX = (float)(GetRand(4000) - 2000);
                float spawnZ = (float)(GetRand(4000) - 2000);
                ActivateEnemy(enemyList, spawnX, spawnZ);
            }
        }

        // --- 赤ゴブリン ---
        redSpawnTimer++;
        if (redSpawnTimer >= 500) {
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
    if (seconds <= 75 && gameState == 0) {
        if (!bossSpawned && enemyList[TEST_ENEMY_GOLEM].mode == NONE) {
            bossSpawned = true;
            enemyList[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 30.0f, -150.0f);
            enemyList[TEST_ENEMY_GOLEM].mode = STAND;
            enemyList[TEST_ENEMY_GOLEM].enemyHP = 20; // ボスHP
            enemyList[TEST_ENEMY_GOLEM].playtime = 0.0f;
            enemyList[TEST_ENEMY_GOLEM].isHit = false;

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

            // ★ ここで1度だけタイマーを起動（1.5秒間）
            if (!bossCutsceneDone) {
                bossCutsceneTimer = BOSS_CUTSCENE_TOTAL_FRAMES;
                bossCutsceneDone = true;
            }
        }
        // ★ 毎フレーム確実に減算し、0になったら通常画面へ自動復帰
        if (bossCutsceneTimer > 0) {
            bossCutsceneTimer--;
        }
    }
}


void GameManager::ActivateRedGoblin(SCharaInfo* enemyList, float x, float z) {
    for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++)
    {
        if (enemyList[i].mode == NONE) {
            // モデルが未複製なら複製
            if (enemyList[i].model1 == -1 && redGoblinBaseModel != -1) {
                enemyList[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
            }
            if (enemyList[i].model1 == -1) continue;

            enemyList[i].pos = VGet(x, 0.0f, z);

            // --------------------------------------------------------
            // 地面接地判定（レイキャスト）
            // --------------------------------------------------------
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
            enemyList[i].enemyHP = 6;
            enemyList[i].playtime = (float)GetRand(30) * 0.1f; // 全員が完全に同じタイミングで動かないよう再生位置を少しバラす
            enemyList[i].isHit = false;
            enemyList[i].currentAnimType = 1; // ★ 1: 待機モーション状態としてフラグを立てる

            MV1SetPosition(enemyList[i].model1, enemyList[i].pos);
            MV1SetVisible(enemyList[i].model1, TRUE);

            // --------------------------------------------------------
            // ★ アニメーションのアタッチ＆初期姿勢の即時適用（Tポーズ完全防止）
            // --------------------------------------------------------
            if (enemyList[i].attachidx != -1) {
                MV1DetachAnim(enemyList[i].model1, enemyList[i].attachidx);
                enemyList[i].attachidx = -1;
            }

            // 赤ゴブリン用がなければ通常ゴブリン用のアニメーションを使う（フォールバック）
            int useNeutralAnim = (red_goblin_anim_neutral != -1) ? red_goblin_anim_neutral : enemy_anim_neutral;

            if (useNeutralAnim != -1) {
                enemyList[i].attachidx = MV1AttachAnim(enemyList[i].model1, 0, useNeutralAnim);
                if (enemyList[i].attachidx != -1) {
                    enemyList[i].anim_totaltime = MV1GetAttachAnimTotalTime(enemyList[i].model1, enemyList[i].attachidx);
                    // ★ 0フレーム目をモデルの骨格に即座に反映（初手Tポーズを完全に防止）
                    MV1SetAttachAnimTime(enemyList[i].model1, enemyList[i].attachidx, enemyList[i].playtime);
                }
            }

            break; // 1体スポーンしたらループ終了
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

void GameManager::DrawUI(int pIdx, int sw, int sh, SCharaInfo* players, int hpBarTex, int crownGraphHandle, int pvpIconHandle, bool isPvPActive, int pvpCooldown) {
    HudText hud;
    int xOffset = (pIdx == 0) ? 0 : sw / 2;
    int halfW = sw / 2;
    hud.SetSize(20);

    // 決着テキスト
    if (gameState != 0) {
        hud.SetSize(60);
        int color = GetColor(255, 255, 0);
        if (gameState == 1)      hud.Draw(xOffset + 50, sh / 2 - 30, "PLAYER 1 WIN!", color);
        else if (gameState == 2) hud.Draw(xOffset + 50, sh / 2 - 30, "PLAYER 2 WIN!", color);
        else                     hud.Draw(xOffset + 100, sh / 2 - 30, "DRAW", color);
        hud.SetSize(20);
    }

    // 画面左上：プレイヤー名 ＆ HPバー
    int startY = 20;
    unsigned int pColor = (pIdx == 0) ? GetColor(100, 200, 255) : GetColor(255, 150, 100);
    hud.Format(xOffset + 20, startY, pColor, " PLAYER %d ", pIdx + 1);

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

    int currentDeaths = (pIdx == 0) ? deathCount[0] : deathCount[1];
    int infoY = drawY + 32;
    hud.Format(xOffset + 20, infoY, GetColor(255, 100, 200), "DEATHS: %d", currentDeaths);

    // 残り15秒以下のフィーバー表示
    int seconds = mainTimer / 60;
    if (gameState == 0 && seconds <= 15 && seconds > 0) {
        int feverColor = ((mainTimer / 15) % 2 == 0) ? GetColor(255, 50, 50) : GetColor(255, 220, 0);
        hud.SetSize(16);
        hud.Draw(xOffset + frameDrawW + 30, startY + 4, "FEVER! x2", feverColor);
        hud.SetSize(20);
    }

    // ========================================================
    // ★ 妨害（PvP攻撃）アイコン（画面左下）
    // ========================================================
    const int iconSize = 60;
    const int iconX = xOffset + 25;
    const int iconY = sh - 75;
    const int maxCooldownFrames = 60 * 3; // 3秒間

    if (pvpIconHandle != -1) {
        DrawExtendGraph(iconX, iconY, iconX + iconSize, iconY + iconSize, pvpIconHandle, TRUE);
    }

    // 1. 発動待機中（Vキーを押して次に当てる1撃が有効な状態）
    if (isPvPActive) {
        int pulse = 180 + (int)(sinf(GetNowCount() * 0.015f) * 75);
        SetDrawBlendMode(DX_BLENDMODE_ADD, pulse);
        DrawCircle(iconX + iconSize / 2, iconY + iconSize / 2, iconSize / 2 + 3, GetColor(255, 60, 60), FALSE);
        DrawCircle(iconX + iconSize / 2, iconY + iconSize / 2, iconSize / 2 + 4, GetColor(255, 60, 60), FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        SetFontSize(13);
        DrawString(iconX + iconSize + 10, iconY + 12, "TARGET READY!", GetColor(255, 80, 80));
        SetFontSize(20);
    }
    // 2. 攻撃命中後の3秒クールダウン中（暗転マスク ＋ カウントダウン数字）
    else if (pvpCooldown > 0) {
        float remainSec = (float)pvpCooldown / 60.0f;
        float cdRate = (float)pvpCooldown / (float)maxCooldownFrames;

        // 暗い半透明マスクで暗転
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 195);
        DrawBox(iconX, iconY, iconX + iconSize, iconY + iconSize, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 下からの赤いクールダウンゲージ
        int maskH = (int)(iconSize * cdRate);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
        DrawBox(iconX, iconY + (iconSize - maskH), iconX + iconSize, iconY + iconSize, GetColor(255, 40, 40), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 残り秒数
        SetFontSize(18);
        DrawFormatString(iconX + 17, iconY + 20, GetColor(255, 230, 230), "%.1f", remainSec);
        SetFontSize(20);
    }
    // 3. 通常待機時（いつでも発動可能）
    else {
        SetFontSize(12);
        DrawString(iconX + 3, iconY + iconSize + 2, (pIdx == 0) ? "[V] ATTACK" : "[1] ATTACK", GetColor(200, 200, 200));
        SetFontSize(20);
    }

    // ========================================================
    // スコア表示（アイコンの右隣に並ぶように配置）
    // ========================================================
    int currentScore = (pIdx == 0) ? p1Score : p2Score;
    char scoreStr[32];
    if (gameState == 0 && seconds <= 30 && seconds > 0) {
        sprintf_s(scoreStr, "Score: ???");
    }
    else {
        sprintf_s(scoreStr, "Score: %d", currentScore);
    }

    int scoreTextW = 0;
    if (fontScore != -1) {
        scoreTextW = GetDrawStringWidthToHandle(scoreStr, (int)strlen(scoreStr), fontScore);
    }
    else {
        scoreTextW = hud.Width(scoreStr, (int)strlen(scoreStr));
    }

    // 1Pはアイコンの右隣、2Pは右端揃え
    int scoreX = (pIdx == 0) ? (iconX + iconSize + 25) : (xOffset + halfW - scoreTextW - 30);
    int scoreY = sh - 55;

    if (fontScore != -1) {
        DrawStringToHandle(scoreX, scoreY, scoreStr, GetColor(0, 0, 0), fontScore, GetColor(255, 255, 255));
    }
    else {
        hud.SetSize(24);
        hud.Draw(scoreX, scoreY, scoreStr, GetColor(0, 0, 0));
        hud.SetSize(20);
    }

    // クラウン表示
    if (p1Score >= 1000 || p2Score >= 1000) {
        int leaderIdx = (p2Score > p1Score) ? 1 : 0;
        if (pIdx == leaderIdx) {
            int crownX = (pIdx == 0) ? (scoreX + scoreTextW + 25) : (scoreX - 25);
            int crownY = scoreY + 14;
            SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
            DrawRotaGraph(crownX, crownY, 0.3f, 0.0f, crownGraphHandle, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    // スコア加算ポップアップ
    for (int i = 0; i < 20; i++) {
        if (popups[i].active && popups[i].playerIdx == pIdx) {
            int popupBaseX = scoreX + 30;
            int popupBaseY = scoreY - 10 - (int)popups[i].offsetY;

            int alpha = 255;
            if (popups[i].timer < 20) {
                alpha = (popups[i].timer * 255) / 20;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            hud.SetSize(22);

            if (gameState == 0 && seconds <= 30 && seconds > 0) {
                hud.Format(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+?");
            }
            else {
                if (popups[i].score > 0) {
                    hud.Format(popupBaseX, popupBaseY, GetColor(255, 60, 60), "+%d", popups[i].score);
                }
                else {
                    hud.Format(popupBaseX, popupBaseY, GetColor(255, 0, 0), "%d", popups[i].score);
                }
            }

            hud.SetSize(20);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);


        }
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
    DrawStringToHandle(x, y, text, color, MenuFont(size));
}

static void MenuText(int y, int size, const char* text, unsigned int color) {
    const int font = MenuFont(size);
    DrawStringToHandle((900 - GetDrawStringWidthToHandle(text, (int)strlen(text), font)) / 2, y, text, color, font);
}

void DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot) {
    SetDrawArea(0, 0, 900, 600);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    ClearDrawScreen();

    const auto gold = GetColor(222, 194, 137), white = GetColor(235, 236, 229), muted = GetColor(127, 145, 153);

    // ========================================================
    // ★ タイトル背景画像（taitol.jpg）の読み込み＆全画面描画
    // ========================================================
    static int titleBgHandle = -1;
    if (titleBgHandle == -1) {
        titleBgHandle = LoadGraph("..\\Data\\UI\\taitol.jpg");
        if (titleBgHandle == -1) {
            titleBgHandle = LoadGraph("..\\Data\\UI\\taitol.png");
        }
    }

    if (titleBgHandle != -1) {
        // 画像を画面全体（900×600）に引き伸ばして描画
        DrawExtendGraph(0, 0, 900, 600, titleBgHandle, FALSE);

        // 文字や炎エフェクトを見やすくするため、画面全体に少し暗めの半透明マスクを重ねる
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 70);
        DrawBox(0, 0, 900, 600, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    else {
        // 画像が見つからない場合のフォールバック（従来のグラデーション）
        for (int y = 0; y < 600; ++y) {
            const float t = y / 600.0f;
            DrawLine(0, y, 900, y, GetColor(8 + (int)(6 * t), 17 + (int)(4 * t), 25 + (int)(5 * t)));
        }
    }

    // 漂う金色の火の粉・光の粒子演出
    const float drift = GetNowCount() * 0.00015f;
    SetDrawBlendMode(DX_BLENDMODE_ADD, 90);
    for (int i = 0; i < 25; ++i) {
        const int x = 30 + (i * 97 % 840);
        const int y = 40 + (int)fmodf(i * 53.0f + drift * 25, 520.0f);
        DrawCircle(x, y, i % 3 == 0 ? 2 : 1, GetColor(255, 180, 50), TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // ========================================================
    // タイトルメニュー画面（炎エフェクト ＆ メニュー）
    // ========================================================
    if (!results) {
        static int titleLogoHandle = -1;
        if (titleLogoHandle == -1) {
            SetTransColor(0, 0, 0);
            titleLogoHandle = LoadGraph("..\\Data\\UI\\genkai.png");
            if (titleLogoHandle == -1) {
                titleLogoHandle = LoadGraph("..\\Data\\UI\\TitleLogo.jpg");
            }
        }

        const int logoW = 680;
        const int logoH = 340;
        const int logoX = (900 - logoW) / 2; // 110 (中央揃え)
        const int logoY = 40;

        // ----------------------------------------------------
        // 1. メラメラ燃える炎オーラ（背面の加算揺らぎ）
        // ----------------------------------------------------
        const int now = GetNowCount();
        SetDrawBlendMode(DX_BLENDMODE_ADD, 130);

        for (int k = 0; k < 3; k++) {
            float wave = sinf(now * 0.008f + k * 1.6f);
            float cosWave = cosf(now * 0.006f + k * 2.1f);
            int offsetX = (int)(cosWave * 6.0f);
            int offsetY = (int)(wave * 5.0f) - 4;

            if (k == 0)      SetDrawBright(255, 70, 0);       // 深紅
            else if (k == 1) SetDrawBright(255, 140, 20);     // 橙炎
            else             SetDrawBright(255, 210, 50);     // 黄金芯

            if (titleLogoHandle != -1) {
                DrawExtendGraph(logoX + offsetX, logoY + offsetY,
                    logoX + logoW + offsetX, logoY + logoH + offsetY,
                    titleLogoHandle, TRUE);
            }
        }
        SetDrawBright(255, 255, 255);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // ----------------------------------------------------
        // 2. 炎から立ち昇る火の粉（スパーク）
        // ----------------------------------------------------
        SetDrawBlendMode(DX_BLENDMODE_ADD, 210);
        const int SPARK_COUNT = 36;
        for (int i = 0; i < SPARK_COUNT; i++) {
            float spawnRatio = (float)(i * 29 % 100) / 100.0f;
            float sparkBaseX = logoX + 50.0f + (logoW - 100.0f) * spawnRatio;

            float speed = 0.08f + (float)(i % 5) * 0.025f;
            float lifeProgress = fmodf((now + i * 140) * speed, 120.0f);
            float sway = sinf((now + i * 70) * 0.009f) * 12.0f;

            int sparkX = (int)(sparkBaseX + sway);
            int sparkY = (int)(logoY + 115 - lifeProgress);

            float alphaRate = 1.0f - (lifeProgress / 120.0f);
            int radius = (i % 3 == 0) ? 3 : 2;

            if (alphaRate > 0.0f) {
                unsigned int sparkColor = (i % 2 == 0) ? GetColor(255, 180, 40) : GetColor(255, 90, 20);
                DrawCircle(sparkX, sparkY, radius, sparkColor, TRUE);
            }
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // ----------------------------------------------------
        // 3. メインロゴ本体の描画
        // ----------------------------------------------------
        if (titleLogoHandle != -1) {
            DrawExtendGraph(logoX, logoY, logoX + logoW, logoY + logoH, titleLogoHandle, TRUE);
        }
        else {
            MenuLabel(340, 150, 64, "限界ラッシュ", gold);
        }

        // ----------------------------------------------------
        // 4. メニュー選択肢（下部中央）
        // ----------------------------------------------------
        const int menuLeft = 280;
        const int menuWidth = 340;
        const int menuStartY = 415;

        for (int row = 0; row < 2; ++row) {
            const int y = menuStartY + row * 58;
            if (row == selected) {
                // 選択中の発光バー
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 140);
                DrawBox(menuLeft, y, menuLeft + menuWidth, y + 42, GetColor(15, 20, 25), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
                DrawBox(menuLeft, y, menuLeft + 6, y + 42, gold, TRUE);
                DrawLine(menuLeft, y + 42, menuLeft + menuWidth, y + 42, gold);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            }
            else {
                // 非選択時は薄いダーク半透明
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
                DrawBox(menuLeft, y, menuLeft + menuWidth, y + 42, GetColor(0, 0, 0), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            }

            MenuLabel(menuLeft + 35, y + 7, 24, row == 1 ? "終了" : "ゲーム開始", row == selected ? gold : muted);
            if (row == selected) MenuLabel(menuLeft + menuWidth - 40, y + 7, 24, ">", gold);
        }
    }
    // ========================================================
    // リザルト画面
    // ========================================================
    else {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
        DrawBox(80, 65, 820, 416, GetColor(10, 15, 20), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        MenuText(85, 36, "試合結果", gold);
        MenuText(139, 36, game.gameState == 1 ? "プレイヤー１の勝利" : game.gameState == 2 ? "プレイヤー２の勝利" : "引き分け", white);
        DrawLine(120, 205, 780, 205, GetColor(53, 71, 78));
        MenuLabel(180, 229, 24, "プレイヤー１", GetColor(131, 187, 207));
        MenuLabel(525, 229, 24, "プレイヤー２", GetColor(219, 165, 140));
        char text[80];
        for (int p = 0; p < 2; ++p) {
            const int x = p == 0 ? 180 : 525;
            const int earned = p == 0 ? game.p1Score : game.p2Score;
            const int score = game.FinalScore(p);
            const int deathCost = game.deathCount[p] * GameManager::DeathPenalty;
            const int specialCost = game.specialUseCount[p] * GameManager::SpecialPenalty;
            sprintf_s(text, "獲得スコア   %d", earned); MenuLabel(x, 271, 16, text, muted);
            sprintf_s(text, "死亡 %d回   -%d", game.deathCount[p], deathCost); MenuLabel(x, 301, 16, text, muted);
            sprintf_s(text, "必殺技 %d回   -%d", game.specialUseCount[p], specialCost); MenuLabel(x, 331, 16, text, muted);
            sprintf_s(text, "合計   %d", score); MenuLabel(x, 366, 24, text, white);
        }

        const int left = 290, width = 320;
        for (int row = 0; row < 2; ++row) {
            const int y = 429 + row * 65;
            if (row == selected) {
                for (int x = 0; x < width; ++x) {
                    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 - x * 75 / width);
                    DrawLine(left + x, y, left + x, y + 49, GetColor(81, 99, 107));
                }
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                DrawBox(left, y, left + 3, y + 49, gold, TRUE);
                DrawLine(left, y + 49, left + width, y + 49, GetColor(77, 90, 92));
            }
            MenuLabel(left + 25, y + 8, 24, row == 1 ? "終了" : "メニューに戻る", row == selected ? gold : muted);
            if (row == selected) MenuLabel(left + width - 32, y + 8, 24, ">", gold);
        }
    }

    DrawLine(110, 548, 790, 548, GetColor(50, 45, 40));
    MenuText(566, 16, "↑↓ 選択      A / Enter 決定", muted);
    if (snapshot) SaveDrawScreenToPNG(0, 0, 900, 600, snapshot);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetFontSize(20);
    ScreenFlip();
}

void GameManager::DrawSpecialGauge(int pIdx, int startX, int endX, int screenHeight, int currentGauge, int maxGauge, int frameGraphHandle) {
    if (maxGauge <= 0) return;
    HudText hud;

    // HPバーの真下に配置
    int drawX = startX + 20;
    int drawY = 80;

    // フレームの表示サイズ（横幅260px、高さ34px）
    const int barW = 260;
    const int barH = 34;

    // 1. フレーム内側の領域（両端のエッジに合わせたマージン）
    // 左右は尖った飾りがあるため約12%、上下は約24%内側に収める
    int innerX1 = drawX + (int)(barW * 0.125f);
    int innerX2 = drawX + barW - (int)(barW * 0.125f);
    int innerY1 = drawY + (int)(barH * 0.24f);
    int innerY2 = drawY + barH - (int)(barH * 0.24f);

    int innerMaxW = innerX2 - innerX1;
    int innerH = innerY2 - innerY1;

    // 割合（0.0f 〜 1.0f）の計算
    float rate = (float)currentGauge / (float)maxGauge;
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    int currentFillW = (int)(innerMaxW * rate);

    // 2. ゲージの背景（暗いスロット底面）
    DrawBox(innerX1, innerY1, innerX2, innerY2, GetColor(15, 20, 28), TRUE);

    // 3. 青色クリスタルゲージの描画
    if (currentFillW > 0) {
        int fillX2 = innerX1 + currentFillW;

        // ① メインの青ゲージ下地（深みのあるコバルトブルー）
        DrawBox(innerX1, innerY1, fillX2, innerY2, GetColor(0, 110, 230), TRUE);

        // ② 上半分のハイライト（明るいシアンブルーで光沢感を出す）
        int midY = innerY1 + innerH / 2;
        DrawBox(innerX1, innerY1, fillX2, midY, GetColor(40, 180, 255), TRUE);

        // ③ 最上段のシャープな白い光筋（クリスタル感の強調）
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawBox(innerX1, innerY1, fillX2, innerY1 + 2, GetColor(200, 240, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // ④ ゲージ先端の光る境界ライン
        if (currentFillW < innerMaxW) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
            DrawLine(fillX2 - 1, innerY1, fillX2 - 1, innerY2, GetColor(220, 245, 255));
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    // 4. 上から金属フレームを描画（中身をきれいにマスクして挟み込む）
    if (frameGraphHandle != -1) {
        DrawExtendGraph(drawX, drawY, drawX + barW, drawY + barH, frameGraphHandle, TRUE);
    }

    // 5. MAX（満タン）時の演出（青白くパルス発光 ＋ READY! 表示）
    if (currentGauge >= maxGauge) {
        // フレーム全体にやわらかい光を重ねる
        int pulseAlpha = 80 + (int)(sinf(GetNowCount() * 0.008f) * 40);
        SetDrawBlendMode(DX_BLENDMODE_ADD, pulseAlpha);
        DrawBox(innerX1, innerY1, innerX2, innerY2, GetColor(50, 150, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 「READY!」の点滅文字表示
        int textAlpha = 180 + (int)(sinf(GetNowCount() * 0.008f) * 75);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, textAlpha);
        SetFontSize(15);
        DrawString(drawX + barW + 8, drawY + 9, "READY!", GetColor(100, 220, 255));
        SetFontSize(20);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}



// ----------------------------------------------------
// ボスカメラ座標計算
// ----------------------------------------------------
bool GameManager::ApplyBossCamera(VECTOR& outCamPos, VECTOR& outCamTarget, const SCharaInfo* enemyList) {
    // タイマーが0、またはボスがいない場合は false を返して通常カメラへ切り替える
    if (bossCutsceneTimer <= 0) return false;
    if (enemyList[TEST_ENEMY_GOLEM].mode == NONE || enemyList[TEST_ENEMY_GOLEM].model1 == -1) return false;

    VECTOR bossPos = enemyList[TEST_ENEMY_GOLEM].pos;

    // ボスの胸〜頭あたりを注視
    outCamTarget = VAdd(bossPos, VGet(0.0f, 130.0f, 0.0f));

    // 進行度（1.0 -> 0.0）
    float progress = (float)bossCutsceneTimer / (float)BOSS_CUTSCENE_TOTAL_FRAMES;

    // 1.5秒の間、外側から顔正面へスムーズにズームイン
    float zoomDist = 240.0f + (progress * 180.0f);
    float camHeight = 110.0f + (progress * 40.0f);

    // ボスの正面（Z手前側）にカメラを配置
    outCamPos = VGet(bossPos.x, bossPos.y + camHeight, bossPos.z + zoomDist);
    return true;
}

// ----------------------------------------------------
// テロップ描画
// ----------------------------------------------------
void GameManager::DrawBossCutsceneUI(int startX, int endX, int screenHeight) const {
    if (bossCutsceneTimer <= 0) return;

    int screenCenterX = (startX + endX) / 2;
    int screenCenterY = screenHeight / 2;

    // 終了前のフェードアウト（残り30フレームでスーッと透明になる）
    int bandAlpha = 190;
    if (bossCutsceneTimer < 30) {
        bandAlpha = (bossCutsceneTimer * 190) / 30;
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, bandAlpha);
    DrawBox(startX, screenCenterY - 45, endX, screenCenterY + 45, GetColor(0, 0, 0), TRUE);
    DrawLine(startX, screenCenterY - 45, endX, screenCenterY - 45, GetColor(230, 30, 30), 2);
    DrawLine(startX, screenCenterY + 45, endX, screenCenterY + 45, GetColor(230, 30, 30), 2);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // 文字サイズ・位置計算
    SetFontSize(36);
    const char* bossText = "BOSS戦開始";
    int textWidth = GetDrawStringWidth(bossText, (int)strlen(bossText));
    int textX = screenCenterX - (textWidth / 2);
    int textY = screenCenterY - 18 - 8;

    // 黒縁取り
    DrawString(textX - 2, textY, bossText, GetColor(0, 0, 0));
    DrawString(textX + 2, textY, bossText, GetColor(0, 0, 0));
    DrawString(textX, textY - 2, bossText, GetColor(0, 0, 0));
    DrawString(textX, textY + 2, bossText, GetColor(0, 0, 0));

    // 発光文字
    DrawString(textX, textY, bossText, GetColor(255, 60, 40));

    // サブタイトル
    SetFontSize(14);
    const char* subText = "- WARNING: GOLEM APPEARED -";
    int subWidth = GetDrawStringWidth(subText, (int)strlen(subText));
    DrawString(screenCenterX - (subWidth / 2), screenCenterY + 18, subText, GetColor(240, 200, 50));

    SetFontSize(20);
}
// ========================================================
// シンプル黒画面ロード（左下で NOW LOADING.... が動く）
// ========================================================
void GameManager::DrawLoadingScreen() {
    SetDrawArea(0, 0, 900, 600);
    ClearDrawScreen();

    // 1. 完全な黒画面
    DrawBox(0, 0, 900, 600, GetColor(0, 0, 0), TRUE);

    // 2. 左下の表示座標
    const int posX = 50;
    const int posY = 530;

    // 3. ドット（.）の数を 0〜4個 に周期的に切り替える（約0.25秒ごと）
    int dotCount = (GetNowCount() / 250) % 5;
    char loadText[32];
    if (dotCount == 0)      sprintf_s(loadText, "NOW LOADING");
    else if (dotCount == 1) sprintf_s(loadText, "NOW LOADING .");
    else if (dotCount == 2) sprintf_s(loadText, "NOW LOADING . .");
    else if (dotCount == 3) sprintf_s(loadText, "NOW LOADING . . .");
    else                    sprintf_s(loadText, "NOW LOADING . . . .");

    // 4. 文字を描画（白文字）
    SetFontSize(24);
    DrawString(posX, posY, loadText, GetColor(255, 255, 255));

    // 5. 文字のすぐ右側で、4つの白い光の点が波打って跳ねる演出
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

#include "PauseAndGameOver.h"

bool PauseAndGameOver::DrawPause()
{
    // dark overlay
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));

    // pause box
    int boxWidth = 420;
    int boxHeight = 240;

    int boxX = GetScreenWidth() / 2 - boxWidth / 2;
    int boxY = GetScreenHeight() / 2 - boxHeight / 2;

    DrawRectangle(boxX, boxY, boxWidth, boxHeight, BLACK);
    DrawRectangleLines(boxX, boxY, boxWidth, boxHeight, WHITE);

    // title
    int titleSize = 60;
    int titleWidth = MeasureText("PAUSED", titleSize);

    DrawText("PAUSED", GetScreenWidth() / 2 - titleWidth / 2,
        boxY + 30, titleSize, WHITE);

    // resume text
    const char* resumeText = "Press ESC to resume";
    int resumeSize = 22;
    int resumeWidth = MeasureText(resumeText, resumeSize);

    DrawText(resumeText, GetScreenWidth() / 2 - resumeWidth / 2,
        boxY + 120,  resumeSize, RAYWHITE);

    // quit button
    int quitWidth = 200;
    int quitHeight = 55;

    int quitX = GetScreenWidth() - quitWidth - 25;
    int quitY = GetScreenHeight() - quitHeight - 25;

    Rectangle quitButton =
    {
        (float)quitX,
        (float)quitY,
        (float)quitWidth,
        (float)quitHeight
    };

    DrawRectangleRec(quitButton, RED);
    DrawRectangleLinesEx(quitButton, 3, WHITE);

    const char* quitText = "Quit Game";
    int quitFont = 24;
    int quitTextWidth = MeasureText(quitText, quitFont);

    DrawText(quitText, quitX + quitWidth / 2 - quitTextWidth / 2,
        quitY + quitHeight / 2 - quitFont / 2,quitFont, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), quitButton) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return true;
    }

    return false;
}

bool PauseAndGameOver::DrawGameOver(int wave, float gameTimer)
{
    int gap = 50;
    int gameOverFont = 180;
    int fontSize = 30;

    ClearBackground(BLACK);

    DrawText("GAME OVER", gap, GetScreenHeight() / 2 - 200,
        gameOverFont, RED);

    DrawText("Press ENTER to return to play again", GetScreenWidth() / 2 - 250,
        (GetScreenHeight() * 3 / 4), fontSize, RAYWHITE);

    DrawText(TextFormat("Wave Reached: %d", wave), GetScreenWidth() / 2 - 120,
        GetScreenHeight() / 2, fontSize,YELLOW);

    DrawText(TextFormat("Time Survived: %.1f", gameTimer), GetScreenWidth() / 2 - 120, 
        GetScreenHeight() / 2 + 50,  fontSize, YELLOW);

    // quit button
    int quitWidth = 200;
    int quitHeight = 55;

    int quitX = GetScreenWidth() - quitWidth - 25;
    int quitY = GetScreenHeight() - quitHeight - 25;

    Rectangle quitButton =
    {
        (float)quitX,
        (float)quitY,
        (float)quitWidth,
        (float)quitHeight
    };

    DrawRectangleRec(quitButton, RED);
    DrawRectangleLinesEx(quitButton, 3, WHITE);

    const char* quitText = "Quit Game";
    int quitFont = 24;
    int quitTextWidth = MeasureText(quitText, quitFont);

    DrawText(quitText, quitX + quitWidth / 2 - quitTextWidth / 2,
        quitY + quitHeight / 2 - quitFont / 2, quitFont, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), quitButton) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return true;
    }

    return false;
}
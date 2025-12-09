// 简易打字游戏：随机字母从窗口顶部下落，按键击中对应字母即可得分。
// 若字母掉到底部未被击中则记为失分；随着得分增加，下落速度会逐渐加快。

#include <windows.h>
#include <time.h>
#include <string>

// 游戏区域和状态数据
RECT   g_rcPlay;             // 客户区左侧可玩区域（含边距）
RECT   g_rcBox;              // 实际游戏矩形框
TCHAR  g_chFalling = 'A';    // 当前下落字母
int    g_nY = 0;             // 字母当前纵坐标
int    g_nScore = 0;         // 当前得分
int    g_nMiss = 0;          // 当前失分
int    g_nSpeed = 8;         // 每次定时器刷新向下移动的像素
UINT   g_uTimerId = 1;       // 定时器 ID
int    g_charWidth = 0;      // 字母字体宽度
int    g_charHeight = 0;     // 字母字体高度
HFONT  g_hFontLetter = NULL; // 下落字母和底部提示使用的等宽字体
HFONT  g_hFontInfo   = NULL; // 信息区字体

// 游戏状态
int lives = 5;               // 玩家生命值
bool gameOver = false;       // 游戏是否结束
bool paused = false;         // 是否暂停
int difficulty = 1;          // 1=简单 2=普通 3=困难
int letterCount = 1;         // 当前下落字母数量
int fallSpeed = 8;           // 当前下落速度
int highScore = 0; // 最高分纪录
int combo = 0;    // 连击数
int maxCombo = 0; // 最大连击

struct FallingLetter {
    TCHAR ch;
    int y;
};
FallingLetter letters[3];    // 最多3个字母

// 根据当前得分动态调整速度（简单难度提升策略）
void UpdateSpeed()
{
    // 根据难度设置速度
    if (difficulty == 1) fallSpeed = 8;
    else if (difficulty == 2) fallSpeed = 13;
    else fallSpeed = 18;
}

// 生成一个新的随机字母并把位置重置到顶部
void NewLetter(HWND hWnd)
{
    UpdateSpeed();
    // 根据难度设置下落字母数量
    if (difficulty == 1) letterCount = 1;
    else if (difficulty == 2) letterCount = 2;
    else letterCount = 3;
    for (int i = 0; i < letterCount; ++i) {
        letters[i].ch = 'A' + rand() % 26;
        letters[i].y = 0;
    }
    InvalidateRect(hWnd, NULL, TRUE);
}

// 绘制游戏画面
void PaintGame(HWND hWnd, HDC hdc)
{
    // 在 PaintGame 函数开头，动态计算字号和布局
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int winW = rcClient.right - rcClient.left;
    int winH = rcClient.bottom - rcClient.top;
    int margin = winH / 20;
    int infoWidth = max(winW / 5, 120);
    int fontSize = max(winH / 20, 16);

    // 创建自适应字体
    if (g_hFontLetter) DeleteObject(g_hFontLetter);
    g_hFontLetter = CreateFontW(-fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, TEXT("Consolas"));
    if (g_hFontInfo) DeleteObject(g_hFontInfo);
    g_hFontInfo = CreateFontW(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, TEXT("Segoe UI"));

    // 游戏区和信息区动态布局
    int playRight = winW - infoWidth;
    g_rcPlay = rcClient;
    g_rcPlay.right = playRight;
    int marginLeft   = margin;
    int marginTop    = margin;
    int marginRight  = margin;
    int marginBottom = margin * 2;
    g_rcBox = {
        g_rcPlay.left + marginLeft,
        g_rcPlay.top + marginTop,
        g_rcPlay.right - marginRight,
        g_rcPlay.bottom - marginBottom
    };

    // 画出游戏矩形边框
    Rectangle(hdc, g_rcBox.left, g_rcBox.top, g_rcBox.right, g_rcBox.bottom);

    // 字体和字母声明
    HFONT oldFont = NULL;
    TCHAR szKeys[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    // 底部和下落字母都用同一个字体对象 g_hFontLetter
    if (g_hFontLetter)
        oldFont = (HFONT)SelectObject(hdc, g_hFontLetter);
    SetTextAlign(hdc, TA_CENTER | TA_TOP);

    // 等分下落区域宽度，每个字母居中
    int availableWidth = g_rcBox.right - g_rcBox.left;
    int cellW = availableWidth / 26;
    int bottomStartX = g_rcBox.left;
    // 绘制底部字母
    int bottomY = g_rcBox.bottom - g_charHeight - 8;
    for (int i = 0; i < 26; ++i)
    {
        int centerX = bottomStartX + cellW * i + cellW / 2;
        wchar_t wch = szKeys[i];
        TextOutW(hdc, centerX, bottomY, &wch, 1);
    }
    // 绘制下落字母
    for (int i = 0; i < letterCount; ++i) {
        int index = letters[i].ch - 'A';
        index = max(0, min(25, index));
        int centerX = bottomStartX + cellW * index + cellW / 2;
        int letterY = g_rcBox.top + 8 + letters[i].y;
        wchar_t wch = letters[i].ch;
        SetTextColor(hdc, RGB(0, 0, 0));
        TextOutW(hdc, centerX, letterY, &wch, 1);
    }
    if (oldFont)
        SelectObject(hdc, oldFont);

    // 在右侧显示得分与失分
    RECT rcInfo = rcClient;
    rcInfo.left = g_rcPlay.right + 30; // 信息区稍微右移一点

    TCHAR buf[64];
    if (g_hFontInfo)
        oldFont = (HFONT)SelectObject(hdc, g_hFontInfo);
    SetTextAlign(hdc, TA_LEFT | TA_TOP);

    int infoX = rcInfo.left;
    int infoY = rcClient.top + 100;
    wsprintf(buf, TEXT("当前得分: %d"), g_nScore);
    TextOut(hdc, infoX, infoY, buf, lstrlen(buf));

    infoY += 40;
    wsprintf(buf, TEXT("当前失误: %d"), g_nMiss);
    TextOut(hdc, infoX, infoY, buf, lstrlen(buf));

    // 分数、生命、难度、暂停提示一起显示在左上角
    wsprintf(buf, TEXT("分数:%d  生命:%d  难度:%s (1/2/3切换)  4-暂停/继续  最高分:%d  连击:%d"),
        g_nScore, lives,
        (difficulty == 1 ? TEXT("简单") : (difficulty == 2 ? TEXT("普通") : TEXT("困难"))),
        highScore, combo);
    SetTextColor(hdc, RGB(255, 128, 0));
    TextOut(hdc, g_rcPlay.left + 10, g_rcPlay.top + 10, buf, lstrlen(buf));

    // 暂停时中央显示提示
    if (paused && !gameOver) {
        wsprintf(buf, TEXT("已暂停，按4继续"));
        SetTextColor(hdc, RGB(0, 128, 255));
        TextOut(hdc, g_rcPlay.left + 120, g_rcPlay.top + 180, buf, lstrlen(buf));
    }

    // 游戏失败提示（生命扣完）
    if (gameOver) {
        wsprintf(buf, TEXT("游戏失败! 按R重开 最高分:%d  最大连击:%d"), highScore, maxCombo);
        SetTextColor(hdc, RGB(255, 0, 0));
        TextOut(hdc, g_rcPlay.left + 80, g_rcPlay.top + 180, buf, lstrlen(buf));
    }

    if (oldFont)
        SelectObject(hdc, oldFont);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        srand((unsigned int)time(NULL));
        NewLetter(hWnd);
        SetTimer(hWnd, g_uTimerId, 80, NULL); // 每 80ms 刷新一次位置

        // 创建不同用途的字体
        g_hFontLetter = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, TEXT("Consolas"));
        g_hFontInfo = CreateFontW(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, TEXT("Segoe UI"));

        if (g_hFontLetter)
        {
            HDC hdc = GetDC(hWnd);
            HFONT old = (HFONT)SelectObject(hdc, g_hFontLetter);
            SIZE size;
            if (GetTextExtentPoint32(hdc, TEXT("A"), 1, &size))
            {
                g_charWidth = size.cx;
                g_charHeight = size.cy;
            }
            SelectObject(hdc, old);
            ReleaseDC(hWnd, hdc);
        }
        return 0;

    case WM_TIMER:
        if (wParam == g_uTimerId && !paused && !gameOver)
        {
            int boxHeight = g_rcBox.bottom - g_rcBox.top;
            int charHeight = g_charHeight > 0 ? g_charHeight : 28;
            int maxY = boxHeight > 0 ? boxHeight - charHeight - 5 : (g_rcPlay.bottom - g_rcPlay.top) - 120;
            bool missed = false;
            for (int i = 0; i < letterCount; ++i) {
                letters[i].y += fallSpeed;
                if (letters[i].y >= maxY) {
                    missed = true;
                }
            }
            if (missed) {
                g_nMiss++;
                lives--;
                combo = 0; // 连击断了
                if (lives <= 0) {
                    lives = 0;
                    gameOver = true;
                    if (g_nScore > highScore) highScore = g_nScore;
                    KillTimer(hWnd, g_uTimerId);
                } else {
                    NewLetter(hWnd);
                }
            }
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;

    case WM_CHAR:
    {
        // 接收用户键盘输入，对大小写一并处理
        TCHAR ch = (TCHAR)wParam;
        if (ch >= 'a' && ch <= 'z')
            ch = ch - 'a' + 'A';

        if (!gameOver && lives > 0) {
            for (int i = 0; i < letterCount; ++i) {
                if (ch == letters[i].ch && letters[i].y < (g_rcBox.bottom - g_rcBox.top)) {
                    combo++;
                    if (combo > maxCombo) maxCombo = combo;
                    int addScore = (combo >= 3 ? 2 : 1); // 连击>=3分数加倍
                    g_nScore += addScore;
                    letters[i].ch = 'A' + rand() % 26;
                    letters[i].y = 0;
                }
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        // 处理游戏暂停与继续
        if (wParam == VK_SPACE)
        {
            paused = !paused;
            if (paused) KillTimer(hWnd, g_uTimerId);
            else if (!gameOver) SetTimer(hWnd, g_uTimerId, 80, NULL);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        // 处理重新开始
        else if (wParam == 'R' || wParam == 'r')
        {
            if (gameOver)
            {
                // 重置游戏状态
                gameOver = false;
                g_nScore = 0;
                g_nMiss = 0;
                lives = 5;
                paused = false;
                combo = 0;
                maxCombo = 0;
                NewLetter(hWnd);
                InvalidateRect(hWnd, NULL, TRUE);
                SetTimer(hWnd, g_uTimerId, 80, NULL);
            }
        }
        // 难度选择
        if (!gameOver && !paused) {
            if (wParam == '1') { difficulty = 1; NewLetter(hWnd); }
            else if (wParam == '2') { difficulty = 2; NewLetter(hWnd); }
            else if (wParam == '3') { difficulty = 3; NewLetter(hWnd); }
        }
        // 处理游戏暂停与继续
        if (wParam == '4') {
            paused = !paused;
            if (paused) KillTimer(hWnd, g_uTimerId);
            else if (!gameOver) SetTimer(hWnd, g_uTimerId, 80, NULL);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        PaintGame(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hWnd, g_uTimerId);
        if (g_hFontLetter)
        {
            DeleteObject(g_hFontLetter);
            g_hFontLetter = NULL;
        }
        if (g_hFontInfo)
        {
            DeleteObject(g_hFontInfo);
            g_hFontInfo = NULL;
        }
        g_charWidth = g_charHeight = 0;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Windows GUI 程序入口
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    const wchar_t CLASS_NAME[] = L"TypingGameWnd";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"简易打字游戏",
        WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 700,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
        return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// 简易打字游戏：随机字母从窗口顶部下落，按键击中对应字母即可得分。
// 若字母掉到底部未被击中则记为失分；随着得分增加，下落速度会逐渐加快。

#include <windows.h>
#include <time.h>

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

// 根据当前得分动态调整速度（简单难度提升策略）
void UpdateSpeed()
{
    if (g_nScore < 10)
        g_nSpeed = 8;
    else if (g_nScore < 20)
        g_nSpeed = 10;
    else if (g_nScore < 40)
        g_nSpeed = 13;
    else
        g_nSpeed = 16;
}

// 生成一个新的随机字母并把位置重置到顶部
void NewLetter(HWND hWnd)
{
    g_chFalling = 'A' + rand() % 26;
    g_nY = 0;
    UpdateSpeed();
    InvalidateRect(hWnd, NULL, TRUE);
}

// 绘制游戏画面
void PaintGame(HWND hWnd, HDC hdc)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    // 左侧为游戏区域，右侧显示得分信息
    g_rcPlay = rcClient;
    int infoWidth = 160;            // 右侧信息栏宽度
    g_rcPlay.right -= infoWidth;    // 调整左侧游戏区域宽度

    // 为了更接近教材效果，预留出内部边距
    int marginLeft   = 50;
    int marginTop    = 40;
    int marginRight  = 20;
    int marginBottom = 60;          // 底部留出一行字母提示

    g_rcBox = {
        g_rcPlay.left + marginLeft,
        g_rcPlay.top + marginTop,
        g_rcPlay.right - marginRight,
        g_rcPlay.bottom - marginBottom
    };

    // 画出游戏矩形边框
    Rectangle(hdc, g_rcBox.left, g_rcBox.top, g_rcBox.right, g_rcBox.bottom);

    // 落下字母与底部提示字母左对齐，保持大小一致
    TCHAR sz[2] = { g_chFalling, 0 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT oldFont = NULL;
    if (g_hFontLetter)
        oldFont = (HFONT)SelectObject(hdc, g_hFontLetter);

    // 计算字母宽度，用于对齐
    if (g_charWidth == 0 || g_charHeight == 0)
    {
        SIZE size;
        GetTextExtentPoint32(hdc, TEXT("A"), 1, &size);
        g_charWidth = size.cx;
        g_charHeight = size.cy;
    }

    int index = g_chFalling - 'A';
    index = max(0, min(25, index));

    int totalWidth = g_charWidth * 26;
    int playWidth = g_rcBox.right - g_rcBox.left;
    int leftOffset = 0;
    if (totalWidth < playWidth)
        leftOffset = (playWidth - totalWidth) / 2;

    // 下落字母绘制位置：与底部字母行保持同一列
    int letterX = g_rcBox.left + leftOffset + index * g_charWidth;
    int letterY = g_rcBox.top + 8 + g_nY;
    SetTextAlign(hdc, TA_LEFT | TA_TOP);
    TextOut(hdc, letterX, letterY, sz, 1);

    if (oldFont)
        SelectObject(hdc, oldFont);

    // 底部显示键盘上的 26 个字母，提示玩家可以按的键
    TCHAR szKeys[] = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    // 在客户端区域内靠左下方显示一行字母，避免超出窗口
    int bottomY = g_rcBox.bottom - g_charHeight - 8;
    if (g_hFontLetter)
        oldFont = (HFONT)SelectObject(hdc, g_hFontLetter);
    SetTextAlign(hdc, TA_LEFT | TA_TOP);

    int bottomStartX = g_rcBox.left + leftOffset;
    for (int i = 0; i < 26; ++i)
    {
        TextOut(hdc, bottomStartX + i * g_charWidth, bottomY, szKeys + i, 1);
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
        if (wParam == g_uTimerId)
        {
            g_nY += g_nSpeed;

            // 判断是否到达底部（这里用游戏矩形内部的下边缘做判断）
            int boxHeight = g_rcBox.bottom - g_rcBox.top;
            int charHeight = g_charHeight > 0 ? g_charHeight : 28;
            int maxY = boxHeight > 0 ? boxHeight - charHeight - 5 : (g_rcPlay.bottom - g_rcPlay.top) - 120;
            if (g_nY >= maxY)
            {
                // 掉到底部且未被击中，算一次失误
                g_nMiss++;
                NewLetter(hWnd);
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

        if (ch == g_chFalling)
        {
            g_nScore++;
            NewLetter(hWnd);
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
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, // 禁止拖动改变大小，界面更接近教材示例
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
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

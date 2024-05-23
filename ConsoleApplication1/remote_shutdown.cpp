#include <stdio.h>
#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS
#define TRAY_ICON_ID 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TASKBAR_CREATED RegisterWindowMessage(TEXT("TaskbarCreated"))
#define MODE_SHUTDOWN 0
#define MODE_HIBERNATE 1
#pragma comment(lib, "ws2_32.lib")

HANDLE hPortListenerThread;
NOTIFYICONDATA nid;
SOCKET listenSocket, clientSocket;

//多线程全局关闭标志
bool g_bProgramRunning = true;

//全局关机设置标志位
int g_bShutdownFlags = MODE_SHUTDOWN;

void remote_shutdown(int shutdownFlags) {
    switch (shutdownFlags) {
    case 0:
        /*MessageBox(NULL, L"假设关机成功", L"debug", 0);*/
        system("shutdown -s -f -t 10");
        break;
    case 1:
        system("shutdown -h");
        /*MessageBox(NULL, L"假设休眠成功", L"debug", 0);*/
        break;
    default:
        system("shutdown -s -f -t 60");
        break;
    }
    
}

// 托盘图标回调函数
LRESULT CALLBACK TrayIconCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_TRAYICON && wParam == TRAY_ICON_ID)
    {
        if (lParam == WM_LBUTTONUP)
        {
            // 鼠标左键单击事件处理
            // 显示提示文字
            MessageBox(hWnd, L"监听程序运行中，非必要请勿关闭进程", L"提示", MB_ICONINFORMATION);
        }
        else if (lParam == WM_RBUTTONUP)
        {
            // 鼠标右键单击事件处理
            HMENU hPopupMenu = CreatePopupMenu();
            AppendMenu(hPopupMenu, MF_STRING, 1, L"退出");
            AppendMenu(hPopupMenu, MF_STRING, 2, L"休眠");
            AppendMenu(hPopupMenu, MF_STRING, 3, L"关机");
            SetForegroundWindow(hWnd);
            // 获取鼠标位置
            POINT cursorPos;
            GetCursorPos(&cursorPos); 
          
            int menuflags = TrackPopupMenu(hPopupMenu, TPM_RETURNCMD| TPM_LEFTALIGN | TPM_LEFTBUTTON, cursorPos.x, cursorPos.y, 0, hWnd, NULL);
            switch (menuflags)
            {
            case 1:

            {int retchoice = MessageBox(hWnd, L"退出该程序会导致远程关机功能失效，确定退出吗", L"提示", MB_YESNO | MB_ICONINFORMATION); 
            if (retchoice == IDNO)
                DestroyMenu(hPopupMenu);
            else if (retchoice == IDYES)
            {
                // 关闭监听线程
                g_bProgramRunning = false;
                WaitForSingleObject(hPortListenerThread, INFINITE);
                CloseHandle(hPortListenerThread);
                closesocket(clientSocket);
                closesocket(listenSocket);
                WSACleanup();
                //MessageBox(NULL, L"线程已关闭", L"debug", 0);
                PostQuitMessage(0);
            }
                    break;
            }

            case 2:

                MessageBox(NULL, L"已经切换为休眠模式", L"注意", 0);
                g_bShutdownFlags = MODE_HIBERNATE;
                break;
            case 3:
                MessageBox(NULL, L"已经切换为关机模式", L"注意", 0);
                g_bShutdownFlags = MODE_SHUTDOWN;
                break;
            default:
                DestroyMenu(hPopupMenu);
                break;
     
             }  
            
           
        }
    }
    if (message == WM_TASKBAR_CREATED)
    {
        //系统Explorer崩溃重启时，重新加载托盘
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

//多线程处理监听
DWORD WINAPI HandleHttpRequest(LPVOID lpParam) {
    WSADATA wsaData;
    char buffer[1024]="0";
    /*SOCKET listenSocket, clientSocket;*///声明在全局
    struct sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);
   
   /* 注意，http1.1 下Content - Length的具体字节数应与body中实际的字节数一致
    否则response会出现各种错误*/
    const char* response = "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
       /* "Content-Length: 12\r\n"*/
        "\r\n"
       "request recieved";

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(NULL, L"初始化失败", L"debug", 0);
        return 1;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        MessageBox(NULL, L"创建socket失败", L"debug", 0);
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5001);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBox(NULL, L"绑定失败", L"debug", 0);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, 1) == SOCKET_ERROR) {
        MessageBox(NULL, L"监听失败", L"debug", 0);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }


    while (g_bProgramRunning) {
        clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientSocket == INVALID_SOCKET) {
            MessageBox(NULL, L"客户端连接失败", L"debug", 0);
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead != SOCKET_ERROR) {
              
           /* wchar_t wideBuffer[1024]=L"{0}"; 
            MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, wideBuffer, sizeof(wideBuffer) / sizeof(wchar_t));
            MessageBox(NULL, wideBuffer, L"Received Request", MB_OK);*/

            char sendcodeChar[256] = "{0}";
            
            int sendcode = send(clientSocket, response, (int)strlen(response), 0);
            _itoa_s(sendcode, sendcodeChar, 10);
            if (sendcode == SOCKET_ERROR) {
                MessageBox(NULL, L"SEND ERROR", L"debug", MB_OK);
                FILE* outputFile;
                if (fopen_s(&outputFile, "output.txt", "w") == 0) {
                    fprintf(outputFile, "Received send:\n%s\n", WSAGetLastError());
                    fclose(outputFile);
                }
            }
           /* else {
                wchar_t sendcodeStr[100] = L"{0}";
                MultiByteToWideChar(CP_UTF8, 0, sendcodeChar, bytesRead, sendcodeStr, sizeof(sendcodeStr) / sizeof(wchar_t));
                MessageBox(NULL, sendcodeStr, L"debug", MB_OK);
            }*/


        }
        
        if (g_bShutdownFlags == MODE_SHUTDOWN) {
            remote_shutdown(g_bShutdownFlags);
            closesocket(clientSocket);
            break;
        }
        else if(g_bShutdownFlags == MODE_HIBERNATE){
            remote_shutdown(g_bShutdownFlags);
            /*MessageBox(NULL, L"假设休眠成功", L"debug", 0);*/
            closesocket(clientSocket);
        //    break;
        }
       
    }
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    HANDLE hPortListenerThread = CreateThread(NULL, 0, HandleHttpRequest, NULL, 0, NULL);
    if (hPortListenerThread == NULL)
    {
        MessageBox(NULL, L"监听线程创建失败", L"debug", 0);
        return 1;
    }
   /* HANDLE lPortListenerThread = CreateThread(NULL, 0, Trayapp, NULL, 0, NULL);
    if (lPortListenerThread == NULL)
    {
        MessageBox(NULL, L"托盘创建失败", L"debug", 0);
        return 1;
    }*/
    // 创建窗口类
    WNDCLASSEX wndClass = { 0 };
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.lpfnWndProc = TrayIconCallback;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = L"MyTrayAppClass";

    // 注册窗口类
    RegisterClassEx(&wndClass);

    // 创建窗口
    HWND hWnd = CreateWindowEx(0, L"MyTrayAppClass", L"MyTrayApp", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    // 加载托盘图标
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP| NIF_INFO;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpy(nid.szInfo, L"远关程序已启动");
    lstrcpy(nid.szInfoTitle, L"注意");

    Shell_NotifyIcon(NIM_ADD, &nid);
    lstrcpyn(nid.szTip, L"5001监听中", sizeof(nid.szTip));
    // 在系统托盘中添加图标
    Shell_NotifyIcon(NIM_MODIFY, &nid);
   

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!g_bProgramRunning)
        {
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            break;
        }
       
    }

    // 移除托盘图标
    Shell_NotifyIcon(NIM_DELETE, &nid);
    return static_cast<int>(msg.wParam);

    
}

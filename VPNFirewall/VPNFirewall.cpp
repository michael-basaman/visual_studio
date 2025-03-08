// VPNFirewall.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "combaseapi.h"
#include "VPNFirewall.h"
#include "Provider.h"

#include <fstream>
#include <sstream>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

HMENU installButtonHandle = (HMENU)0xFFFF1;
HMENU uninstallButtonHandle = (HMENU)0xFFFF2;
HMENU listViewHandle = (HMENU)0xFFFF3;

HWND hWnd = 0;
HWND installButton = 0;
HWND uninstallButton = 0;
HWND hWndListView = 0;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    //LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VPNFIREWALL, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    wcscpy_s(szTitle, L"VPN Firewall");

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VPNFIREWALL));

    MSG msg;

    Provider provider;
    //provider.PrintMask16("192.168");
    //provider.Install();
    //provider.Uninstall();

    /*GUID providerKey;
    HRESULT hCreateGuid = CoCreateGuid(&providerKey);
    
    char providerKeyString[256];
    memset(providerKeyString, 0, 256);

    snprintf(providerKeyString, 256, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
        providerKey.Data1, providerKey.Data2, providerKey.Data3,
        providerKey.Data4[0], providerKey.Data4[1], providerKey.Data4[2], providerKey.Data4[3],
        providerKey.Data4[4], providerKey.Data4[5], providerKey.Data4[6], providerKey.Data4[7]);

    GUID subLayerKey;
    hCreateGuid = CoCreateGuid(&subLayerKey);

    char subLayerKeyString[256];
    memset(subLayerKeyString, 0, 256);

    snprintf(subLayerKeyString, 256, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
        subLayerKey.Data1, subLayerKey.Data2, subLayerKey.Data3,
        subLayerKey.Data4[0], subLayerKey.Data4[1], subLayerKey.Data4[2], subLayerKey.Data4[3],
        subLayerKey.Data4[4], subLayerKey.Data4[5], subLayerKey.Data4[6], subLayerKey.Data4[7]);

    std::ofstream log_file("C:/Users/state/logs/vpnfirewall.txt", std::ios::app);
    if (log_file.is_open()) {
        log_file << "providerKey: " << providerKeyString << std::endl;
        log_file << "subLayerKey: " << subLayerKeyString << std::endl;
    }*/

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VPNFIREWALL));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_VPNFIREWALL);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

LPWSTR ConvertToLPWSTR(const char* charArray) {
    int length = MultiByteToWideChar(CP_UTF8, 0, charArray, -1, nullptr, 0);
    LPWSTR wideCharArray = new wchar_t[length];
    MultiByteToWideChar(CP_UTF8, 0, charArray, -1, wideCharArray, length);
    return wideCharArray;
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    HWND hWndList = (HWND)lParamSort;

    LVITEM item1 = { 0 }, item2 = { 0 };
    wchar_t text1[256], text2[256];
    memset(text1, 0, 256);
    memset(text2, 0, 256);

    item1.mask = LVIF_TEXT;
    item1.iItem = lParam1; // item index is stored in lParam
    item1.iSubItem = 0;
    item1.pszText = text1;
    item1.cchTextMax = sizeof(text1);
    ListView_GetItem(hWndList, &item1);

    item2.mask = LVIF_TEXT;
    item2.iItem = lParam2;
    item2.iSubItem = 0;
    item2.pszText = text2;
    item2.cchTextMax = sizeof(text2);
    ListView_GetItem(hWndList, &item2);

    char* endptr; // Pointer to store the address of the first invalid character

    std::wstring wtext1(text1);
    std::wstring wtext2(text2);

    std::string stext1(wtext1.begin(), wtext1.end());
    std::string stext2(wtext2.begin(), wtext2.end());

    uint64_t result1 = std::strtoull(stext1.c_str(), &endptr, 10);
    uint64_t result2 = std::strtoull(stext2.c_str(), &endptr, 10);

    if (result1 == result2) {
        return 0;
    }
    else if (result1 > result2) {
        return 1;
    }
    else {
        return -1;
    }
}

BOOL RefreshFilterDisplay() {
    if (!hWnd)
    {
        return FALSE;
    }

    Provider provider;
    std::list<FilterInfo> filters = provider.GetFilters();    

    if (filters.size() == 1 && filters.begin()->isDummy) {
        provider.log_error("Could not load filters", ERROR_SUCCESS);

        if (uninstallButton) {
            EnableWindow(uninstallButton, false);
        }

        if (installButton) {
            EnableWindow(installButton, false);
        }

        return FALSE;
    }

    ListView_DeleteAllItems(hWndListView);

    LVITEM lv;
    lv.iSubItem = 0;
    lv.iItem = 0;
    lv.state = 0;
    lv.stateMask = 0;
    lv.iSubItem = 0;

    filters.sort([](const FilterInfo& a, const FilterInfo& b) { return a.effectiveWeight < b.effectiveWeight; });

    for (std::list<FilterInfo>::const_iterator filter = filters.begin(); filter != filters.end(); ++filter) {
        //LPWSTR weightPtr = ConvertToLPWSTR(filter->weight.c_str());
        LPWSTR actionPtr = ConvertToLPWSTR(filter->action.c_str());
        LPWSTR namePtr = ConvertToLPWSTR(filter->name.c_str());
        LPWSTR filterKeyPtr = ConvertToLPWSTR(filter->guid.c_str());

        LPWSTR remoteAddressPtr = ConvertToLPWSTR(filter->remote_address.c_str());
        LPWSTR remotePortPtr = ConvertToLPWSTR(filter->remote_port.c_str());
        LPWSTR localAddressPtr = ConvertToLPWSTR(filter->local_address.c_str());
        LPWSTR localPortPtr = ConvertToLPWSTR(filter->local_port.c_str());
        LPWSTR protocolPtr = ConvertToLPWSTR(filter->protocol.c_str());

        ListView_InsertItem(hWndListView, &lv);
        //ListView_SetItemText(hWndListView, 0, 0, weightPtr);
        ListView_SetItemText(hWndListView, 0, 0, actionPtr);
        ListView_SetItemText(hWndListView, 0, 1, namePtr);
        ListView_SetItemText(hWndListView, 0, 2, filterKeyPtr);

        ListView_SetItemText(hWndListView, 0, 3, protocolPtr);
        ListView_SetItemText(hWndListView, 0, 4, localAddressPtr);
        ListView_SetItemText(hWndListView, 0, 5, localPortPtr);
        ListView_SetItemText(hWndListView, 0, 6, remoteAddressPtr);
        ListView_SetItemText(hWndListView, 0, 7, remotePortPtr);        

        //delete[] weightPtr;
        delete[] actionPtr;
        delete[] namePtr;
        delete[] filterKeyPtr;

        delete[] remoteAddressPtr;
        delete[] remotePortPtr;
        delete[] localAddressPtr;
        delete[] localPortPtr;
        delete[] protocolPtr;
    }

    //ListView_SortItems(hWndListView, CompareFunc, hWndListView);

    if (filters.size() == 0) {
        if (installButton) {
            EnableWindow(installButton, true);
        }
    }
    else {
        if (uninstallButton) {
            EnableWindow(uninstallButton, true);
        }
    }

    return TRUE;
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int windowWidth = 832;
    int windowHeight = 468;

    hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        (screenWidth - windowWidth) / 2, (screenHeight - windowHeight) / 2, windowWidth, windowHeight, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    if ((UINT64)installButtonHandle < 1000 || (UINT64)uninstallButtonHandle < 1000) {
        Provider provider;
        provider.log_error("Could not create button handles", ERROR_SUCCESS);
        return FALSE;
    }

    RECT rcClient;                       // The parent window's client area.

    GetClientRect(hWnd, &rcClient);

    installButton = CreateWindow(
        L"BUTTON",
        L"Install",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        (rcClient.right - rcClient.left) / 2 - 130,         // x position 
        rcClient.bottom - rcClient.top - 60, // y position 
        120,        // Button width
        40,        // Button height
        hWnd,     // Parent window
        installButtonHandle,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);

    uninstallButton = CreateWindow(
        L"BUTTON",
        L"Uninstall",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        (rcClient.right - rcClient.left) / 2 + 10,         // x position 
        rcClient.bottom - rcClient.top - 60, // y position 
        120,        // Button width
        40,        // Button height
        hWnd,     // Parent window
        uninstallButtonHandle,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    hWndListView = CreateWindow(WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EX_GRIDLINES | LVS_EDITLABELS,
        0, 0,
        rcClient.right - rcClient.left,
        rcClient.bottom - rcClient.top - 80,
        hWnd,
        listViewHandle,
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);

    // LPWSTR str1 = const_cast<wchar_t*>(L"Weight");
    LPWSTR str2 = const_cast<wchar_t*>(L"Action");
    LPWSTR str3 = const_cast<wchar_t*>(L"Name");
    LPWSTR str4 = const_cast<wchar_t*>(L"Guid");

    LPWSTR str5 = const_cast<wchar_t*>(L"Protocol");
    LPWSTR str6 = const_cast<wchar_t*>(L"Local");
    LPWSTR str7 = const_cast<wchar_t*>(L"Port");
    LPWSTR str8 = const_cast<wchar_t*>(L"Remote");
    LPWSTR str9 = const_cast<wchar_t*>(L"Port");    

    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    
    /*lvc.iSubItem = 0;
    lvc.cx = 140;
    lvc.pszText = str1;
    ListView_InsertColumn(hWndListView, 0, &lvc);*/

    lvc.iSubItem = 0;
    lvc.cx = 50;
    lvc.pszText = str2;
    ListView_InsertColumn(hWndListView, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 165;
    lvc.pszText = str3;
    ListView_InsertColumn(hWndListView, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = 260;
    lvc.pszText = str4;
    ListView_InsertColumn(hWndListView, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.cx = 60;
    lvc.pszText = str5;
    ListView_InsertColumn(hWndListView, 3, &lvc);

    lvc.iSubItem = 4;
    lvc.cx = 100;
    lvc.pszText = str6;
    ListView_InsertColumn(hWndListView, 4, &lvc);

    lvc.iSubItem = 5;
    lvc.cx = 40;
    lvc.pszText = str7;
    ListView_InsertColumn(hWndListView, 5, &lvc);

    lvc.iSubItem = 6;
    lvc.cx = 100;
    lvc.pszText = str8;
    ListView_InsertColumn(hWndListView, 6, &lvc);

    lvc.iSubItem = 7;
    lvc.cx = 40;
    lvc.pszText = str9;
    ListView_InsertColumn(hWndListView, 7, &lvc);

    EnableWindow(installButton, false);
    EnableWindow(uninstallButton, false);

    RefreshFilterDisplay();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            if (LOWORD(wParam) == (WORD) installButtonHandle && HIWORD(wParam) == BN_CLICKED) {
                if (installButton) {
                    EnableWindow(installButton, false);
                }

                Provider provider;
                provider.log_error("install button clicked", ERROR_SUCCESS);

                DWORD result = provider.Install();

                RefreshFilterDisplay();
            } else if (LOWORD(wParam) == (WORD)uninstallButtonHandle && HIWORD(wParam) == BN_CLICKED) {
                if (uninstallButton) {
                    EnableWindow(uninstallButton, false);
                }

                Provider provider;
                provider.log_error("uninstall button clicked", ERROR_SUCCESS);

                DWORD result = provider.Uninstall();

                RefreshFilterDisplay();
            } else {
                // Parse the menu selections:
                switch (wmId)
                {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
                }
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

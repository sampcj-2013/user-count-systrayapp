#include "resource.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <commctrl.h>
#include <jansson.h>
#include <curl/curl.h>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */

#define URL_FORMAT   "http://api.justin.tv/api/stream/summary.json?channel="
#define URL_SIZE     256

#define ID_TIMER    1

/*variables*/
UINT WM_TASKBAR = 0;
HWND Hwnd;
HWND hEdit;
HWND hButton;
HWND hStatus;
HWND hwndCB; 
HMENU Hmenu;
HINSTANCE g_hInst;
NOTIFYICONDATA notifyIconData;
WNDPROC wpOrigEditProc;

TCHAR szTIP[64] = TEXT("Initialising...");
char szClassName[ ] = "Viewer Count SysTray App";
char blah[4] ="INT";
char conv[256];
char ptext[256];
char *text;
char url[URL_SIZE];

size_t i;

json_t *root;
json_error_t error;

int secondsElapsed = 1;
int num = 0;
int count = 1;
int nCopy;

/*procedures  */
LRESULT APIENTRY EditSubclassProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
void minimize();
void restore();
void InitNotifyIconData();

HICON CreateSmallIcon(HWND, char[4]);

inline int UpdateViewerCount();
inline void humanise(char *);

void safefree(void **pp);
char* itoa(int, char*, int );

typedef struct write_result
{
    char *data;
    int pos;
} write_result;

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static char *request(const char *url)
{
    CURL *curl;
    CURLcode status;
    char *data;
    long code;

    curl = curl_easy_init();
    data = (char *)malloc(BUFFER_SIZE);
    if(!curl || !data)
        return NULL;

    /*
    struct write_result write_result = {
        .data = data,
        .pos = 0
    };
    */
    
    write_result write_result = {
        data: data,
        pos: 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}

/* Return the offset of the first newline in text or the length of
   text if there's no newline */
static int newline_offset(const char *text)
{
    const char *newline = strchr(text, '\n');
    if(!newline)
        return strlen(text);
    else
        return (int)(newline - text);
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nCmdShow)
{
    nCopy = nCmdShow;
    setlocale(LC_ALL, "");
    snprintf(url, URL_SIZE, "%stestuser", URL_FORMAT);
    
    text = request(url);
    if(!text)
        return 1;
    
    root = json_loads(text, 0, &error);
    free(text);
    
    if(!root)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }
    
    
    INITCOMMONCONTROLSEX icc;

    // Initialise common controls.
    icc.dwSize = sizeof(icc);
    icc.dwICC =  ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
    
    /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    WM_TASKBAR = RegisterWindowMessageA("TaskbarCreated");
    /* The Window structure */
    g_hInst = hThisInstance;
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;                 /* Catch double-clicks */
    wincl.cbSize = sizeof (WNDCLASSEX);
    
    /* Use default icon and mouse-pointer */

	wincl.hIcon = CreateSmallIcon(Hwnd, blah);
	wincl.hIconSm = CreateSmallIcon(Hwnd, blah);
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    wincl.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(0, 255,128)));
    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx (&wincl))
        return 0;

    /* The class is registered, let's create the program*/
    Hwnd = CreateWindowEx (
               0,                   /* Extended possibilites for variation */
               szClassName,         /* Classname */
               szClassName,       /* Title Text */
               WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, /* default window */
               CW_USEDEFAULT,       /* Windows decides the position */
               CW_USEDEFAULT,       /* where the window ends up on the screen */
               300,                 /* The programs width */
               90,                 /* and height in pixels */
               // HWND_DESKTOP,        /* The window is a child-window to desktop */
    
               NULL,
               NULL,                /* No menu */
               hThisInstance,       /* Program Instance handler */
               NULL                 /* No Window Creation data */
           );
    while (!SetTimer (Hwnd, ID_TIMER,  30000, NULL))
         if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
            return FALSE ;
    while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)) {
        if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
            return FALSE ;
    }
    UpdateWindow (Hwnd);
    /*Initialize the NOTIFYICONDATA structure only once*/
    InitNotifyIconData();
    UpdateViewerCount();   
    /* Make the window visible on the screen */
    ShowWindow (Hwnd, nCmdShow);
    // ShowWindow (Hwnd, SW_HIDE);
    

    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (GetMessage (&messages, NULL, 0, 0))
    {
        /* Translate virtual-key messages into character messages */
        TranslateMessage(&messages);
        /* Send message to WindowProcedure */
        DispatchMessage(&messages);
    }
    
    return messages.wParam;
}

inline void humanise(char * input) {
    int num, val;
    num =  val =  atoi(input);
    if (val >= 1000) {        
        int count = 0;
        do {
            val = val / 10;
            count ++;
        } while (val);
        double dbl = num;
        if (count > 7 && count < 9) { 
            dbl /= pow(10, 7);
            snprintf(input, sizeof(input), "%.0fM", dbl);
        }
        if (count == 6) {
            dbl /= pow(10, 3);
            snprintf(input, sizeof(input), "%.0fK", dbl);
        }
        if (count == 5) {
            dbl /= pow(10, 3);
            snprintf(input, sizeof(input), "%.0fK", dbl);
        }
        if (count == 4) { 
            dbl /= pow(10, 3);
            snprintf(input, sizeof(input), "+%1.0fK", floor(dbl));
        }
    }
    else if (strlen(input) == 1)
        snprintf(input, sizeof(input), "%s           ", input);
}


/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    if ( message==WM_TASKBAR && !IsWindowVisible( Hwnd ) )
    {
        minimize();
        return 0;
    }

    switch (message)                  /* handle the messages */
    {
    case WM_TIMER :   // Timer message comming !
    {
        switch (wParam)
        {
            case ID_TIMER:{
                int res = UpdateViewerCount();
                while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)) {
                    if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                        return FALSE ;
                }
                secondsElapsed = 1;
                return res;
            }
            case IDT_COUNTER: {
                char timeElapsedMessage[48];
                snprintf(timeElapsedMessage, 48, "Viewer count last updated %d seconds ago", secondsElapsed);
                SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)timeElapsedMessage);      
                secondsElapsed++;        
            }
        }
    }
    case WM_ACTIVATE:   
    {
        Shell_NotifyIcon(NIM_ADD, &notifyIconData);
        break;
    }
    case WM_CREATE:
    {
        hButton = CreateWindow(TEXT("button"), TEXT("Refresh"),    
		    WS_VISIBLE | WS_CHILD  | BS_PUSHBUTTON,
		    5, 5, 80, 25,        
		    hwnd, (HMENU) 1, NULL, NULL); 
        hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
            hwnd, (HMENU)IDC_MAIN_STATUS, GetModuleHandle(NULL), NULL);  
        int statwidths[] = {250, -1};
        SendMessage(hStatus, SB_SETPARTS, sizeof(statwidths)/sizeof(int), (LPARAM)statwidths);
        SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)"Initialising");        
        hEdit = CreateWindow(TEXT("Edit"), NULL, WS_CHILD | WS_VISIBLE | WS_BORDER,
				90, 7, 200, 22, hwnd, (HMENU) IDC_MAIN_EDIT,
				NULL, NULL);
        HGDIOBJ hfDefault=GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(hEdit,
				WM_SETFONT,
				(WPARAM)hfDefault,
				MAKELPARAM(FALSE,0));
        SendMessage(hButton,
				WM_SETFONT,
				(WPARAM)hfDefault,
				MAKELPARAM(FALSE,0));
        SendMessage(hEdit,WM_SETTEXT,NULL,(LPARAM)"testuser");
        Hmenu = CreatePopupMenu();
        AppendMenu(Hmenu, MF_STRING, ID_TRAY_REFRESH,  TEXT( "Refresh" ) );
        AppendMenu(Hmenu, MF_STRING, ID_TRAY_EXIT,  TEXT( "Close" ) );  
        wpOrigEditProc = (WNDPROC) SetWindowLong(hEdit , GWL_WNDPROC, (LONG) EditSubclassProc); 
        ShowWindow(hEdit, nCopy);
        UpdateWindow(hEdit);
        break;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == 1) {
            // Reset timer seeming as we just refreshed; no need to spam requests unnecessarily
            while (!SetTimer (Hwnd, ID_TIMER,  30000, NULL)) {
                if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                    return FALSE ;
            } 
            // Use this timer to keep track of the time passed since the timer was reset
            while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)) {
                if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                    return FALSE ;
            }
            secondsElapsed = 1;
            UpdateViewerCount();
	   }
       break;
    }
    case WM_SYSCOMMAND:
    {
        /*In WM_SYSCOMMAND messages, the four low-order bits of the wParam parameter 
		are used internally by the system. To obtain the correct result when testing the value of wParam, 
		an application must combine the value 0xFFF0 with the wParam value by using the bitwise AND operator.*/ 
		
		switch( wParam & 0xFFF0 )  
        {
        case SC_MINIMIZE:
        case SC_CLOSE:  
            minimize() ;
            return 0 ;
            break;
        }
        break;
    }

        
        // Our user defined WM_SYSICON message.
    case WM_SYSICON:
    {

        switch(wParam)
        {
        case ID_TRAY_APP_ICON:
        SetForegroundWindow(Hwnd);
            break;
        }


        if (lParam == WM_LBUTTONUP)
        {
            while (!SetTimer (Hwnd, ID_TIMER,  30000, NULL)) {
                if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                    return FALSE ;
            }
            // Use this timer to keep track of the time passed since the timer was last reset
            while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)) {
                if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                    return FALSE ;
            }
            secondsElapsed = 1;
            UpdateViewerCount();
            restore();
        }
        else if (lParam == WM_RBUTTONDOWN) 
        {
            // Get current mouse position.
            POINT curPoint ;
            GetCursorPos( &curPoint ) ;
			SetForegroundWindow(Hwnd);

            // TrackPopupMenu blocks the app until TrackPopupMenu returns

            UINT clicked = TrackPopupMenu(Hmenu,TPM_RETURNCMD | TPM_NONOTIFY,curPoint.x,curPoint.y,0,hwnd,NULL);
            
            SendMessage(hwnd, WM_NULL, 0, 0); // send benign message to window to make sure the menu goes away.
            if (clicked == ID_TRAY_EXIT)
            {
                // quit the application.
                Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
                PostQuitMessage( 0 ) ;
            }
            if (clicked == ID_TRAY_REFRESH )
            {
                while (!SetTimer (Hwnd, ID_TIMER,  30000, NULL)){
                    if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                        return FALSE ;
                }
                // Use this timer to keep track of the time passed since the timer was reset
                while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)){
                    if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                        return FALSE ;
                }
                secondsElapsed = 1;
                UpdateViewerCount();  
            }
                
        }
        break;
    }
    

    // intercept the hittest message..
    case WM_NCHITTEST:
    {
       UINT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
        if(uHitTest == HTCLIENT)
            return HTCAPTION;
        else
            return uHitTest;
    }

    case WM_CLOSE:
    {
        minimize();
        return 0;
        break;
    }
    case WM_DESTROY:
    {
        json_decref(root);
        KillTimer (hwnd, ID_TIMER) ;
        PostQuitMessage (0);
        break;
    }
    }

    return DefWindowProc( hwnd, message, wParam, lParam ) ;
}

void minimize()
{
    // hide the main window
    ShowWindow(Hwnd, SW_HIDE);
}


void restore()
{
    ShowWindow(Hwnd, SW_SHOW);
}

void InitNotifyIconData()
{
    memset( &notifyIconData, 0, sizeof( NOTIFYICONDATA ) ) ;

    notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    notifyIconData.hWnd = Hwnd;
    notifyIconData.uID = ID_TRAY_APP_ICON;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyIconData.uCallbackMessage = WM_SYSICON; //Set up our invented Windows Message
	notifyIconData.hIcon = CreateSmallIcon(Hwnd, blah);
    strncpy(notifyIconData.szTip, szTIP, sizeof(szTIP));
}

inline int UpdateViewerCount()
{
    // char *conv = (char *) malloc(256*sizeof(char));
    // char *ptext = (char *) malloc(256*sizeof(char));
    // char conv[256];
    // char ptext[256];
    SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)"Fetching viewer count - Please wait a moment");
    LPWSTR buffer[256];
    SendMessage(hEdit,
		WM_GETTEXT,
		sizeof(buffer)/sizeof(buffer[0]),
		reinterpret_cast<LPARAM>(buffer));
    snprintf(url, URL_SIZE, "%s%s", URL_FORMAT, buffer);    
    text = request(url);
        
    if(!text)
        return 1;
        
    root = json_loads(text, 0, &error);
    free(text);

    if(!root)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }        
    json_t *test;
    test = json_object_get(root, "viewers_count");
        
    
    if (json_object_size(root)==0 || json_integer_value(test) == 0) {
        snprintf(ptext, 256, "Offline\n");
        snprintf(conv, 256, "OFF", conv);
    }
    else {
        // Parse the json object, retrieve the value pertaining to viewer count...
        snprintf(ptext, 256, "%d", json_integer_value(test));
        strncpy(conv, ptext, 256);
        /*
        char c1[32], c2[32];
        snprintf(c1, 32, "1: %d\n2: %d\nLENGTH: %d\nSIZE: %d", conv[0], conv[1], strlen(conv), sizeof(conv)/sizeof(int));
        MessageBox(Hwnd, c1, "Before conversion", MB_OK);
        snprintf(c1, 32, "SIZE OF STRING:%d\nCONTENTS:%s", sizeof(json_string_value(test))/sizeof(int), json_string_value(test));
        snprintf(c1, 32, "1: %d\n2: %d\nLENGTH: %d\nSIZE: %d", json_string_value(test)[0], json_string_value(test)[1], strlen(json_string_value(test)), sizeof(conv)/sizeof(int));
        MessageBox(Hwnd, c1, "Before conversion", MB_OK);
        */
        // Add thousands designation
        snprintf(ptext, 256, "Number of viewers: %d\n", json_integer_value(test));
        
        // make it so that the resulting representation never has more than three characters
        humanise(conv);
    }
    // update all icons and tool tips as necessary
    SendMessage(Hwnd, WM_SETICON, ICON_SMALL, (LPARAM)CreateSmallIcon(Hwnd,conv));
        
    notifyIconData.hIcon = CreateSmallIcon(Hwnd, conv);
    memset( &notifyIconData.szTip, 0, 64) ;
    strncpy(notifyIconData.szTip, ptext, 64);
    Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
    // free pointers safely
    // safefree((void **)&ptext);
    // safefree((void **   )&conv);
        
    // remove this reference
    SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)"Viewer count updated");  
    json_decref(root);
    return 0;
}

LRESULT APIENTRY EditSubclassProc(
                                  HWND hwnd, 
                                  UINT uMsg, 
                                  WPARAM wParam, 
                                  LPARAM lParam) 
{ 
    switch(uMsg)
	{
        /*
		case VK_RETURN:
            MessageBox(Hwnd, "Hello", "Before conversion", MB_OK);
			break;
        */
		case WM_KEYDOWN: 
			if (wParam == VK_RETURN) {
				while (!SetTimer (Hwnd, ID_TIMER,  30000, NULL)) {
                if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                    return FALSE ;
                }
                // Use this timer to keep track of the time passed since the timer was reset
                while (!SetTimer (Hwnd, IDT_COUNTER,  1000, NULL)) {
                    if (IDCANCEL == MessageBox (Hwnd, "Too many clocks or timers!", "Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL))
                        return FALSE ;
                }
                secondsElapsed = 1;
                UpdateViewerCount();
            }
			break;
	}
   // Pass all messages on to the original wndproc. Obviously, the WM_CHAR one has been tampered with :-)
   return CallWindowProc(wpOrigEditProc, hwnd, uMsg, wParam, lParam); 
} 


HICON CreateSmallIcon( HWND hWnd, TCHAR tText[4])
{
    // static TCHAR *szText = TEXT (tText );
    // TCHAR szText[64] = TEXT("100");
    TCHAR   szText[4];
    //*szText = TEXT(*tText);
    //MessageBox(Hwnd, szText, szText, MB_OK);
    HDC hdc, hdcMem;
    HBITMAP hBitmap = NULL;
    HBITMAP hOldBitMap = NULL;
    HBITMAP hBitmapMask = NULL;
    ICONINFO iconInfo;
    HFONT hFont;
    HICON hIcon;

    hdc = GetDC ( hWnd );
    hdcMem = CreateCompatibleDC ( hdc );
    hBitmap = CreateCompatibleBitmap ( hdc, 32, 32);
    hBitmapMask = CreateCompatibleBitmap ( hdc,32, 32);
    ReleaseDC ( hWnd, hdc );
    hOldBitMap = (HBITMAP) SelectObject ( hdcMem, hBitmap );

    // Draw percentage
    hFont = CreateFont (32, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    TEXT ("Arial"));
    hFont = (HFONT) SelectObject ( hdcMem, hFont );
    TextOut ( hdcMem, 0, 0, tText, lstrlen (tText) );

    SelectObject ( hdc, hOldBitMap );
    hOldBitMap = NULL;

    iconInfo.fIcon = TRUE;
    iconInfo.xHotspot = 0;
    iconInfo.yHotspot = 0;
    iconInfo.hbmMask = hBitmapMask;
    iconInfo.hbmColor = hBitmap;

    hIcon = CreateIconIndirect ( &iconInfo );

    DeleteObject ( SelectObject ( hdcMem, hFont ) );
    DeleteDC ( hdcMem );
    DeleteDC ( hdc );
    DeleteObject ( hBitmap );
    DeleteObject ( hBitmapMask );

    return hIcon;
}



/* Alternative version for 'free()' */
void safefree(void **pp)
{
    if (pp != NULL && *pp != NULL) { /* safety check */
        free(*pp);                  /* deallocate chunk */
        *pp = NULL;                 /* reset original pointer */
    }
}


/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
char* itoa(int value, char *result, int base) {
	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }

	char* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while ( value );

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

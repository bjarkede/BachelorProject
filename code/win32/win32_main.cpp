/*
Copyright (c) 2021-2022 Bjarke Damsgaard Eriksen. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    1. Redistributions of source code must retain the above
       copyright notice, this list of conditions and the
       following disclaimer.

    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials
       provided with the distribution.

    3. Neither the name of the copyright holder nor the names of
       its contributors may be used to endorse or promote products
       derived from this software without specific prior written
       permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define WIN32_LEAN_AND_MEAN
#include "../common/shared.h"
#include <Windows.h>
#include <hidusage.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"

#include "../renderer/r_public.h"
#include "../scene/s_public.h"

#define WINDOWED_STYLE (WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

// not defined with VS 2013 + Windows 7
#ifndef RI_MOUSE_HWHEEL
#define RI_MOUSE_HWHEEL 0x0800
#endif

bool running = true;

VideoConfig r_videoConfig;
long perfCountFreq;

struct Local
{
    HWND windowHandle;
    RECT windowRect;
    HMONITOR currentMonitor;
    RECT monitorRect;
};

static Local local;

static BOOL EnumMonitorCallback(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM)
{
    if (local.currentMonitor == hMonitor)
    {
        local.monitorRect = *rect;
        return FALSE;
    }
    return TRUE;
}

static void ToggleFullScreen()
{
    ShowWindow(local.windowHandle, SW_HIDE);

    LONG style = GetWindowLong(local.windowHandle, GWL_STYLE);
    bool isFullscreen = !!(style & WS_POPUP);
    if (!isFullscreen)
    {
        local.currentMonitor = MonitorFromWindow(local.windowHandle, MONITOR_DEFAULTTONEAREST);
        EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback, NULL);
        GetWindowRect(local.windowHandle, &local.windowRect);

        SetWindowLong(local.windowHandle, GWL_STYLE, WS_POPUP);
        LONG width = local.monitorRect.right - local.monitorRect.left;
        LONG height = local.monitorRect.bottom - local.monitorRect.top;
        SetWindowPos(local.windowHandle, HWND_TOPMOST, local.monitorRect.left, local.monitorRect.top, width, height, SWP_SHOWWINDOW);
    }
    else
    {
        SetWindowLong(local.windowHandle, GWL_STYLE, WINDOWED_STYLE);
        LONG x = local.windowRect.left;
        LONG y = local.windowRect.top;
        LONG w = local.windowRect.right - local.windowRect.left;
        LONG h = local.windowRect.bottom - local.windowRect.top;
        SetWindowPos(local.windowHandle, HWND_NOTOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    }

    ShowWindow(local.windowHandle, SW_SHOW);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WAL_MainWindowCallback(HWND Window, UINT Message,
    WPARAM WParam, LPARAM LParam)
{
    LRESULT result = 0;

    if (ImGui_ImplWin32_WndProcHandler(Window, Message, WParam, LParam))
        return true;

    switch (Message)
    {
    case WM_CLOSE:
    {
        running = false;
    }
    break;
    case WM_SIZE:
    {
        if (WParam == SIZE_RESTORED)
        {
            UINT width = LOWORD(LParam);
            UINT height = HIWORD(LParam);
            r_videoConfig.width = width;
            r_videoConfig.height = height;
            R_WindowSizeChanged();
        }
    }
    break;
    default:
    {
        result = DefWindowProc(Window, Message, WParam, LParam);
    }
    break;
    }

    return result;
}

u64 Sys_GetTimestamp()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result.QuadPart;
}

u64 Sys_GetElapsedMilliseconds(u64 startTimestamp)
{
    u64 endTimestamp = Sys_GetTimestamp();
    u64 cyclesElapsed = endTimestamp - startTimestamp;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return ((cyclesElapsed * 1000) / frequency.QuadPart);
}

u64 Sys_GetElapsedMicroseconds(u64 startTimestamp)
{
    u64 endTimestamp = Sys_GetTimestamp();
    u64 cyclesElapsed = endTimestamp - startTimestamp;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return ((cyclesElapsed * 1000000) / frequency.QuadPart);
}

void Sys_Quit()
{
    running = false;
}

static void ProcessInputMessage(InputState* NewState, u32 isDown)
{
    //assert(new_state->ended_down != is_down);
    NewState->isDown = isDown;
}

static void ProcessMouseButton(unsigned short btnFlags,
    unsigned short flagUp, unsigned short flagDown,
    InputState* NewState)
{
    u32 isDown = 0;
    u32 wasDown = 0;

    if (btnFlags & flagUp)
        wasDown = 1;
    if ((btnFlags & flagDown) && wasDown != 1)
        isDown = 1;

    if (wasDown != isDown)
    {
        ProcessInputMessage(NewState, isDown);
    }
}

static void WAL_ProcessPendingMessages(RawMouse* mouseInput, RawKeyboard* keyboardInput)
{
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
        case WM_QUIT:
        {
            running = false;
        }
        break;
        case WM_INPUT:
        {
            UINT apiRawInputNumBytes;
            if (GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, NULL, &apiRawInputNumBytes, sizeof(RAWINPUTHEADER)) != 0)
            {
                Sys_FatalError("GetRawInputData struct size request failed");
            }

            // the size doesn't always match perfectly, we can sometimes get a little less
            if (apiRawInputNumBytes > sizeof(RAWINPUT))
            {
                Sys_FatalError("GetRawInputData struct size too large (reserved %d, got %d)", (int)sizeof(RAWINPUT), (int)apiRawInputNumBytes);
            }

            UINT expectedNumBytesCopied = apiRawInputNumBytes;
            RAWINPUT rawInput;
            if (GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, &rawInput, &apiRawInputNumBytes, sizeof(RAWINPUTHEADER)) != expectedNumBytesCopied)
            {
                Sys_FatalError("GetRawInputData failed (expected %d, got %d)", (int)expectedNumBytesCopied, (int)apiRawInputNumBytes);
            }
            RAWINPUT* raw = &rawInput;

            if (raw->header.dwType == RIM_TYPEMOUSE)
            {
                unsigned short flags = raw->data.mouse.usFlags;
                unsigned short btnFlags = raw->data.mouse.usButtonFlags;
                unsigned short btnData = raw->data.mouse.usButtonData;
                u32 btn = raw->data.mouse.ulButtons;

                if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                {
                    mouseInput->dx += raw->data.mouse.lLastX;
                    mouseInput->dy += raw->data.mouse.lLastY;
                }

                // Handle scroll wheel
                if ((btnFlags & RI_MOUSE_WHEEL) || (btnFlags & RI_MOUSE_HWHEEL))
                {
                    f32 wheel_dt = (f32)(short)btnData;
                    f32 numTick = wheel_dt / WHEEL_DELTA;
                    mouseInput->wheel_dt += numTick;
                }

                ProcessMouseButton(btnFlags, RI_MOUSE_LEFT_BUTTON_UP, RI_MOUSE_LEFT_BUTTON_DOWN, &mouseInput->Mouse1);
                ProcessMouseButton(btnFlags, RI_MOUSE_RIGHT_BUTTON_UP, RI_MOUSE_RIGHT_BUTTON_DOWN, &mouseInput->Mouse2);
                ProcessMouseButton(btnFlags, RI_MOUSE_MIDDLE_BUTTON_UP, RI_MOUSE_MIDDLE_BUTTON_DOWN, &mouseInput->Mouse3);
            }

            if (raw->header.dwType == RIM_TYPEKEYBOARD)
            {
                u32 code = raw->data.keyboard.VKey;
                u32 flags = raw->data.keyboard.Flags;
                u32 isDown = !(flags & 1);

                // @TODO:
                // Should toggle full screen be called from client?
                if (code < ARRAY_LEN(keyboardInput->wasDown))
                {
                    if (code == 'F' && isDown && !keyboardInput->wasDown[code])
                    {
                        ToggleFullScreen();
                    }
                }

                if (code == VK_ESCAPE)
                    running = false;
                if (code < ARRAY_LEN(keyboardInput->wasDown))
                {
                    keyboardInput->isDown[code] = isDown;
                }
            }
        }
        break;
        default:
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        break;
        }
    }
}

static void AllocateMemoryArenas(SystemMemory* memory, MemoryPools* state)
{
    AllocateArena(&state->persistent, (size_t)memory->permanentStorageSize, (u8*)memory->permanentStorage, "persistent arena");
    AllocateArena(&state->transient, (size_t)memory->transientStorageSize, (u8*)memory->transientStorage, "transient arena");
}

int CALLBACK WinMain(HINSTANCE Instance,
    HINSTANCE prevInstance,
    LPSTR CommandLine,
    int showCode)
{

    HINSTANCE hInstance = GetModuleHandle(NULL);
    LARGE_INTEGER lpFreq;
    QueryPerformanceFrequency(&lpFreq);
    perfCountFreq = lpFreq.QuadPart;

    const HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
    WNDCLASSEX windowClass = { sizeof(WNDCLASSEX), CS_CLASSDC, &WAL_MainWindowCallback, 0L, 0L, hInstance, NULL, NULL, bgBrush, NULL, "Render Tool", NULL };

    int refreshRate = 250;
    int updateRate = refreshRate;
    float millisecondsElapsedTarget = 1000.0f / (float)updateRate;

    if (RegisterClassEx(&windowClass))
    {
        RECT windowSize;
        windowSize.left = 0;
        windowSize.top = 0;
        windowSize.right = SCREEN_WIDTH;
        windowSize.bottom = SCREEN_HEIGHT;

        DWORD style = WINDOWED_STYLE;

        AdjustWindowRect(&windowSize, style, FALSE);

        s32 height = windowSize.bottom - windowSize.top;
        s32 width = windowSize.right - windowSize.left;

        HWND windowHandle = CreateWindow(windowClass.lpszClassName, "Renderer", style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, windowClass.hInstance, NULL);
        local.windowHandle = windowHandle;

        if (windowHandle)
        {
            HDC deviceContext = GetDC(windowHandle);

            // Setup the window and show it.
            ShowWindow(windowHandle, SW_SHOW);
            SetForegroundWindow(windowHandle);
            SetFocus(windowHandle);
            ShowCursor(SW_SHOW);

            r_videoConfig.height = SCREEN_HEIGHT;
            r_videoConfig.width = SCREEN_WIDTH;

            // Get memory from the system.
            SystemMemory systemMemory = {};
            systemMemory.permanentStorageSize = Megabytes(64);
            systemMemory.permanentStorage = VirtualAlloc(0, systemMemory.permanentStorageSize,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            systemMemory.transientStorageSize = Megabytes(64);
            systemMemory.transientStorage = VirtualAlloc(0, systemMemory.transientStorageSize,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            MemoryPools memory;
            AllocateMemoryArenas(&systemMemory, &memory);
            S_Init(&memory);

            // Setup the imgui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();

            // Setup DX11 graphcis
            R_Init(windowHandle, &memory);
            ImGui_ImplWin32_Init(windowHandle);

            // Client input
            ClientInput input[2] = {};
            ClientInput* newInput = &input[0];
            ClientInput* oldInput = &input[1];

            RAWINPUTDEVICE rawInputDevices[2] = {};
            rawInputDevices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
            rawInputDevices[0].usUsage = HID_USAGE_GENERIC_MOUSE;
            rawInputDevices[0].dwFlags = 0;
            rawInputDevices[0].hwndTarget = windowHandle;

            rawInputDevices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
            rawInputDevices[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
            rawInputDevices[1].dwFlags = 0;
            rawInputDevices[1].hwndTarget = windowHandle;

            if (RegisterRawInputDevices(rawInputDevices, 2, sizeof(rawInputDevices[0])) == FALSE)
            {
                Sys_FatalError("RegisterRawInputDevices failed! Last error: 0x%08X", (unsigned int)GetLastError());
            }

            ImVec4 clearColor = ImVec4(0.0f, 0.5f, 0.5f, 1.00f);

            bool show_demo_window = true;
            bool show_status = true;

            // Frame-timing
            float msPerFrame = 0.0f;
            u64 startTimestamp = Sys_GetTimestamp();

            while (running)
            {
                RawMouse* oldMouseInput = &oldInput->mouse; // This is the input from the last frame.
                RawMouse* newMouseInput = &newInput->mouse; // This is the input we are going to get this frame.
                RawKeyboard* oldKeyboardInput = &oldInput->keyboard;
                RawKeyboard* newKeyboardInput = &newInput->keyboard;

                newInput->dt = msPerFrame;

                *newMouseInput = {}; // Reset

                for (int BtnIndex = 0; BtnIndex < ARRAY_LEN(newMouseInput->Btn); ++BtnIndex)
                    newMouseInput->Btn[BtnIndex].isDown = oldMouseInput->Btn[BtnIndex].isDown;

                memcpy(newKeyboardInput, oldKeyboardInput, sizeof(RawKeyboard));
                memcpy(newKeyboardInput->wasDown, oldKeyboardInput->isDown, sizeof(oldKeyboardInput->isDown));

                WAL_ProcessPendingMessages(newMouseInput, newKeyboardInput);

                ImGui_ImplWin32_NewFrame();
                R_DrawFrame(newInput);

                // Timing
                u64 usElapsed = Sys_GetElapsedMicroseconds(startTimestamp);
                startTimestamp = Sys_GetTimestamp();
                msPerFrame = usElapsed / 1000.0f;

                // Update for next frame
                ClientInput* temp = newInput;
                newInput = oldInput;
                oldInput = temp;
            }

            R_ShutDown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            rawInputDevices[0].dwFlags = RIDEV_REMOVE;
            rawInputDevices[0].hwndTarget = NULL;
            rawInputDevices[1].dwFlags = RIDEV_REMOVE;
            rawInputDevices[1].hwndTarget = NULL;
            RegisterRawInputDevices(rawInputDevices, 2, sizeof(rawInputDevices[0]));

            DestroyWindow(windowHandle);
            UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        }
    }

    return WM_QUIT;
}

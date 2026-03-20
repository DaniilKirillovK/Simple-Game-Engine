#pragma once
#include <GLFW/glfw3.h>

namespace OpenGLKeyCode 
{
    constexpr int A = GLFW_KEY_A;
    constexpr int B = GLFW_KEY_B;
    constexpr int C = GLFW_KEY_C;
    constexpr int D = GLFW_KEY_D;
    constexpr int E = GLFW_KEY_E;
    constexpr int F = GLFW_KEY_F;
    constexpr int G = GLFW_KEY_G;
    constexpr int H = GLFW_KEY_H;
    constexpr int I = GLFW_KEY_I;
    constexpr int J = GLFW_KEY_J;
    constexpr int K = GLFW_KEY_K;
    constexpr int L = GLFW_KEY_L;
    constexpr int M = GLFW_KEY_M;
    constexpr int N = GLFW_KEY_N;
    constexpr int O = GLFW_KEY_O;
    constexpr int P = GLFW_KEY_P;
    constexpr int Q = GLFW_KEY_Q;
    constexpr int R = GLFW_KEY_R;
    constexpr int S = GLFW_KEY_S;
    constexpr int T = GLFW_KEY_T;
    constexpr int U = GLFW_KEY_U;
    constexpr int V = GLFW_KEY_V;
    constexpr int W = GLFW_KEY_W;
    constexpr int X = GLFW_KEY_X;
    constexpr int Y = GLFW_KEY_Y;
    constexpr int Z = GLFW_KEY_Z;

    constexpr int Num0 = GLFW_KEY_0;
    constexpr int Num1 = GLFW_KEY_1;
    constexpr int Num2 = GLFW_KEY_2;
    constexpr int Num3 = GLFW_KEY_3;
    constexpr int Num4 = GLFW_KEY_4;
    constexpr int Num5 = GLFW_KEY_5;
    constexpr int Num6 = GLFW_KEY_6;
    constexpr int Num7 = GLFW_KEY_7;
    constexpr int Num8 = GLFW_KEY_8;
    constexpr int Num9 = GLFW_KEY_9;

    constexpr int F1 = GLFW_KEY_F1;
    constexpr int F2 = GLFW_KEY_F2;
    constexpr int F3 = GLFW_KEY_F3;
    constexpr int F4 = GLFW_KEY_F4;
    constexpr int F5 = GLFW_KEY_F5;
    constexpr int F6 = GLFW_KEY_F6;
    constexpr int F7 = GLFW_KEY_F7;
    constexpr int F8 = GLFW_KEY_F8;
    constexpr int F9 = GLFW_KEY_F9;
    constexpr int F10 = GLFW_KEY_F10;
    constexpr int F11 = GLFW_KEY_F11;
    constexpr int F12 = GLFW_KEY_F12;

    constexpr int Space = GLFW_KEY_SPACE;
    constexpr int Enter = GLFW_KEY_ENTER;
    constexpr int Escape = GLFW_KEY_ESCAPE;
    constexpr int Tab = GLFW_KEY_TAB;
    constexpr int Backspace = GLFW_KEY_BACKSPACE;
    constexpr int Insert = GLFW_KEY_INSERT;
    constexpr int Delete = GLFW_KEY_DELETE;
    constexpr int Home = GLFW_KEY_HOME;
    constexpr int End = GLFW_KEY_END;
    constexpr int PageUp = GLFW_KEY_PAGE_UP;
    constexpr int PageDown = GLFW_KEY_PAGE_DOWN;

    constexpr int Up = GLFW_KEY_UP;
    constexpr int Down = GLFW_KEY_DOWN;
    constexpr int Left = GLFW_KEY_LEFT;
    constexpr int Right = GLFW_KEY_RIGHT;

    constexpr int LeftShift = GLFW_KEY_LEFT_SHIFT;
    constexpr int RightShift = GLFW_KEY_RIGHT_SHIFT;
    constexpr int LeftCtrl = GLFW_KEY_LEFT_CONTROL;
    constexpr int RightCtrl = GLFW_KEY_RIGHT_CONTROL;
    constexpr int LeftAlt = GLFW_KEY_LEFT_ALT;
    constexpr int RightAlt = GLFW_KEY_RIGHT_ALT;
    constexpr int LeftSuper = GLFW_KEY_LEFT_SUPER;
    constexpr int RightSuper = GLFW_KEY_RIGHT_SUPER;

    constexpr int Apostrophe = GLFW_KEY_APOSTROPHE;
    constexpr int Comma = GLFW_KEY_COMMA;
    constexpr int Minus = GLFW_KEY_MINUS;
    constexpr int Period = GLFW_KEY_PERIOD;
    constexpr int Slash = GLFW_KEY_SLASH;
    constexpr int Semicolon = GLFW_KEY_SEMICOLON;
    constexpr int Equal = GLFW_KEY_EQUAL;
    constexpr int LeftBracket = GLFW_KEY_LEFT_BRACKET;
    constexpr int Backslash = GLFW_KEY_BACKSLASH;
    constexpr int RightBracket = GLFW_KEY_RIGHT_BRACKET;
    constexpr int GraveAccent = GLFW_KEY_GRAVE_ACCENT;

    constexpr int Kp0 = GLFW_KEY_KP_0;
    constexpr int Kp1 = GLFW_KEY_KP_1;
    constexpr int Kp2 = GLFW_KEY_KP_2;
    constexpr int Kp3 = GLFW_KEY_KP_3;
    constexpr int Kp4 = GLFW_KEY_KP_4;
    constexpr int Kp5 = GLFW_KEY_KP_5;
    constexpr int Kp6 = GLFW_KEY_KP_6;
    constexpr int Kp7 = GLFW_KEY_KP_7;
    constexpr int Kp8 = GLFW_KEY_KP_8;
    constexpr int Kp9 = GLFW_KEY_KP_9;
    constexpr int KpDecimal = GLFW_KEY_KP_DECIMAL;
    constexpr int KpDivide = GLFW_KEY_KP_DIVIDE;
    constexpr int KpMultiply = GLFW_KEY_KP_MULTIPLY;
    constexpr int KpSubtract = GLFW_KEY_KP_SUBTRACT;
    constexpr int KpAdd = GLFW_KEY_KP_ADD;
    constexpr int KpEnter = GLFW_KEY_KP_ENTER;
    constexpr int KpEqual = GLFW_KEY_KP_EQUAL;
}

namespace OpenGLMouseButton 
{
    constexpr int Left = GLFW_MOUSE_BUTTON_LEFT;
    constexpr int Right = GLFW_MOUSE_BUTTON_RIGHT;
    constexpr int Middle = GLFW_MOUSE_BUTTON_MIDDLE;
    constexpr int Button4 = GLFW_MOUSE_BUTTON_4;
    constexpr int Button5 = GLFW_MOUSE_BUTTON_5;
    constexpr int Button6 = GLFW_MOUSE_BUTTON_6;
    constexpr int Button7 = GLFW_MOUSE_BUTTON_7;
    constexpr int Button8 = GLFW_MOUSE_BUTTON_8;
}
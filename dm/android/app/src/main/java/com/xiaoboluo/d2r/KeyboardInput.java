// External Android keyboard keys -> USB HID usages, matching iPad UIKey.
// Gamepad sources stay owned by PadInput; IME dialog keeps text entry focus.
package com.xiaoboluo.d2r;

import android.view.InputDevice;
import android.view.KeyEvent;

final class KeyboardInput {
    private KeyboardInput() {}
    static boolean onKey(KeyEvent e) {
        int source=e.getSource();
        if((source & InputDevice.SOURCE_KEYBOARD)!=InputDevice.SOURCE_KEYBOARD ||
           (source & InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD) return false;
        InputDevice d=e.getDevice();
        if(d!=null && d.getKeyboardType()==InputDevice.KEYBOARD_TYPE_NONE) return false;
        int hid=hidUsage(e.getKeyCode());
        if(hid<0) return false;
        if(e.getAction()==KeyEvent.ACTION_DOWN && e.getRepeatCount()==0)
            DmActivity.nativeKeyboardEvent(hid,true);
        else if(e.getAction()==KeyEvent.ACTION_UP)
            DmActivity.nativeKeyboardEvent(hid,false);
        return true; // suppress Android UI navigation and key-repeat toggling
    }
    static int hidUsage(int code) {
        if(code>=KeyEvent.KEYCODE_A && code<=KeyEvent.KEYCODE_Z)
            return 0x04+code-KeyEvent.KEYCODE_A;
        if(code>=KeyEvent.KEYCODE_1 && code<=KeyEvent.KEYCODE_9)
            return 0x1e+code-KeyEvent.KEYCODE_1;
        if(code==KeyEvent.KEYCODE_0)return 0x27;
        if(code>=KeyEvent.KEYCODE_F1 && code<=KeyEvent.KEYCODE_F12)
            return 0x3a+code-KeyEvent.KEYCODE_F1;
        switch(code) {
            case KeyEvent.KEYCODE_ENTER: case KeyEvent.KEYCODE_NUMPAD_ENTER:return 0x28;
            case KeyEvent.KEYCODE_DEL:return 0x2a; // Backspace
            case KeyEvent.KEYCODE_FORWARD_DEL:return 0x4c;
            case KeyEvent.KEYCODE_MINUS:return 0x2d;
            case KeyEvent.KEYCODE_LEFT_BRACKET:return 0x2f;
            case KeyEvent.KEYCODE_RIGHT_BRACKET:return 0x30;
            case KeyEvent.KEYCODE_BACKSLASH:return 0x31;
            case KeyEvent.KEYCODE_SEMICOLON:return 0x33;
            case KeyEvent.KEYCODE_APOSTROPHE:return 0x34;
            case KeyEvent.KEYCODE_GRAVE:return 0x35;
            case KeyEvent.KEYCODE_COMMA:return 0x36;
            case KeyEvent.KEYCODE_PERIOD:return 0x37;
            case KeyEvent.KEYCODE_SLASH:return 0x38;
            case KeyEvent.KEYCODE_CAPS_LOCK:return 0x39;
            case KeyEvent.KEYCODE_SYSRQ:return 0x46; // Print Screen
            case KeyEvent.KEYCODE_SCROLL_LOCK:return 0x47;
            case KeyEvent.KEYCODE_BREAK:return 0x48; // Pause
            case KeyEvent.KEYCODE_INSERT:return 0x49;
            case KeyEvent.KEYCODE_MOVE_HOME:return 0x4a;
            case KeyEvent.KEYCODE_PAGE_UP:return 0x4b;
            case KeyEvent.KEYCODE_MOVE_END:return 0x4d;
            case KeyEvent.KEYCODE_PAGE_DOWN:return 0x4e;
            case KeyEvent.KEYCODE_CTRL_LEFT:return 0xe0;
            case KeyEvent.KEYCODE_CTRL_RIGHT:return 0xe4;
            case KeyEvent.KEYCODE_SHIFT_LEFT:return 0xe1;
            case KeyEvent.KEYCODE_SHIFT_RIGHT:return 0xe5;
            case KeyEvent.KEYCODE_ALT_LEFT:return 0xe2;
            case KeyEvent.KEYCODE_ALT_RIGHT:return 0xe6;
            case KeyEvent.KEYCODE_META_LEFT:return 0xe3;
            case KeyEvent.KEYCODE_META_RIGHT:return 0xe7;
            case KeyEvent.KEYCODE_NUMPAD_0:return 0x62;
            case KeyEvent.KEYCODE_NUMPAD_1:return 0x59;
            case KeyEvent.KEYCODE_NUMPAD_2:return 0x5a;
            case KeyEvent.KEYCODE_NUMPAD_3:return 0x5b;
            case KeyEvent.KEYCODE_NUMPAD_4:return 0x5c;
            case KeyEvent.KEYCODE_NUMPAD_5:return 0x5d;
            case KeyEvent.KEYCODE_NUMPAD_6:return 0x5e;
            case KeyEvent.KEYCODE_NUMPAD_7:return 0x5f;
            case KeyEvent.KEYCODE_NUMPAD_8:return 0x60;
            case KeyEvent.KEYCODE_NUMPAD_9:return 0x61;
            case KeyEvent.KEYCODE_ESCAPE:return 0x29;
            case KeyEvent.KEYCODE_TAB:return 0x2b;
            case KeyEvent.KEYCODE_SPACE:return 0x2c;
            case KeyEvent.KEYCODE_EQUALS:return 0x2e;
            case KeyEvent.KEYCODE_NUMPAD_ADD:return 0x57;
            case KeyEvent.KEYCODE_DPAD_RIGHT:return 0x4f;
            case KeyEvent.KEYCODE_DPAD_LEFT:return 0x50;
            case KeyEvent.KEYCODE_DPAD_DOWN:return 0x51;
            case KeyEvent.KEYCODE_DPAD_UP:return 0x52;
            default:return -1;
        }
    }
}

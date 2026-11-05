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
        if(code>=KeyEvent.KEYCODE_F1 && code<=KeyEvent.KEYCODE_F8)
            return 0x3a+code-KeyEvent.KEYCODE_F1;
        switch(code) {
            case KeyEvent.KEYCODE_ENTER: case KeyEvent.KEYCODE_NUMPAD_ENTER:return 0x28;
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

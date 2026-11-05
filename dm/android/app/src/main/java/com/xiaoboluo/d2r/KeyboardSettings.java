// Human-readable controls editor; backed by the same sw_keyboard.txt
// native serializer as iPad. The original gamepad KeyBindingMenu remains intact.
package com.xiaoboluo.d2r;

import android.app.Activity;
import android.app.AlertDialog;
import android.widget.Toast;
import java.util.Locale;

final class KeyboardSettings {
    private KeyboardSettings() {}
    private static final int[] KEYS = {
        0x1a,0x04,0x16,0x07, 0x0d,0x0e,0x18,0x0c,
        0x09,0x0a,0x14,0x08,0x1d,0x06,
        0x28,0x29,0x2b,0x2c,0x2e,
        0x50,0x52,0x4f,0x51,
        0x1e,0x1f,0x20,0x21,
        0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x10
    };
    private static final String[] KEY_NAMES = {
        "W","A","S","D", "J","K","U","I",
        "F","G","Q","E","Z","C",
        "Enter","Esc","Tab","Space","=",
        "←","↑","→","↓", "1","2","3","4",
        "F1","F2","F3","F4","F5","F6","F7","F8","M"
    };
    private static final int[] ACTIONS={
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        16,17,18,19,40,41,42,48,49,50,51,52,53,54,55,255
    };
    private static final String[] ACTION_NAMES={
        "A / 确定","B / 返回","X","Y","左摇杆按下","右摇杆按下",
        "L","R","ZL","ZR","菜单 (+)","次级菜单 (-)",
        "十字左 / 药水1","十字上 / 药水2","十字右 / 药水3","十字下 / 药水4",
        "行走上","行走左","行走下","行走右",
        "确认 (Enter)","返回/打开菜单 (Esc)","切换地图",
        "技能1 (F1)","技能2 (F2)","技能3 (F3)","技能4 (F4)",
        "技能5 (F5)","技能6 (F6)","技能7 (F7)","技能8 (F8)",
        "不绑定"
    };
    static void show(Activity activity) {
        final boolean zh=Locale.getDefault().getLanguage().equals("zh");
        String[] rows=new String[KEYS.length+1];
        for(int i=0;i<KEYS.length;i++) {
            int val=DmActivity.nativeKbAction(KEYS[i]), j=0;
            for(;j<ACTIONS.length-1 && ACTIONS[j]!=val;j++);
            rows[i]=KEY_NAMES[i]+"     →     "+ACTION_NAMES[j];
        }
        rows[KEYS.length]=zh?"恢复默认键位":"Reset all bindings";
        new AlertDialog.Builder(activity)
            .setTitle(zh?"控制 · 键鼠":"Controls · Keyboard")
            .setItems(rows,(d,which)->{
                if(which==KEYS.length) {
                    new AlertDialog.Builder(activity)
                        .setMessage(zh?"恢复全部键鼠默认键位？":"Reset all keyboard bindings?")
                        .setPositiveButton(zh?"恢复":"Reset",(w,n)->{
                            DmActivity.nativeKbDefaults();show(activity);
                        })
                        .setNegativeButton(zh?"取消":"Cancel",null).show();
                } else choose(activity,which);
            })
            .setNegativeButton(zh?"关闭":"Close",null)
            .show();
    }
    private static void choose(Activity activity,int idx) {
        boolean zh=Locale.getDefault().getLanguage().equals("zh");
        new AlertDialog.Builder(activity)
            .setTitle(KEY_NAMES[idx])
            .setItems(ACTION_NAMES,(d,which)->{
                if(DmActivity.nativeKbBind(KEYS[idx],ACTIONS[which]))show(activity);
                else Toast.makeText(activity,zh?"无法保存键位":"Cannot save binding",Toast.LENGTH_SHORT).show();
            })
            .setNegativeButton(zh?"取消":"Cancel",null).show();
    }
}

// Human-readable controls editor; backed by the same sw_keyboard.txt
// native serializer as iPad. The original gamepad KeyBindingMenu remains intact.
package com.xiaoboluo.d2r;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
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
    private static final String[] MOUSE_NAMES={"Mouse Left / 鼠标左键",
        "Mouse Right / 鼠标右键","Mouse Middle / 鼠标中键"};
    private static final String[] MOUSE_ACTIONS={"Primary click / 左键交互",
        "Secondary use / 右键使用","Toggle map / 切换地图"};
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
    static ScrollView createInline(Activity activity) {
        ScrollView scroll=new ScrollView(activity);
        scroll.setFillViewport(false);
        scroll.setBackgroundColor(Color.argb(242,12,12,15));
        scroll.setClickable(true);
        LinearLayout content=new LinearLayout(activity);
        content.setOrientation(LinearLayout.VERTICAL);
        int pad=(int)(12*activity.getResources().getDisplayMetrics().density);
        content.setPadding(pad,pad,pad,pad);
        scroll.addView(content,new ScrollView.LayoutParams(-1,-2));
        return scroll;
    }
    static void refreshInline(Activity activity,ScrollView scroll) {
        if(scroll==null || scroll.getChildCount()==0)return;
        LinearLayout content=(LinearLayout)scroll.getChildAt(0);
        content.removeAllViews();
        int pad=(int)(6*activity.getResources().getDisplayMetrics().density);
        for(int i=0;i<KEYS.length+4;i++) {
            final int index=i;
            String title;
            if(i<KEYS.length) {
                int val=DmActivity.nativeKbAction(KEYS[i]),j=0;
                for(;j<ACTIONS.length-1 && ACTIONS[j]!=val;j++);
                title=KEY_NAMES[i]+"     →     "+ACTION_NAMES[j];
            } else if(i<KEYS.length+3) {
                int physical=i-KEYS.length,role=DmActivity.nativeMouseBinding(physical);
                title=MOUSE_NAMES[physical]+"     →     "+MOUSE_ACTIONS[role>=0 && role<3?role:0];
            } else title=Locale.getDefault().getLanguage().equals("zh")?"恢复此角色默认键鼠设置":"Reset character's keyboard/mouse";
            Button row=new Button(activity);
            row.setText(title);
            row.setAllCaps(false);
            row.setTextColor(Color.WHITE);
            row.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.rgb(45,45,52)));
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);
            lp.bottomMargin=pad;
            content.addView(row,lp);
            row.setOnClickListener(view->{
                if(index<KEYS.length)chooseInline(activity,scroll,index);
                else if(index<KEYS.length+3)chooseMouseInline(activity,scroll,index-KEYS.length);
                else new AlertDialog.Builder(activity)
                    .setMessage(Locale.getDefault().getLanguage().equals("zh")?"恢复此角色的全部默认键鼠设置？":"Reset keyboard and mouse for this character?")
                    .setPositiveButton(Locale.getDefault().getLanguage().equals("zh")?"恢复":"Reset",(d,n)->{
                        DmActivity.nativeKbDefaults();refreshInline(activity,scroll);
                    })
                    .setNegativeButton(Locale.getDefault().getLanguage().equals("zh")?"取消":"Cancel",null).show();
            });
        }
    }
    private static void chooseInline(Activity activity,ScrollView scroll,int index) {
        new AlertDialog.Builder(activity).setTitle(KEY_NAMES[index]).setItems(ACTION_NAMES,(d,which)->{
            if(DmActivity.nativeKbBind(KEYS[index],ACTIONS[which]))refreshInline(activity,scroll);
            else Toast.makeText(activity,"Cannot save this character's key binding",Toast.LENGTH_LONG).show();
        }).setNegativeButton("Cancel",null).show();
    }
    private static void chooseMouseInline(Activity activity,ScrollView scroll,int physical) {
        new AlertDialog.Builder(activity).setTitle(MOUSE_NAMES[physical]).setItems(MOUSE_ACTIONS,(d,which)->{
            if(DmActivity.nativeMouseBind(physical,which))refreshInline(activity,scroll);
            else Toast.makeText(activity,"Cannot save this character's mouse binding",Toast.LENGTH_LONG).show();
        }).setNegativeButton("Cancel",null).show();
    }
    static void show(Activity activity) {
        final boolean zh=Locale.getDefault().getLanguage().equals("zh");
        String[] rows=new String[KEYS.length+4];
        for(int i=0;i<KEYS.length;i++) {
            int val=DmActivity.nativeKbAction(KEYS[i]), j=0;
            for(;j<ACTIONS.length-1 && ACTIONS[j]!=val;j++);
            rows[i]=KEY_NAMES[i]+"     →     "+ACTION_NAMES[j];
        }
        for(int i=0;i<3;i++) {
            int role=DmActivity.nativeMouseBinding(i);
            rows[KEYS.length+i]=MOUSE_NAMES[i]+"     →     "+MOUSE_ACTIONS[role>=0 && role<3?role:0];
        }
        rows[KEYS.length+3]=zh?"恢复默认键鼠设置":"Reset all bindings";
        new AlertDialog.Builder(activity)
            .setTitle(zh?"控制 · 键鼠":"Controls · Keyboard")
            .setItems(rows,(d,which)->{
                if(which==KEYS.length+3) {
                    new AlertDialog.Builder(activity)
                        .setMessage(zh?"恢复全部键鼠默认键位？":"Reset all keyboard bindings?")
                        .setPositiveButton(zh?"恢复":"Reset",(w,n)->{
                            DmActivity.nativeKbDefaults();show(activity);
                        })
                        .setNegativeButton(zh?"取消":"Cancel",null).show();
                } else if(which>=KEYS.length) chooseMouse(activity,which-KEYS.length);
                else choose(activity,which);
            })
            .setNegativeButton(zh?"关闭":"Close",null)
            .show();
    }
    private static void chooseMouse(Activity activity,int physical) {
        boolean zh=Locale.getDefault().getLanguage().equals("zh");
        new AlertDialog.Builder(activity)
            .setTitle(MOUSE_NAMES[physical])
            .setItems(MOUSE_ACTIONS,(d,which)->{
                if(DmActivity.nativeMouseBind(physical,which))show(activity);
                else Toast.makeText(activity,zh?"无法保存鼠标功能":"Cannot save mouse binding",Toast.LENGTH_SHORT).show();
            })
            .setNegativeButton(zh?"取消":"Cancel",null).show();
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

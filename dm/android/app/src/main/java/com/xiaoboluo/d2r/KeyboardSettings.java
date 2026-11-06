// Original character keyboard bindings use .key; gamepad bindings use .ctl.
package com.xiaoboluo.d2r;
import android.app.Activity;
import android.graphics.Color;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
final class KeyboardSettings {
    private KeyboardSettings() {}
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
        Button keys=new Button(activity);
        keys.setAllCaps(false);
        keys.setText("原生角色 .key · 编辑主键 / 副键");
        keys.setOnClickListener(v->NativePcKeyboardSettings.show(activity));
        content.addView(keys);
        TextView hint=new TextView(activity);
        hint.setTextColor(Color.WHITE);
        hint.setText("键盘按角色 .key 选择动作，经 Switch 输入桥执行。\n"+
            "手柄使用原生 .ctl；左键交互、右键使用，中键/滚轮可以通过 .key 绑定。");
        content.addView(hint);
    }
    static void show(Activity activity) {NativePcKeyboardSettings.show(activity);}
}

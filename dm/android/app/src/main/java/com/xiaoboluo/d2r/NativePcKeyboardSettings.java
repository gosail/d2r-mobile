package com.xiaoboluo.d2r;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.view.KeyEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Toast;

/** Original Diablo II native .key controls, independent from Switch .kbm and .ctl. */
final class NativePcKeyboardSettings {
    private NativePcKeyboardSettings() {}
    private static final String[] COMMANDS={
        "Character", "Inventory", "Party", "Message log", "Quests",
        "Chat", "Help", "Automap", "Center map", "Fade map",
        "Party on map", "Names on map", "Skill tree", "Skill bar",
        "Skill 1","Skill 2","Skill 3","Skill 4","Skill 5","Skill 6","Skill 7","Skill 8",
        "Show belt","Potion 1","Potion 2","Potion 3","Potion 4",
        "Voice 1","Voice 2","Voice 3","Voice 4","Voice 5","Voice 6","Voice 7",
        "Hold run","Toggle run","Stand still","Show ground items","Close panels",
        "Previous skill","Next skill","Clear messages","Screenshot","Portraits",
        "Weapon swap","Toggle minimap",
        "Skill 9","Skill 10","Skill 11","Skill 12","Skill 13","Skill 14","Skill 15","Skill 16",
        "Hireling","Voice 8","Menu"
    };
    private static String keyName(int code) {
        if(code<0)return "Invalid";
        if(code==0xffff)return "—";
        if(code>='A' && code<='Z')return Character.toString((char)code);
        if(code>='0' && code<='9')return Character.toString((char)code);
        if(code>=0x70 && code<=0x7b)return "F"+(code-0x70+1);
        if(code>=0x60 && code<=0x69)return "Num "+(code-0x60);
        switch(code) {
            case 0x08:return "Backspace";case 0x09:return "Tab";
            case 0x0d:return "Enter";case 0x10:return "Shift";
            case 0x11:return "Ctrl";case 0x12:return "Alt";
            case 0x1b:return "Esc";case 0x20:return "Space";
            case 0xc0:return "` / ~";
            case 0x100:return "Mouse special 0x100";
            case 0x101:return "Mouse special 0x101";
            case 0x102:return "Mouse special 0x102";
            case 0x103:return "Wheel previous";
            case 0x104:return "Wheel next";
            default:return String.format("VK 0x%04X",code);
        }
    }
    private static void toast(Activity activity,String message) {
        Toast.makeText(activity,message,Toast.LENGTH_LONG).show();
    }
    static void show(Activity activity) {
        final AlertDialog[] parent={null};
        ScrollView sc=new ScrollView(activity);
        LinearLayout rows=new LinearLayout(activity);
        rows.setPadding(18,12,18,12);rows.setOrientation(LinearLayout.VERTICAL);
        sc.addView(rows);
        for(int id=0;id<COMMANDS.length;id++) {
            LinearLayout line=new LinearLayout(activity);
            line.setOrientation(LinearLayout.HORIZONTAL);
            final int command=id;
            for(int slot=0;slot<2;slot++) {
                final int s=slot;
                int vk=DmActivity.nativePcGet(command,s);
                Button button=new Button(activity);
                button.setAllCaps(false);
                button.setTextColor(Color.WHITE);
                button.setText(COMMANDS[id]+"\n"+(slot==0?"Primary: ":"Secondary: ")+keyName(vk));
                line.addView(button,new LinearLayout.LayoutParams(0,-2,1));
                button.setOnClickListener(v->capture(activity,parent[0],command,s));
            }
            rows.addView(line);
        }
        Button reset=new Button(activity);
        reset.setText("Restore verified Legacy defaults");
        reset.setOnClickListener(v->new AlertDialog.Builder(activity)
            .setMessage("Restore Legacy keyboard bindings for this character?")
            .setPositiveButton("Restore",(d,w)->{
                int rc=DmActivity.nativePcResetLegacy();
                if(rc==0){parent[0].dismiss();show(activity);}
                else toast(activity,"Cannot save native .key ("+rc+")");
            })
            .setNegativeButton("Cancel",null).show());
        rows.addView(reset);
        Button modern=new Button(activity);
        modern.setText("Restore Resurrected skills (1–0, - and =)");
        modern.setOnClickListener(v->new AlertDialog.Builder(activity)
            .setMessage("Use Resurrected skill keys 1–0, - and =? Potion keys 1–4 will be unbound to prevent conflicts.")
            .setPositiveButton("Restore",(d,w)->{
                int rc=DmActivity.nativePcResetResurrected();
                if(rc==0){parent[0].dismiss();show(activity);}
                else toast(activity,"Cannot save native .key ("+rc+")");
            }).setNegativeButton("Cancel",null).show());
        rows.addView(modern);
        parent[0]=new AlertDialog.Builder(activity).setTitle("Native PC .key · primary / secondary")
            .setView(sc).setPositiveButton("Done",null).create();
        parent[0].show();
    }
    private static void capture(Activity activity,AlertDialog parent,int action,int slot) {
        AlertDialog dialog=new AlertDialog.Builder(activity)
            .setTitle(COMMANDS[action]+" · "+(slot==0?"Primary":"Secondary"))
            .setMessage("Press a physical keyboard key")
            .setNegativeButton("Cancel",null)
            .setNeutralButton("Clear",(d,w)->{
                int rc=DmActivity.nativePcBind(action,slot,0xffff);
                if(rc==0){parent.dismiss();show(activity);}else toast(activity,"Cannot unbind ("+rc+")");
            }).create();
        dialog.setOnKeyListener((DialogInterface d,int keyCode,KeyEvent event)->{
            if(event.getAction()!=KeyEvent.ACTION_DOWN || event.getRepeatCount()!=0)return true;
            int hid=KeyboardInput.hidUsage(keyCode);
            int vk=hid<0?-1:DmActivity.nativePcVkFromHid(hid);
            if(vk<0) {
                toast(activity,"Key cannot be encoded in this native .key variant");
                return true;
            }
            int rc=DmActivity.nativePcBind(action,slot,vk);
            if(rc==0) { dialog.dismiss();parent.dismiss();show(activity); }
            else if(rc==-5)toast(activity,"Binding conflicts with another action");
            else toast(activity,"Cannot save native .key ("+rc+")");
            return true;
        });
        dialog.show();
    }
}

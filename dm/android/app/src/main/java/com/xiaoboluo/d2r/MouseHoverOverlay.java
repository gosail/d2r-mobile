package com.xiaoboluo.d2r;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

/** Non-interactive presentation for game-world mouse hover. The tooltip text
 * comes from the game's current language; item labels are never recreated. */
final class MouseHoverOverlay extends View {
    private final DmActivity activity;
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint outline = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float[] h = new float[9];
    private final int[] rect = new int[4];

    MouseHoverOverlay(DmActivity activity) {
        super(activity); this.activity = activity;
        setWillNotDraw(false); setClickable(false); setFocusable(false);
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        float d = getResources().getDisplayMetrics().density;
        text.setTextSize(18 * d); text.setColor(Color.rgb(214, 196, 148));
        outline.setStyle(Paint.Style.STROKE); outline.setStrokeWidth(2 * d);
        outline.setColor(Color.rgb(255, 220, 130));
        fill.setColor(Color.argb(190, 0, 0, 0));
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!activity.isMouseGameReady()) { postInvalidateDelayed(200); return; }
        String name = DmActivity.nativeMouseHover(h);
        if (name == null) { postInvalidateDelayed(50); return; }
        // The world uses the full window in true fullscreen, and otherwise
        // occupies the same letterboxed rectangle as the rendered picture.
        DmActivity.nativeMouseWorldRect(getWidth(), getHeight(), rect);
        float ox = rect[0], oy = rect[1], w = rect[2], ht = rect[3];
        if (w > 0 && ht > 0) {
            if (((int)h[0] == 1 || (int)h[0]==3) && !name.isEmpty()) {
                boolean monster=(int)h[0]==3;
                String[] parts=name.split("\\n",2);
                String detail=parts.length>1?parts[1]:"";
                float d = getResources().getDisplayMetrics().density;
                float size=monster?Math.max(text.measureText(parts[0]),text.measureText(detail)):text.measureText(parts[0]);
                float bw=Math.min(ox+w-8*d,Math.max(90*d,size+22*d));
                float px=ox+h[1]*w;
                float left=Math.max(ox,Math.min(ox+w-bw,px-bw/2));
                float height=monster?(detail.isEmpty()?42*d:60*d):29*d;
                float top=oy+h[2]*ht-height-3*d;
                canvas.drawRoundRect(new RectF(left,top,left+bw,top+height),4*d,4*d,fill);
                text.setColor(monster?Color.rgb(255,169,147):Color.rgb(214,196,148));
                canvas.drawText(parts[0],left+11*d,top+21*d,text);
                if(monster && !detail.isEmpty()) {
                    text.setTextSize(13*d);
                    canvas.drawText(detail,left+8*d,top+42*d,text);
                    text.setTextSize(18*d);
                }
                text.setColor(Color.rgb(214,196,148));
                if(monster && h[6]>0) {
                    Paint hp=new Paint(Paint.ANTI_ALIAS_FLAG);
                    hp.setColor(Color.DKGRAY);
                    canvas.drawRoundRect(new RectF(left+6*d,top+height-9*d,left+bw-6*d,top+height-4*d),2*d,2*d,hp);
                    hp.setColor(Color.rgb(215,55,48));
                    float fraction=Math.max(0,Math.min(1,h[5]/h[6]));
                    canvas.drawRoundRect(new RectF(left+6*d,top+height-9*d,left+6*d+(bw-12*d)*fraction,top+height-4*d),2*d,2*d,hp);
                }
            } else if ((int)h[0] == 2 || (int)h[0] == 4) {
                float d = getResources().getDisplayMetrics().density;
                RectF box = new RectF(ox + h[1] * w - 3 * d, oy + h[2] * ht - 2 * d,
                        ox + (h[1] + h[3]) * w + 3 * d, oy + (h[2] + h[4]) * ht + 2 * d);
                canvas.drawRoundRect(box, 3 * d, 3 * d, outline);
            }
        }
        postInvalidateDelayed(50);
    }
}

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
    private final float[] h = new float[5];
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
            if ((int)h[0] == 1 && !name.isEmpty()) {
                float size = text.measureText(name), px = ox + h[1] * w;
                float d = getResources().getDisplayMetrics().density;
                float left = Math.max(ox, Math.min(ox + w - size - 22 * d, px - size / 2 - 11 * d));
                float top = oy + h[2] * ht - 32 * d;
                canvas.drawRoundRect(new RectF(left, top, left + size + 22 * d, top + 29 * d), 4 * d, 4 * d, fill);
                canvas.drawText(name, left + 11 * d, top + 21 * d, text);
            } else if ((int)h[0] == 2) {
                float d = getResources().getDisplayMetrics().density;
                RectF box = new RectF(ox + h[1] * w - 3 * d, oy + h[2] * ht - 2 * d,
                        ox + (h[1] + h[3]) * w + 3 * d, oy + (h[2] + h[4]) * ht + 2 * d);
                canvas.drawRoundRect(box, 3 * d, 3 * d, outline);
            }
        }
        postInvalidateDelayed(50);
    }
}

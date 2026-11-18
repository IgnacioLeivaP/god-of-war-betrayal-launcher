package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public class DirectUtils {
    public static DirectGraphics getDirectGraphics(final Graphics g) {
        return new DirectGraphics() {
            public void drawImage(Image img, int x, int y, int anchor, int manip) {
                g.dibuja(img, x, y, anchor, ((manip & FLIP_HORIZONTAL) != 0 ? 1 : 0) | ((manip & FLIP_VERTICAL) != 0 ? 2 : 0));
            }
        };
    }
}

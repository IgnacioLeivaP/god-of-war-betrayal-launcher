package com.nokia.mid.ui;

import javax.microedition.lcdui.Image;

public interface DirectGraphics {
    int FLIP_HORIZONTAL = 8192, FLIP_VERTICAL = 16384;

    void drawImage(Image img, int x, int y, int anchor, int manipulation);
}

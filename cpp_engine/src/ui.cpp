#define NOMINMAX
#include <Windows.h>

#include <GL/gl.h>

#include "engine/ui.hpp"

namespace svl {

UiRect camera_button_rect(int viewport_width, int) {
    const int button_w = 72;
    const int button_h = 40;
    const int margin = 16;
    return UiRect{
        viewport_width - margin - button_w,
        margin,
        viewport_width - margin,
        margin + button_h,
    };
}

bool rect_contains(const UiRect& rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void draw_camera_toggle_ui(int viewport_width, int viewport_height, bool tracking_enabled) {
    const UiRect rect = camera_button_rect(viewport_width, viewport_height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(viewport_width), static_cast<double>(viewport_height), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    if (tracking_enabled) {
        glColor3d(0.18, 0.62, 0.32);
    } else {
        glColor3d(0.20, 0.24, 0.34);
    }
    glBegin(GL_QUADS);
    glVertex2i(rect.left, rect.top);
    glVertex2i(rect.right, rect.top);
    glVertex2i(rect.right, rect.bottom);
    glVertex2i(rect.left, rect.bottom);
    glEnd();

    glColor3d(0.85, 0.88, 0.96);
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2i(rect.left, rect.top);
    glVertex2i(rect.right, rect.top);
    glVertex2i(rect.right, rect.bottom);
    glVertex2i(rect.left, rect.bottom);
    glEnd();

    const int cx = (rect.left + rect.right) / 2;
    const int cy = (rect.top + rect.bottom) / 2;
    const int body_w = 24;
    const int body_h = 14;

    glBegin(GL_LINE_LOOP);
    glVertex2i(cx - body_w / 2, cy - body_h / 2);
    glVertex2i(cx + body_w / 2, cy - body_h / 2);
    glVertex2i(cx + body_w / 2, cy + body_h / 2);
    glVertex2i(cx - body_w / 2, cy + body_h / 2);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2i(cx + body_w / 2, cy - 5);
    glVertex2i(cx + body_w / 2 + 10, cy);
    glVertex2i(cx + body_w / 2, cy + 5);
    glEnd();

    glPopAttrib();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace svl


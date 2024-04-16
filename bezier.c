#include <xgfx/drawing.h>

typedef struct {
    int x;
    int y;
} Point;

Point lerp(Point a, Point b, float t) {
    Point c;
    c.x = a.x + (b.x - a.x) * t;
    c.y = a.y + (b.y - a.y) * t;
    return c;
}

Point bezier(Point a, Point b, Point c, float t) {
    return lerp(lerp(a, b, t), lerp(b, c, t), t);
}

void drawBezier(Point a, Point b, Point c, int color) {
    for (float t = 0; t < 1.f; t += 0.1f) {
        Point start = bezier(a, b, c, t);
        Point end = bezier(a, b, c, t + 0.1f);
        line(start.x, start.y, end.x, end.y, color);
    }
}
#include "chrome_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct ChromeMetrics small = chrome_metrics(480), current = chrome_metrics(800),
                         hd = chrome_metrics(1080), double_size = chrome_metrics(1600),
                         huge = chrome_metrics(4320);
    assert(small.bar == 20 && small.font >= 11);
    assert(current.bar == 26 && current.icon == 16 && current.font == 13);
    assert(hd.bar == 35 && hd.icon > current.icon && hd.font > current.font);
    assert(double_size.bar == 2 * current.bar && double_size.icon == 2 * current.icon);
    assert(huge.bar == 52 && huge.font == 26);
    assert(chrome_metrics(0).bar == 20 && chrome_metrics(1000000).bar == 52);
    struct ChromeRect start = {260, 75, 1400, 650}, r;
    r = chrome_motion(start, 100, 50, 0, 1920, 800, 11, 820, 500);
    assert(r.x == 360 && r.y == 125 && r.width == 1400 && r.height == 650);
    r = chrome_motion(start, -500, -500, 0, 1920, 800, 11, 820, 500);
    assert(r.x == 0 && r.y == 11);
    r = chrome_motion(start, 900, 900, 0, 1920, 800, 11, 820, 500);
    assert(r.x == 520 && r.y == 150);
    r = chrome_motion(start, 100, 50, 1, 1920, 800, 11, 820, 500);
    assert(r.x == 260 && r.y == 75 && r.width == 1500 && r.height == 700);
    r = chrome_motion(start, -10000, -10000, 1, 1920, 800, 11, 820, 500);
    assert(r.width == 820 && r.height == 500);
    r = chrome_motion(start, 10000, 10000, 1, 1920, 800, 11, 820, 500);
    assert(r.width == 1660 && r.height == 725);
    /* An object larger than its screen must keep its title reachable. */
    r = chrome_motion(start, 0, 0, 0, 640, 480, 11, 820, 500);
    assert(r.x == 0 && r.y == 11);
    puts("Chrome drag/resize bounds and minimum sizes passed.");
    return 0;
}

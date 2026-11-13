#include "chrome_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
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

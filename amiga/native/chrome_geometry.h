#ifndef TERMIC_CHROME_GEOMETRY_H
#define TERMIC_CHROME_GEOMETRY_H
struct ChromeRect
{
    long x, y, width, height;
};
static long chrome_limit(long n, long low, long high)
{
    if (high < low)
        high = low;
    return n < low ? low : n > high ? high : n;
}
static struct ChromeRect chrome_motion(struct ChromeRect start, long dx, long dy, int resize,
                                       long screen_width, long screen_height, long screen_top,
                                       long minimum_width, long minimum_height)
{
    if (resize)
    {
        start.width = chrome_limit(start.width + dx, minimum_width, screen_width - start.x);
        start.height = chrome_limit(start.height + dy, minimum_height, screen_height - start.y);
    }
    else
    {
        start.x = chrome_limit(start.x + dx, 0, screen_width - start.width);
        start.y = chrome_limit(start.y + dy, screen_top, screen_height - start.height);
    }
    return start;
}
#endif

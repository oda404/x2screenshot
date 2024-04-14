/**
 * Copyright Alexandru Olaru
 * See LICENSE file for copyright and license details
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static Window g_win;
static GC g_gc;
static int g_origin_x, g_end_x;
static int g_origin_y, g_end_y;

static int create_blur_window(Display* dpy, Window root, int screen)
{
    XVisualInfo vinfo;
    XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo);

    XSetWindowAttributes wa;
    wa.colormap = XCreateColormap(dpy, root, vinfo.visual, AllocNone);
    wa.border_pixel = 0;
    wa.background_pixel = 0;
    wa.override_redirect = 1;

    g_win = XCreateWindow(
        dpy,
        root,
        0,
        0,
        DisplayWidth(dpy, screen),
        DisplayHeight(dpy, screen),
        0,
        vinfo.depth,
        CopyFromParent,
        vinfo.visual,
        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel,
        &wa);

    g_gc = XCreateGC(dpy, g_win, 0, NULL);
    XSetForeground(dpy, g_gc, 0x55000000);
    XMapRaised(dpy, g_win);
    return 0;
}

static int grab_keyboard(Display* dpy, Window root)
{
    /* yoinked from dmenu */
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
    for (size_t i = 0; i < 1000; ++i)
    {
        if (XGrabKeyboard(
                dpy, root, true, GrabModeAsync, GrabModeAsync, CurrentTime) ==
            GrabSuccess)
            return 0;

        nanosleep(&ts, NULL);
    }

    return -1;
}

static void destroy_blur_window(Display* dpy)
{
    XFreeGC(dpy, g_gc);
    XDestroyWindow(dpy, g_win);
}

static void arrange_blur_window(Display* dpy, int screen, int curx, int cury)
{
    const size_t w = DisplayWidth(dpy, screen);
    const size_t h = DisplayHeight(dpy, screen);

    /* Clear working area */
    {
        size_t xs = g_origin_x;
        size_t ys = g_origin_y;
        size_t xe = g_end_x;
        size_t ye = g_end_y;

        if (xe < xs)
        {
            size_t tmp = xe;
            xe = xs;
            xs = tmp;
        }

        if (ye < ys)
        {
            size_t tmp = ye;
            ye = ys;
            ys = tmp;
        }

        size_t w = xe - xs;
        size_t h = ye - ys;
        XClearArea(dpy, g_win, xs, ys, w, h, true);
    }

    /* There must be a cleaner way to do this... */
    if (curx >= g_origin_x && cury >= g_origin_y)
    {
        XFillRectangle(dpy, g_win, g_gc, 0, 0, g_origin_x, cury);
        XFillRectangle(dpy, g_win, g_gc, 0, cury, curx, h - cury);
        XFillRectangle(
            dpy, g_win, g_gc, g_origin_x, 0, w - g_origin_x, g_origin_y);
        XFillRectangle(
            dpy, g_win, g_gc, curx, g_origin_y, w - curx, h - g_origin_y);
    }
    else if (curx < g_origin_x && cury >= g_origin_y)
    {
        XFillRectangle(dpy, g_win, g_gc, 0, 0, curx, cury);
        XFillRectangle(dpy, g_win, g_gc, 0, cury, g_origin_x, h - cury);
        XFillRectangle(dpy, g_win, g_gc, curx, 0, w - curx, g_origin_y);
        XFillRectangle(
            dpy,
            g_win,
            g_gc,
            g_origin_x,
            g_origin_y,
            w - g_origin_x,
            h - g_origin_y);
    }
    else if (curx >= g_origin_x && cury < g_origin_y)
    {
        XFillRectangle(dpy, g_win, g_gc, 0, 0, g_origin_x, g_origin_y);
        XFillRectangle(dpy, g_win, g_gc, 0, g_origin_y, curx, h - g_origin_y);
        XFillRectangle(dpy, g_win, g_gc, g_origin_x, 0, w - g_origin_x, cury);
        XFillRectangle(dpy, g_win, g_gc, curx, cury, w - curx, h - cury);
    }
    else
    {
        XFillRectangle(dpy, g_win, g_gc, 0, 0, curx, g_origin_y);
        XFillRectangle(
            dpy, g_win, g_gc, 0, g_origin_y, g_origin_x, h - g_origin_y);
        XFillRectangle(dpy, g_win, g_gc, curx, 0, w - curx, cury);
        XFillRectangle(
            dpy, g_win, g_gc, g_origin_x, cury, w - g_origin_x, h - cury);
    }

    g_end_x = curx;
    g_end_y = cury;
}

static void set_capture_origin_to_cursor(Display* dpy, Window root, int screen)
{
    Window ret1, ret2;
    int xret1, yret1;
    unsigned int maskret;
    XQueryPointer(
        dpy,
        root,
        &ret1,
        &ret2,
        &g_origin_x,
        &g_origin_y,
        &xret1,
        &yret1,
        &maskret);
    arrange_blur_window(dpy, screen, g_origin_x, g_origin_y);
}

static void save_screenshot()
{
    if (g_end_x < g_origin_x)
    {
        int tmp = g_end_x;
        g_end_x = g_origin_x;
        g_origin_x = tmp;
    }

    if (g_end_y < g_origin_y)
    {
        int tmp = g_end_y;
        g_end_y = g_origin_y;
        g_origin_y = tmp;
    }

    size_t w = g_end_x - g_origin_x;
    size_t h = g_end_y - g_origin_y;

    char cmdbuf[64];
    snprintf(
        cmdbuf, 64, "scrot -a %d,%d,%zu,%zu", g_origin_x, g_origin_y, w, h);

    /* FIXME: stinky shell out to scrot */
    system(cmdbuf);
}

static int screenshot_thread()
{
    Display* dpy = XOpenDisplay(NULL);
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    /* I don't really yet get how X11 does error handling/too lazy to read man
     * pages so no error handling for you */
    create_blur_window(dpy, root, screen);

    if (grab_keyboard(dpy, root) < 0)
    {
        destroy_blur_window(dpy);
        XCloseDisplay(dpy);
        return -1;
    }

    XSelectInput(
        dpy,
        g_win,
        PointerMotionMask | ButtonPressMask | SubstructureNotifyMask);
    set_capture_origin_to_cursor(dpy, root, screen);
    XSync(dpy, 0);

    XEvent ev;
    bool should_save = true;
    bool running = true;
    while (running && !XNextEvent(dpy, &ev))
    {
        if (ev.type == KeyPress)
        {
            switch (XLookupKeysym(&ev.xkey, 0))
            {
            case XK_s:
                set_capture_origin_to_cursor(dpy, root, screen);
                break;

            case XK_a:
                g_origin_x = 0;
                g_origin_y = 0;
                g_end_x = DisplayWidth(dpy, screen);
                g_end_y = DisplayHeight(dpy, screen);
                running = false;
                break;

            case XK_Escape:
                should_save = false;
                running = false;
                break;

            case XK_c:
                if (ev.xkey.state != ControlMask)
                    break;

                should_save = false;
                running = false;
                break;
            }
        }
        else if (ev.type == MotionNotify)
        {
            arrange_blur_window(dpy, screen, ev.xmotion.x, ev.xmotion.y);
        }
        else if (ev.type == ButtonPress)
        {
            running = false;
        }
        else
        {
            XRaiseWindow(dpy, g_win);
        }
    }

    /* is this necessary? */
    XUngrabKeyboard(dpy, CurrentTime);

    destroy_blur_window(dpy);
    XCloseDisplay(dpy);

    /* Wait for everything to close so we have a clear view of the screen */
    if (should_save)
        save_screenshot();

    return 0;
}

int main(int argc, char** argv)
{
    return screenshot_thread(); // Can't be fucked to rename this
}

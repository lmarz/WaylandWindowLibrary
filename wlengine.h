#include <wayland-client.h>
#include "xdg-shell.h"

typedef struct window {
    struct wl_display* display;
    struct wl_compositor* compositor;
    struct wl_shm* shm;
    struct xdg_wm_base* wm_base;
    struct wl_surface* surface;
    struct xdg_toplevel* toplevel;

    int width;
    int height;
    int running;

    uint32_t* content;
} window;

window* createWindow(int width, int height, const char* title);
int windowShouldClose(window* window);
void getDimensions(window* window, int* width, int* height);
int draw(window* window, uint32_t* content, int size);
void setTitle(window* window, const char* title);
void closeWindow(window* window);
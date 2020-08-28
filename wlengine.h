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

/**
 * Creates and returns a window
 * @width: The width of the window
 * @height: The height of the window
 * @title: The title of the window
 */
window* createWindow(int width, int height, const char* title);

/**
 * Returns true, when the window should be closed
 * @window: The window object
 */
int windowShouldClose(window* window);

/**
 * Gets the current width and height of the window
 * @window: The window object
 * @width: A pointer, where the width will be written
 * @height: A pointer, where the height will be written
 */
void getDimensions(window* window, int* width, int* height);

/**
 * Draws the content to the screen
 * @window: The window object
 * @content: An array, which represents the pixel colors. A Pixel has the format
 * XRGB
 * @size: The size of the content array. This should be width * height * 4
 */
int draw(window* window, uint32_t* content, int size);

/**
 * Sets the title of the window
 * @window: The window object
 * @title: The new title
 */
void setTitle(window* window, const char* title);

/**
 * Closes the window
 * @window: The window object
 */
void closeWindow(window* window);
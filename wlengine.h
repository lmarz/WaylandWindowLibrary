#ifndef WLENGINE_H
#define WLENIGNE_H

#include <inttypes.h>

typedef void wlengineWindow;

/**
 * Creates and returns a window
 * @width: The width of the window
 * @height: The height of the window
 * @title: The title of the window
 */
wlengineWindow* wlengineCreateWindow(int width, int height, const char* title);

/**
 * Returns true, when the window should be closed
 * @window: The window object
 */
int wlengineShouldClose(wlengineWindow* window);

/**
 * Gets the current width and height of the window
 * @window: The window object
 * @width: A pointer, where the width will be written
 * @height: A pointer, where the height will be written
 */
void wlengineGetDimensions(wlengineWindow* window, int* width, int* height);

/**
 * Draws the content to the screen
 * @window: The window object
 * @content: An array, which represents the pixel colors. A Pixel has the format
 * XRGB
 * @size: The size of the content array. This should be width * height * 4
 */
int wlengineDraw(wlengineWindow* window, uint32_t* content, int size);

/**
 * Sets the title of the window
 * @window: The window object
 * @title: The new title
 */
void wlengineSetTitle(wlengineWindow* window, const char* title);

/**
 * Closes the window
 * @window: The window object
 */
void wlengineCloseWindow(wlengineWindow* window);

#endif /* WLENGINE_H */
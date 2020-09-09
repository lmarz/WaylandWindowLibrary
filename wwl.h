/**
 * MIT License
 * 
 * Copyright (c) 2020 lmarz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#ifndef WWL_H
#define WWL_H

#include <inttypes.h>

typedef void wwlWindow;

/**
 * Creates and returns a window
 * @width: The width of the window
 * @height: The height of the window
 * @title: The title of the window
 */
wwlWindow* wwlCreateWindow(int width, int height, const char* title);

/**
 * Returns true, when the window should be closed
 * @window: The window object
 */
int wwlShouldClose(wwlWindow* window);

/**
 * Gets the current width and height of the window
 * @window: The window object
 * @width: A pointer, where the width will be written
 * @height: A pointer, where the height will be written
 */
void wwlGetDimensions(wwlWindow* window, int* width, int* height);

/**
 * Draws the content to the screen
 * @window: The window object
 * @content: An array, which represents the pixel colors. A Pixel has the format
 * XRGB
 * @size: The size of the content array. This should be width * height * 4
 */
int wwlDraw(wwlWindow* window, uint32_t* content, int size);

/**
 * Redraw only a specific region on screen
 * @window: The window object
 * @content: An array, which represents the pixel colors. A Pixel has the format
 * XRGB
 * @x: The top left x position of the region
 * @y: The top left y position of the region
 * @width: The width of the region
 * @height: The height of the region
 */
int wwlDrawRegion(wwlWindow* window, uint32_t* content, int x, int y, int width, int height);

/**
 * Sets the title of the window
 * @window: The window object
 * @title: The new title
 */
void wwlSetTitle(wwlWindow* window, const char* title);

enum wwlKeyAction {
    WWL_KEY_PRESSED,
    WWL_KEY_RELEASED
};

/**
 * Sets the callback for when a key is pressed or released
 * @window: The window object
 * @key_callback: The callback function, which takes the window object, the name of the key and the action (either pressed or released)
 */
void wwlSetKeyCallback(wwlWindow* window, void (*key_callback)(wwlWindow* window, char* key, enum wwlKeyAction action));

/**
 * Gets the current position of the cursor in the window
 * @window: The window object
 * @x: The x position of the cursor
 * @y: The y position of the cursor
 */
void wwlGetCursorPos(wwlWindow* window, double* x, double* y);

/**
 * Sets the callback for when the cursor gets moved in the window
 * @window: The window object
 * @cursor_callback: The callback function, which takes the window object, the x coordinate in the window of the cursor and the y coordinate in the window of the cursor
 */
void wwlSetCursorCallback(wwlWindow* window, void (*cursor_callback)(wwlWindow* window, double x, double y));

/**
 * Sets the callback for when a button on the mouse gets pressed or released
 * @window: The window object
 * @button_callback: The callback function, which takes the window object, the name of the button and the action (either pressed or released)
 */
void wwlSetMouseButtonCallback(wwlWindow* window, void (*button_callback)(wwlWindow* window, int button, enum wwlKeyAction action));

/**
 * Sets the callback for when the scrolling happens
 * @window: The window object
 * @scroll_callback: The callback function, which takes the window object, the vertical offset and the horizontal offset
 */
void wwlSetScrollCallback(wwlWindow* window, void (*scroll_callback)(wwlWindow* window, double x_offset, double y_offset));

/**
 * Closes the window
 * @window: The window object
 */
void wwlCloseWindow(wwlWindow* window);

#endif /* WWL_H */
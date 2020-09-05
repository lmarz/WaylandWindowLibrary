#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wwl.h"

void key_callback(wwlWindow* window, char* key, enum wwlKeyAction action) {
    /* The keys a modifier sensitive. This means Shift_L and a have been pressed */
    if(strcmp(key, "A") == 0 && action == WWL_KEY_PRESSED) {
        printf("'A' was pressed\n");
    }
}

int main(int argc, char const *argv[]) {
    /* Create a window */
    wwlWindow* window = wwlCreateWindow(800, 600, "Test");

    /* Set the key callback function */
    wwlSetKeyCallback(window, key_callback);

    /* Get the current size of the window. This is important for tiling WMs like
    sway */
    int width, height;
    wwlGetDimensions(window, &width, &height);

    /* Create the content of the window and make all pixels white */
    uint32_t* content = malloc(width * height * sizeof(uint32_t));
    for(int i = 0; i < width * height; i++) {
        content[i] = 0xFFFFFFFF;
    }

    /* Draw the initial content */
    wwlDraw(window, content, width * height * sizeof(uint32_t));

    /* Main loop */
    while(!wwlShouldClose(window)) {
        /* Check if the window has been resized */
        int newWidth, newHeight;
        wwlGetDimensions(window, &newWidth, &newHeight);
        if(newWidth != width || newHeight != height) {
            /* Recreate the content with the new size */
            width = newWidth;
            height = newHeight;
            free(content);
            content = malloc(width * height * sizeof(uint32_t));
            for(int i = 0; i < width * height; i++) {
                content[i] = 0xFFFFFFFF;
            }
            wwlDraw(window, content, width * height * sizeof(uint32_t));
        }
    }

    /* Destroy the window after the close request */
    wwlCloseWindow(window);
    return 0;
}

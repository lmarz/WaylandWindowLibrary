#include <stdlib.h>
#include "wwl.h"

int main(int argc, char const *argv[]) {
    /* Create a window */
    wlengineWindow* window = wlengineCreateWindow(800, 600, "Test");

    /* Get the current size of the window. This is important for tiling WMs like
    sway */
    int width, height;
    wlengineGetDimensions(window, &width, &height);

    /* Create the content of the window and make all pixels white */
    uint32_t* content = malloc(width * height * sizeof(uint32_t));
    for(int i = 0; i < width * height; i++) {
        content[i] = 0xFFFFFFFF;
    }

    /* Draw the initial content */
    wlengineDraw(window, content, width * height * sizeof(uint32_t));

    /* Main loop */
    while(!wlengineShouldClose(window)) {
        /* Check if the window has been resized */
        int newWidth, newHeight;
        wlengineGetDimensions(window, &newWidth, &newHeight);
        if(newWidth != width || newHeight != height) {
            /* Recreate the content with the new size */
            width = newWidth;
            height = newHeight;
            free(content);
            content = malloc(width * height * sizeof(uint32_t));
            for(int i = 0; i < width * height; i++) {
                content[i] = 0xFFFFFFFF;
            }
            wlengineDraw(window, content, width * height * sizeof(uint32_t));
        }
    }

    /* Destroy the window after the close request */
    wlengineCloseWindow(window);
    return 0;
}

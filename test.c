#include <stdlib.h>
#include "wlengine.h"

int main(int argc, char const *argv[]) {
    window* window = createWindow(800, 600, "Test");
    int width, height;
    getDimensions(window, &width, &height);
    uint32_t* content = malloc(width * height * sizeof(uint32_t));
    for(int i = 0; i < width * height; i++) {
        content[i] = 0xFFFFFFFF;
    }
    draw(window, content, width * height * sizeof(uint32_t));
    while(!windowShouldClose(window)) {
        int newWidth, newHeight;
        getDimensions(window, &newWidth, &newHeight);
        if(newWidth != width || newHeight != height) {
            width = newWidth;
            height = newHeight;
            free(content);
            content = malloc(width * height * sizeof(uint32_t));
            for(int i = 0; i < width * height; i++) {
                content[i] = 0xFFFFFFFF;
            }
            draw(window, content, width * height * sizeof(uint32_t));
        }
    }
    closeWindow(window);
    return 0;
}

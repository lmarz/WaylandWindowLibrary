#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell.h"

typedef struct wlengineWindow {
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
} wlengineWindow;

/**
 * ==================================
 * Shared memory section
 * ==================================
 */

/**
 * Create a pseudo random name for the shared memory file
 * @buf: The name template
 */
static void randname(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

/**
 * Create a shared memory file in /dev/shm
 * @size: The size of the file
 */
static int create_shm_file(int size) {
    char name[] = "/wlengine-XXXXXX";
    int retries = 100;
    int fd = -1;

    do {
        randname(name + strlen(name) - 6);
        --retries;
        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if(fd >= 0) {
            shm_unlink(name);
            break;
        }
    } while (retries > 0 && errno == EEXIST);
    if(fd < 0) {
        return fd;
    }
    if(ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/**
 * ==================================
 * Buffer Section
 * ==================================
 */

/**
 * Delete a buffer, when it's not in use anymore
 */
static void release_buffer(void *data, struct wl_buffer *wl_buffer) {
    wl_buffer_destroy(wl_buffer);
}

static struct wl_buffer_listener buffer_listener = {
    release_buffer
};

/**
 * Create a buffer and fill it with content
 * @window: The window object
 * @content: The content of the buffer. When NULL, the function creates a black
 * buffer
 */
static struct wl_buffer* createFrame(wlengineWindow* window, uint32_t* content) {
    int stride = window->width * 4;
    int size = stride * window->height;
    int fd = create_shm_file(size);
    if(fd < 0) {
        fprintf(stderr, "Couldn't create shared memory file\n");
        return NULL;
    }

    uint32_t* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(data == MAP_FAILED) {
        fprintf(stderr, "mmap failed %d %d\n", window->width, window->height);
        close(fd);
        return NULL;
    }

    struct wl_shm_pool* pool = wl_shm_create_pool(window->shm, fd, size);
    struct wl_buffer* buffer = wl_shm_pool_create_buffer(pool, 0, window->width, window->height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    if(content != NULL) {
        memcpy(data, content, size);
    } else {
        for(int i = 0; i < window->width * window->height; i++) {
            data[i] = 0xFF000000;
        }
    }
    munmap(data, size);
    wl_buffer_add_listener(buffer, &buffer_listener, NULL);
    return buffer;
}

/**
 * ==================================
 * Listener Section
 * ==================================
 */

/**
 * Get the global objects
 */
static void global_listener(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
    wlengineWindow* window = data;
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        window->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
    } else if(strcmp(interface, wl_shm_interface.name) == 0) {
        window->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
    } else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
        window->wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, version);
    }
}

static void global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
    /* Nada */
}

static struct wl_registry_listener listener = {
    global_listener,
    global_remove
};

/**
 * Honestly. I don't really know what's up with this function. I have to have it
 * and I have to create and attach a buffer, even if I don't want to.
 */
static void surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    wlengineWindow* window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    struct wl_buffer* buffer = createFrame(window, window->content);
    wl_surface_attach(window->surface, buffer, 0, 0);
    wl_surface_commit(window->surface);
}

static struct xdg_surface_listener surface_listener = {
    surface_configure
};

/**
 * Gets called when the size of the window changes
 */
static void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    wlengineWindow* window = data;
    if(width != 0 && height != 0) {
        window->width = width;
        window->height = height;
    }
}

/**
 * Gets called when the window should be closed
 */
static void toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    wlengineWindow* window = data;
    window->running = 0;
}

static struct xdg_toplevel_listener toplevel_listener = {
    toplevel_configure,
    toplevel_close
};

/**
 * ==================================
 * API Section
 * ==================================
 */
wlengineWindow* wlengineCreateWindow(int width, int height, const char* title) {
    wlengineWindow* window = malloc(sizeof(wlengineWindow));
    window->width = width;
    window->height = height;
    window->running = 1;
    
    window->display = wl_display_connect(NULL);
    if(window->display == NULL) {
        fprintf(stderr, "Couldn't find wayland display\n");
        return NULL;
    }
    struct wl_registry* registry = wl_display_get_registry(window->display);
    wl_registry_add_listener(registry, &listener, window);
    wl_display_roundtrip(window->display);
    window->surface = wl_compositor_create_surface(window->compositor);
    struct xdg_surface* xdg_surface = xdg_wm_base_get_xdg_surface(window->wm_base, window->surface);
    window->toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_surface_add_listener(xdg_surface, &surface_listener, window);
    xdg_toplevel_add_listener(window->toplevel, &toplevel_listener, window);
    xdg_toplevel_set_title(window->toplevel, title);
    wl_surface_commit(window->surface);

    return window;
}

int wlengineShouldClose(wlengineWindow* window) {
    return wl_display_dispatch(window->display) == -1 || !window->running;
}

void wlengineGetDimensions(wlengineWindow* window, int* width, int* height) {
    *width = window->width;
    *height = window->height;
}

/**
 * When the compositor is ready, this function is called. It attaches the new
 * buffer to the surface with content from the draw() call
 */
static void callback_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data) {
    wlengineWindow* window = data;
    
    wl_callback_destroy(wl_callback);
    struct wl_buffer* buffer = createFrame(window, window->content);
    wl_surface_attach(window->surface, buffer, 0, 0);
    wl_surface_damage_buffer(window->surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(window->surface);
}

static struct wl_callback_listener callback_listener = {
    callback_done
};

int wlengineDraw(wlengineWindow* window, uint32_t* content, int size) {
    if(window->width * window->height * 4 != size) {
        fprintf(stderr, "Size doesn't match\n");
        return -1;
    }
    window->content = content;
    struct wl_callback* cb = wl_surface_frame(window->surface);
    wl_callback_add_listener(cb, &callback_listener, window);
    return 0;
}

void wlengineSetTitle(wlengineWindow* window, const char* title) {
    xdg_toplevel_set_title(window->toplevel, title);
    wl_surface_commit(window->surface);
}

void wlengineCloseWindow(wlengineWindow* window) {
    window->running = 0;
    xdg_toplevel_destroy(window->toplevel);
    wl_surface_destroy(window->surface);
    free(window);
}
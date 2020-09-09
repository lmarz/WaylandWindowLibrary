#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include "xdg-shell.h"

enum wwlKeyAction {
    WWL_KEY_PRESSED,
    WWL_KEY_RELEASED
};

enum pointer_event_mask {
       POINTER_EVENT_ENTER = 1 << 0,
       POINTER_EVENT_LEAVE = 1 << 1,
       POINTER_EVENT_MOTION = 1 << 2,
       POINTER_EVENT_BUTTON = 1 << 3,
       POINTER_EVENT_AXIS = 1 << 4,
       POINTER_EVENT_AXIS_SOURCE = 1 << 5,
       POINTER_EVENT_AXIS_STOP = 1 << 6,
       POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct pointer_event {
       uint32_t event_mask;
       wl_fixed_t surface_x, surface_y;
       uint32_t button, state;
       uint32_t time;
       uint32_t serial;
       struct {
               int valid;
               wl_fixed_t value;
               int32_t discrete;
       } axes[2];
       uint32_t axis_source;
};

typedef struct wwlWindow {
    struct wl_display* display;
    struct wl_compositor* compositor;
    struct wl_shm* shm;
    struct xdg_wm_base* wm_base;
    struct wl_surface* surface;
    struct xdg_toplevel* toplevel;
    struct wl_seat* seat;
    struct wl_keyboard* keyboard;
    struct wl_pointer* pointer;
    struct xkb_state* keyboard_state;
    struct xkb_context* keyboard_context;
    struct xkb_keymap* keyboard_keymap;

    int width;
    int height;
    int running;
    int damaged;

    uint32_t* content;
    struct wl_buffer* buffer;

    void (*key_callback)(void* window, char* key, enum wwlKeyAction action);
    void (*cursor_callback)(void* window, double x, double y);
    void (*button_callback)(void* window, int button, enum wwlKeyAction action);
    void (*scroll_callback)(void* window, double x_offset, double y_offset);
    struct pointer_event pointer_event;
    double cursor_x;
    double cursor_y;
} wwlWindow;

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
    char name[] = "/wwl-XXXXXX";
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
static struct wl_buffer* createFrame(wwlWindow* window, uint32_t* content) {
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

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
    wwlWindow* window = data;
    
    if(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        char* map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if(map_shm != MAP_FAILED) {
            xkb_keymap_unref(window->keyboard_keymap);
            xkb_state_unref(window->keyboard_state);
            window->keyboard_keymap = xkb_keymap_new_from_string(window->keyboard_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_MAP_COMPILE_NO_FLAGS);
            munmap(map_shm, size);
            close(fd);
            window->keyboard_state = xkb_state_new(window->keyboard_keymap);
        }
    }
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    wwlWindow* window = data;

    if(window->key_callback != NULL) {
        uint32_t* key;
        wl_array_for_each(key, keys) {
            char buf[128];
            xkb_keysym_t sym = xkb_state_key_get_one_sym(window->keyboard_state, *key+8);
            xkb_keysym_get_name(sym, buf, sizeof(buf));
            window->key_callback(window, buf, WWL_KEY_PRESSED);
        }
    }
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {

}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    wwlWindow* window = data;

    if(window->key_callback != NULL) {
        char buf[128];
        uint32_t keycode = key+8;
        xkb_keysym_t sym = xkb_state_key_get_one_sym(window->keyboard_state, keycode);
        xkb_keysym_get_name(sym, buf, sizeof(buf));
        enum wwlKeyAction action = state == WL_KEYBOARD_KEY_STATE_PRESSED ? WWL_KEY_PRESSED : WWL_KEY_RELEASED;
        window->key_callback(window, buf, action);
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    wwlWindow* window = data;
    xkb_state_update_mask(window->keyboard_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {

}

static struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info
};

static void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_ENTER;
    window->pointer_event.serial = serial;
    window->pointer_event.surface_x = surface_x;
    window->pointer_event.surface_y = surface_y;
}

static void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
    window->pointer_event.serial = serial;
}

static void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_MOTION;
    window->pointer_event.time = time;
    window->pointer_event.surface_x = surface_x;
    window->pointer_event.surface_y = surface_y;
}

static void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
    window->pointer_event.time = time;
    window->pointer_event.serial = serial;
    window->pointer_event.button = button;
    window->pointer_event.state = state;
}

static void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_AXIS;
    window->pointer_event.time = time;
    window->pointer_event.axes[axis].valid = 1;
    window->pointer_event.axes[axis].value = value;
}

static void pointer_frame(void *data, struct wl_pointer *wl_pointer) {
    wwlWindow* window = data;
    struct pointer_event* event = &window->pointer_event;

    if(event->event_mask & POINTER_EVENT_ENTER) {
        window->cursor_x = wl_fixed_to_double(event->surface_x);
        window->cursor_y = wl_fixed_to_double(event->surface_y);
        if(window->cursor_callback != NULL) {
            window->cursor_callback(window, window->cursor_x, window->cursor_y);
        }
    }
    if(event->event_mask & POINTER_EVENT_LEAVE) {
        
    }
    if(event->event_mask & POINTER_EVENT_MOTION) {
        window->cursor_x = wl_fixed_to_double(event->surface_x);
        window->cursor_y = wl_fixed_to_double(event->surface_y);
        if(window->cursor_callback != NULL) {
            window->cursor_callback(window, window->cursor_x, window->cursor_y);
        }
    }
    if(event->event_mask & POINTER_EVENT_BUTTON) {
        enum wwlKeyAction action = event->state == WL_POINTER_BUTTON_STATE_PRESSED ? WWL_KEY_PRESSED : WWL_KEY_RELEASED;
        if(window->button_callback != NULL) {
            window->button_callback(window, event->button, action);
        }
    }

    uint32_t axis_events = POINTER_EVENT_AXIS
            | POINTER_EVENT_AXIS_SOURCE
            | POINTER_EVENT_AXIS_STOP
            | POINTER_EVENT_AXIS_DISCRETE;
    if (event->event_mask & axis_events) {
        if(window->scroll_callback != NULL) {
            window->scroll_callback(window, event->axes[WL_POINTER_AXIS_VERTICAL_SCROLL].value, event->axes[WL_POINTER_AXIS_HORIZONTAL_SCROLL].value);
        }
    }
    memset(event, 0, sizeof(*event));
}

static void pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
    window->pointer_event.axis_source = axis_source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_AXIS_STOP;
    window->pointer_event.time = time;
    window->pointer_event.axes[axis].valid = 1;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
    wwlWindow* window = data;
    window->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
    window->pointer_event.axes[axis].valid = 1;
    window->pointer_event.axes[axis].discrete = discrete;
}

static struct wl_pointer_listener pointer_listener = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
    pointer_frame,
    pointer_axis_source,
    pointer_axis_stop,
    pointer_axis_discrete
};

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
    wwlWindow* window = data;

    if(window->keyboard == NULL) {
        window->keyboard = wl_seat_get_keyboard(wl_seat);
        wl_keyboard_add_listener(window->keyboard, &keyboard_listener, window);
    }

    if(window->pointer == NULL) {
        window->pointer = wl_seat_get_pointer(wl_seat);
        wl_pointer_add_listener(window->pointer, &pointer_listener, window);
    }
}

static void seat_name(void *data, struct wl_seat *wl_seat, const char *name) {

}

static struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name
};

/**
 * Get the global objects
 */
static void global_listener(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
    wwlWindow* window = data;
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        window->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
    } else if(strcmp(interface, wl_shm_interface.name) == 0) {
        window->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
    } else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
        window->wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, version);
    } else if(strcmp(interface, wl_seat_interface.name) == 0) {
        window->seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, version);
        wl_seat_add_listener(window->seat, &seat_listener, window);
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
    wwlWindow* window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    window->buffer = createFrame(window, window->content);
    wl_surface_attach(window->surface, window->buffer, 0, 0);
    wl_surface_commit(window->surface);
}

static struct xdg_surface_listener surface_listener = {
    surface_configure
};

/**
 * Gets called when the size of the window changes
 */
static void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    wwlWindow* window = data;
    if(width != 0 && height != 0) {
        window->width = width;
        window->height = height;
    }
}

/**
 * Gets called when the window should be closed
 */
static void toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    wwlWindow* window = data;
    window->running = 0;
}

static struct xdg_toplevel_listener toplevel_listener = {
    toplevel_configure,
    toplevel_close
};

static struct wl_callback_listener frame_listener;

static void frame_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data) {
    wwlWindow* window = data;
    
    wl_callback_destroy(wl_callback);
    wl_callback = wl_surface_frame(window->surface);
    wl_callback_add_listener(wl_callback, &frame_listener, window);

    if(window->damaged) {
        window->buffer = createFrame(window, window->content);
        wl_surface_attach(window->surface, window->buffer, 0, 0);
        wl_surface_damage_buffer(window->surface, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit(window->surface);
        window->damaged = 0;
    }
}

static struct wl_callback_listener frame_listener = {
    frame_done
};

/**
 * ==================================
 * API Section
 * ==================================
 */
wwlWindow* wwlCreateWindow(int width, int height, const char* title) {
    wwlWindow* window = malloc(sizeof(wwlWindow));
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

    struct wl_callback* cb = wl_surface_frame(window->surface);
    wl_callback_add_listener(cb, &frame_listener, window);

    window->keyboard_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    return window;
}

int wwlShouldClose(wwlWindow* window) {
    return wl_display_dispatch(window->display) == -1 || !window->running;
}

void wwlGetDimensions(wwlWindow* window, int* width, int* height) {
    *width = window->width;
    *height = window->height;
}

int wwlDraw(wwlWindow* window, uint32_t* content, int size) {
    if(window->width * window->height * 4 != size) {
        fprintf(stderr, "Size doesn't match\n");
        return -1;
    }
    window->content = content;
    window->damaged = 1;
    return 0;
}

void wwlSetTitle(wwlWindow* window, const char* title) {
    xdg_toplevel_set_title(window->toplevel, title);
    wl_surface_commit(window->surface);
}

void wwlSetKeyCallback(wwlWindow* window, void (*key_callback)(void* window, char* key, enum wwlKeyAction action)) {
    window->key_callback = key_callback;
}

void wwlGetCursorPos(wwlWindow* window, double* x, double* y) {
    *x = window->cursor_x;
    *y = window->cursor_y;
}

void wwlSetCursorCallback(wwlWindow* window, void (*cursor_callback)(void* window, double x, double y)) {
    window->cursor_callback = cursor_callback;
}

void wwlSetMouseButtonCallback(wwlWindow* window, void (*button_callback)(void* window, int button, enum wwlKeyAction action)) {
    window->button_callback = button_callback;
}

void wwlSetScrollCallback(wwlWindow* window, void (*scroll_callback)(void* window, double x_offset, double y_offset)) {
    window->scroll_callback = scroll_callback;
}

void wwlCloseWindow(wwlWindow* window) {
    window->running = 0;
    xdg_toplevel_destroy(window->toplevel);
    wl_surface_destroy(window->surface);
    free(window);
}
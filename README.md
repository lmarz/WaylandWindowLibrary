# WLENGINE
Simple window library for wayland with control over each pixel
## Requirements
- Running wayland server
- wayland development files
- wayland-protocols
- wayland-scanner
## Compiling
```
make
```
## Best Practices
Only use draw(), when the content or the size of the window has changed. See [test](test.c) for more details.
## LICENSE
This project is licensed under the MIT license. See [LICENSE](LICENSE) for more details.
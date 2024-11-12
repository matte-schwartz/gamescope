## Fedora
On Fedora, all build dependencies for the latest packaged version of Gamescope can be installed via `dnf` with `dnf builddep gamescope`. To manually install all dependencies, run the following command:

```
dnf install -y \
    meson \
    cmake \
    ninja-build \
    libX11-devel \
    libXdamage-devel \
    libXcomposite-devel \
    libXrender-devel \
    libXext-devel \
    libXxf86vm-devel \
    libXtst-devel \
    libXres-devel \
    libXcursor-devel \
    libXfixes-devel \
    libXmu-devel \
    libdrm-devel \
    luajit-devel \
    wayland-devel \
    wayland-protocols-devel \
    libxkbcommon-devel \
    libcap-devel \
    SDL2-devel \
    libdecor-devel \
    libdisplay-info-devel \
    libeis-devel \
    libliftoff-devel \
    pipewire-devel \
    vulkan-loader-devel \
    spirv-headers-devel \
    glslang \
    stb_image-devel \
    stb_image_resize-devel \
    stb_image_write-devel \
    glm-devel \
    libavif-devel \
    hwdata-devel
```
and then proceed with the build instructions in the project README.

## Arch Linux
On Arch Linux, all build dependencies for the latest packaged version of gamescope can be installed if you clone Arch's [PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/gamescope/-/blob/main/PKGBUILD?ref_type=heads) and use `makepkg -s --nobuild` to install the dependencies without building the package.
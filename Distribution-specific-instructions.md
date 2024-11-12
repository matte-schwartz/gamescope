## General Build Notes:


## Fedora
On Fedora, all build dependencies for the latest packaged version of Gamescope can be installed via `dnf` with `dnf builddep gamescope`. To manually build the project and install all dependencies, run the following command:

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

## Arch
On Arch, the easiest method to build gamescope yourself is with the [gamescope-git AUR package](https://aur.archlinux.org/packages/gamescope-git). This can be done either with an AUR helper which will install all the dependencies and build the package, or by cloning https://aur.archlinux.org/gamescope-git.git and running `makepkg -si`. To manually build the project and all dependencies, run the following command:

```
sudo pacman -S \
    gcc-libs \
    glibc \
    glm \
    hwdata \
    lcms2 \
    libavif \
    libcap \
    libdecor \
    libdrm \
    libinput \
    libpipewire \
    libx11 \
    libxcb \
    libxcomposite \
    libxdamage \
    libxext \
    libxfixes \
    libxkbcommon \
    libxmu \
    libxrender \
    libxres \
    libxtst \
    libxxf86vm \
    luajit \
    sdl2 \
    seatd \
    vulkan-icd-loader \
    wayland \
    xcb-util-errors \
    xcb-util-wm \
    xorg-server-xwayland \
    google-benchmark \
    cmake \
    git \
    glslang \
    meson \
    ninja \
    vulkan-headers \
    wayland-protocols
```
and then proceed with the build instructions in the project README.

## Ubuntu / Debian
**Please note: we only support gamescope built with up-to-date libs and gamescope's own subproject modules.**
On Ubuntu / Debian, 

# Define directories based on Meson options
bindir = get_option('bindir')
datadir = get_option('datadir')
libdir = get_option('libdir')
prefix = get_option('prefix')

# Define the systemd user unit directory
systemd_user_unit_dir = join_paths(prefix, libdir, 'systemd', 'user')

# Install binaries with executable permissions
install_data(
  [
    'gamescope-session',
    'start-gamescope-session',
    'steam-http-loader',
    'steamos-session-select',
  ],
  install_dir: bindir,
  install_mode: 'rwxr-xr-x',
)

# Install the Wayland session file
install_data(
  'gamescope-wayland.desktop',
  install_dir: join_paths(datadir, 'wayland-sessions'),
  install_mode: 'rw-r--r--',
)

# Install URL handling desktop files
install_data(
  [
    'steam_http_loader.desktop',
    'gamescope-mimeapps.list',
  ],
  install_dir: join_paths(datadir, 'applications'),
  install_mode: 'rw-r--r--',
)

# Install the systemd service file
install_data(
  'gamescope-session.service',
  install_dir: systemd_user_unit_dir,
  install_mode: 'rw-r--r--',
)

# Install portal configuration
install_data(
  'gamescope-portals.conf',
  install_dir: join_paths(datadir, 'xdg-desktop-portal'),
  install_mode: 'rw-r--r--',
)

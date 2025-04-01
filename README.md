# LightLimit

LightLimit is an advanced CPU management tool for Linux systems that allows you to control CPU usage limits, set processor affinities, and monitor system resources with an interactive interface.

![GitHub](https://img.shields.io/github/license/QKing-Official/lightlimit)

## Features

- **CPU Usage Limiting**: Cap total CPU usage for your system
- **Core Affinity Control**: Bind processes to specific CPU cores
- **Interactive Monitor**: Visual process manager with kill functionality
- **System Information**: View detailed CPU and memory statistics
- **CGroup Management**: Leverages Linux control groups for resource management
- **Cross-Distribution Support**: Works on Debian/Ubuntu, Fedora, Arch, and more

## Requirements

- Linux-based operating system
- Root privileges (for most functionality)
- GCC compiler
- ncurses development libraries

## Installation

### Quick Install

LightLimit includes an automated dependency detection and installation process:

```bash
git clone https://github.com/QKing-Official/lightlimit.git
cd lightlimit
make install-deps  # Install dependencies for your distribution
make               # Build the executable
make install       # Install to /usr/local/bin (optional)
```

### Manual Installation

If you prefer to install dependencies manually:

```bash
# Debian/Ubuntu
sudo apt-get install build-essential libncurses-dev

# Fedora/RHEL/CentOS
sudo dnf install gcc ncurses-devel

# Arch Linux
sudo pacman -S gcc ncurses

# Build and install
make
sudo make install  # Optional
```

## Usage

LightLimit provides several commands for managing CPU resources:

```
Usage: lightlimit COMMAND [ARGS]

Commands:
  total <cpu_percentage>   Sets total CPU limit for all processes (0-100%)
  preference <core_list>   Sets CPU affinity (e.g., '0,1,3')
  reset                    Resets CPU limits and cgroup
  info                     Displays CPU and memory info
  monitor                  Interactive process monitor with task management
  uninstall                Removes the cgroup that lightlimit creates
  help                     Displays this help message
```

### Examples

#### Limiting CPU Usage

Limit the system to use only 50% of available CPU resources:

```bash
sudo lightlimit total 50
```

#### Setting CPU Affinity

Restrict LightLimit to use only cores 0 and 1:

```bash
lightlimit preference 0,1
```

#### Process Monitoring

Launch the interactive process monitor:

```bash
lightlimit monitor
```

In the monitor interface:
- Use arrow keys to navigate the process list
- Press `k` to kill a selected process
- Press `q` to quit
- Press `r` to refresh manually

#### System Information

Display system CPU and memory information:

```bash
lightlimit info
```

#### Cleanup

Remove CPU limits and the cgroup created by LightLimit:

```bash
sudo lightlimit reset
```

Completely uninstall the tool:

```bash
sudo make uninstall
```

## How It Works

LightLimit uses Linux control groups (cgroups) to manage CPU resources. The tool creates a cgroup at `/sys/fs/cgroup/cpu/lightlimit` to control CPU quotas for processes.

The interactive monitor provides real-time information about running processes, including:
- Process ID (PID)
- CPU usage percentage
- Memory usage percentage
- Memory consumption in MB
- Process command name

## Distribution-Specific Notes

### Debian/Ubuntu
- Cgroups are usually available by default
- Make sure to run `make install-deps` or install `libncurses-dev` manually

### Fedora/RHEL/CentOS
- The cgroup filesystem might need to be mounted
- Requires the `ncurses-devel` package

### Arch Linux
- Install the `ncurses` package for the monitor functionality
- The cgroup filesystem should be available by default

## Troubleshooting

### Permission Denied

Most commands require root privileges. If you encounter permission errors, run the command with `sudo`.

### Cgroup Not Found

If you get errors about missing cgroup files, make sure your system has cgroups enabled and mounted:

```bash
# For older systems (v1 cgroups)
sudo mkdir -p /sys/fs/cgroup/cpu
sudo mount -t cgroup -o cpu,cpuacct none /sys/fs/cgroup/cpu

# For systemd-based systems with cgroup v2
sudo mkdir -p /sys/fs/cgroup/cpu
```

### No Color in Monitor Mode

If colors aren't displaying correctly:
- Ensure your terminal supports colors
- Try setting the `TERM` environment variable: `export TERM=xterm-256color`

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request to the [GitHub repository](https://github.com/QKing-Official/lightlimit).

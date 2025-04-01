CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lpthread -lncurses

# Detect the Linux distribution
DISTRO := $(shell if [ -f /etc/debian_version ]; then echo "debian"; \
		 elif [ -f /etc/fedora-release ]; then echo "fedora"; \
		 elif [ -f /etc/arch-release ]; then echo "arch"; \
		 else echo "unknown"; fi)

lightlimit: lightlimit.c
	$(CC) $(CFLAGS) -o lightlimit lightlimit.c $(LIBS)

install-deps:
ifeq ($(DISTRO),debian)
	sudo apt-get update && sudo apt-get install -y libncurses-dev
else ifeq ($(DISTRO),fedora)
	sudo dnf install -y ncurses-devel
else ifeq ($(DISTRO),arch)
	sudo pacman -S --noconfirm ncurses
else
	@echo "Unknown distribution. Please install ncurses development package manually."
endif

install: lightlimit
	sudo install -m 755 lightlimit /usr/local/bin/

uninstall:
	sudo rm -f /usr/local/bin/lightlimit
	-sudo rmdir /sys/fs/cgroup/cpu/lightlimit 2>/dev/null || true
	@echo "LightLimit has been uninstalled."

clean:
	rm -f lightlimit

help:
	@echo "Available make targets:"
	@echo "  make: Build the lightlimit executable."
	@echo "  make install-deps: Install required dependencies for your distribution."
	@echo "  make install: Install lightlimit to /usr/local/bin/."
	@echo "  make uninstall: Uninstall lightlimit and remove cgroup directory."
	@echo "  make clean: Remove the lightlimit executable."
	@echo "  make help: Display this help message."

.PHONY: install uninstall clean help install-deps

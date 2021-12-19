# Xiccd

A simple bridge between colord and X. It does the following tasks:

- Enumerates displays and register them in colord.
- Creates default ICC profiles based on EDID data.
- Applies ICC profiles provided by colord.
- Maintains user's private ICC storage directory.

The primary goal of xiccd is providing color profile support for
desktop environments other than Gnome and KDE (Xfce, LXDE, and probably
others) that don't support native color management yet.

It does basically the same as the `gnome-settings-daemon` color plugin
or `colord-kde` but doesn't depend on any particular desktop.
It doesn't even depend on GTK so it doesn't create a unnecessary GTK3
dependency if the desktop environment is GTK2-based or vice versa.

## Dependencies

Xiccd depends only on:

- GLib
- colord
- libxrandr

And to build, the following are required:

- Autoconf
- Automake
- Make
- Git

You can install all of the above with the commands:

```sh
# For Debian, Ubuntu, and derivatives:
apt install build-essential libglib2.0-dev libcolord-dev libxrandr-dev git

# For Arch, Manjaro, and derivatives:
pacman -S base-devel glib2 colord libxrandr git
```

## Installation

Xiccd uses Autotools to build, so if you've checked out the source code
with `git`, you'll probably need to generate the build scripts with the
following commands:

```sh
aclocal
autoconf
automake --add-missing --foreign
```

Once that's done, you'll be able to build and install the program with
the traditional Make commands:

```sh
./configure
make
make install
```

which will install Xiccd into `/usr/local` by default.
The usual conventions (`PREFIX`, `DESTDIR`, etc.) are respected.

# Emacs supporting pure-gtk3

In this fork, I'm working to make Emacs support pure-gtk3, in order to support Wayland.

## Building

You need cairo.

```sh
git clone https://github.com/masm11/emacs.git
cd emacs
./autogen.sh
./configure --without-x --with-cairo --with-modules
make
```

Ignore warnings.

#### For archlinux users

This repository contains PKGBUILD, so you can do to install:

```
mkdir /tmp/emacs
cd /tmp/emacs
wget https://raw.githubusercontent.com/masm11/emacs/master/PKGBUILD
makepkg -s
sudo pacman -U emacs-pgtk*.tar.xz
```

## Running

```sh
cd src
GDK_BACKEND=wayland ./emacs  (or try ./emacs -Q if you have problems.)
```

## X11 and Wayland

Of course, PGTK supports X11 and Wayland connections.

You can use `GDK_BACKEND` environment variable and `--display` option,
and you can do `(make-frame-on-display display-name)` with display-name of
different backend from the first frame.

You can know which backend is used for a frame:

```elisp
(pgtk-backend-display-class)
```

This returns `"GdkWaylandDisplay"` for Wayland, or `"GdkX11Display"` for X11.

Note: Segmentation fault may occur on multiple display environment.

## TODO

Known problems:
- Segmentation fault while multiple-display.
- Exits when a connection to display server is closed by peer. (I may not be able to resolve.)

Not implemented:
- GTK on wayland does not implement functions for these features:
  - x_set_offset
  - x_set_no_focus_on_map
  - x_set_no_accept_focus
  - x_set_z_group
  - auto-raise/lower
- Some other features. Keywords:
  - popup_activated
  - vendor_specific_keysyms
  - x_find_modifier_meanings
  - x_load_resources
  - dnd
  - gtk_plug (not exists on wayland)
  - x_set_parent_frame
  - frame_x_embedded_p

I may not develop them because I don't use them.

## Debugging

Edit src/pgtkterm.h to uncomment:

```c
#define PGTK_DEBUG 1
```

It enables so much debugging outputs.

On gdb, you may want to do:

```
(gdb) handle SIGPIPE nostop noprint
```

## Supporting Color Emoji Fonts

By default, color fonts are ignored.

To use them, you can write this code in `~/.emacs`:

```elisp
(when (featurep 'pgtk)
  (setq xft-ignore-color-fonts nil))
```

Changing this variable on the fly may not work.

You may need build and install cairo from git repo. 1.15.12 is insufficient.

## My Environment

- archlinux
- gtk+ 3.24.8
- glib2 2.60.4
- gnome-shell 3.32.2
- gcc 8.3.0
- wayland 1.17.0
- wayland-protocols 1.17
- cairo 1.16.0
- freetype2 2.10.0
- imagemagick 7.0.8.49

## Notice

- This code is NOT completely pure.

  It uses backend-specific functions to obtain the socket file
  descriptor to the display server. It supports X11 and Wayland.

- Commit messages are in Japanese.

## About me

masm11.

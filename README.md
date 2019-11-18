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
wget https://raw.githubusercontent.com/masm11/emacs/pgtk/PKGBUILD
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

## Instead of xrdb

X has the resource database, and you could store initial default values into it.

Gtk/Gdk can't handle it even if on X11, so I implemented similar feature using gsettings.

Saving:
```elisp
(pgtk-set-resource "background" "gray")
```

Getting:
```elisp
(x-get-resource "background" "Background")
```

If your emacs got failing to start, then edit your settings with `dconf-editor`.
Your settings are saved under `/org/gnu/emacs/defaults-by-name/<instance-name>/` and
`/org/gnu/emacs/defaults-by-class/`. All are of string type.
Correct your mistakes.

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
- GTK does not implement functions for these features:
  - vendor_specific_keysyms
- Some other features. Keywords:
  - popup_activated
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

You need cairo 1.16.0, or greater.

## Input Methods

```elisp
(when (eq window-system 'pgtk)
  (pgtk-use-im-context t))
```

This enables Gtk's `GtkIMContext`.

However, when you type e.g. `C-x o`,
`C-x` goes through input methods and is handled by Emacs,
and `o` is handled by input methods, so `お` appears as a preedit text.
I have no idea. You can turn off input method before typing that.
I do, so no problem.

## My Environment

- archlinux
- gtk+ 3.24.12
- glib2 2.62.2
- gcc 9.2.0
- wayland 1.17.0
- wayland-protocols 1.18
- cairo 1.17.2
- freetype2 2.10.1
- imagemagick 7.0.9.2
- ibus 1.5.21
- mozc 2.23.2815.102
- wayfire 0.3

## Notice

- This code is NOT completely pure.

  It uses backend-specific functions to obtain the socket file
  descriptor to the display server. It supports X11 and Wayland.

- Commit messages are in Japanese.

## About me

masm11.

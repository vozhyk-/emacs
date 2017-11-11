# Emacs supporting pure-gtk3

In this fork, I'm working to make Emacs support pure-gtk3, in order to support Wayland.

Maybe It's only for Wayland now.

The work is in progress.

## Building

You need cairo.

```
git clone https://github.com/masm11/emacs.git
cd emacs
./autogen.sh
./configure --without-x --with-cairo
make
```

Ignore many warnings.

## Running

You maybe need `Noto Sans Mono CJK JP` fonts.

```
cd src
./emacs  (or ./emacs -Q if you have problems.)
```

Many many debugging outputs. Please ignore them.

## Status

- character displaying
  - ascii characters can be displayed.
  - Japanese characters can be displayed.
  - characters already displayed are not cleared.
  - You need to resize the window to get correct faces.
  - mode-line is normal face...

- keyboard
  - You can type characters, and they are displayed.
  - C-x C-f works.

- frame
  - resizable by dragging window borders.
  - no scrollbar
  - no toolbar
  - no menubar
  - fringes are displayed, but face is not correct.
  - C-x 5 2 doesn't work.
  - fullscreen not supported.

- command line options
  - `-fn`, `-bg`, and `-fg` are available.

- On pure gtk+-3 with X11
  - maybe doesn't work.

- other features
  - so many features not supported.

## My Environment

- archlinux
- gtk+-3.22.26
- glib-2.54.2
- gnome-shell 3.26.2
- gcc 7.2.0
- wayland 1.14.0
- wayland-protocols 1.11

## Notice

Commit messages and comments in codes are in Japanese.

## About me

masm11.

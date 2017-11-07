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
./emacs  (or ./emacs -Q if you have troubles.)
```

Many many debugging outputs. Please ignore them.

## Status

- character displaying
  - ascii characters can be displayed.
  - unknown about Japanese characters.
  - characters already displayed are not cleared.
  - You need to resize the window to get correct faces.
  - mode-line is normal face...

- keyboard
  - You can type, and the character is displayed.
  - C-x C-f works.

- frame
  - You can resize it by dragging window frames.
  - no scrollbar
  - no toolbar
  - no menubar
  - no fringe
  - C-x 5 2, I don't know.
  - fullscreen not supported.

- command line options
  - `-fn`, `-bg`, and `-fg` are available.

- other features
  - so many features not supported.

- bugs
  - when scrolling, segv occurs.

## My Environment

- archlinux
- gtk+-3.22.5
- glib-2.54.0
- gnome-shell 3.26.1
- gcc 7.2.0
- wayland 1.14.0
- wayland-protocols 1.11

## Notice

Commit messages and comments in codes are in Japanese.

## About me

masm11.

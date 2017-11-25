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

## TODO

- cursor doesn't blink.
- face of mode-line doesn't change.
- no scrollbar.
- no toolbar.
- no menubar.
- no visual-bell.
- no visible-bell.
- no mouse events.
- sometimes cursor are left at the end of a line.
- fullscreen not supported.
- C-x 5 0 doesn't work.
- maybe doesn't work on pure gtk+-3 with X11.
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

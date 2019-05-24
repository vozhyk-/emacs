/* image support for pure Gtk+3.
   Copyright (C) 1989, 1993, 2005, 2008-2017 Free Software Foundation,
   Inc.

This file is part of GNU Emacs.

GNU Emacs is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs.  If not, see <https://www.gnu.org/licenses/>.  */

#ifdef HAVE_PGTK

extern Emacs_Pixmap pgtk_image_create(int width, int height, int depth);
extern void pgtk_image_destroy(Emacs_Pixmap pixmap);
extern unsigned long pgtk_image_get_pixel(Emacs_Pixmap pixmap, int x, int y);
extern void pgtk_image_put_pixel(Emacs_Pixmap pixmap, int x, int y, unsigned long pixel);
extern Emacs_Pixmap pgtk_image_create_from_xbm(char *bits, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg);
#if 0
extern Emacs_Pixmap pgtk_image_convert_from_xbm(char *bits, unsigned int width, unsigned int height);
#endif
extern void pgtk_image_set_alpha(Emacs_Pixmap pixmap, int x, int y, int alpha);

#endif

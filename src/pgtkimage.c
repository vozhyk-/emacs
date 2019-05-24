/* Image support for pure gtk-3.0

Copyright (C) 1989, 1992-2018 Free Software Foundation, Inc.

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

#include <config.h>
#include "pgtkterm.h"
#include "pgtkimage.h"

struct pgtk_image_t {
  unsigned char *data;
  int width, height;
  int depth;
};

Emacs_Pixmap pgtk_image_create(int width, int height, int depth)
{
  struct pgtk_image_t *img;
  if ((img = xmalloc(sizeof *img)) == NULL)
    return NULL;
  if ((img->data = xmalloc(width * height * 4)) == NULL) {
    xfree(img);
    return NULL;
  }
  img->width = width;
  img->height = height;
  img->depth = depth;
  return img;
}

void pgtk_image_destroy(Emacs_Pixmap pixmap)
{
  struct pgtk_image_t *img = pixmap;
  xfree(img->data);
  xfree(img);
}

unsigned long pgtk_image_get_pixel(Emacs_Pixmap pixmap, int x, int y)
{
  struct pgtk_image_t *img = pixmap;
  if (x < 0 || x >= img->width)
    return 0;
  if (y < 0 || y >= img->height)
    return 0;
  unsigned char *p = img->data + img->width * y * 4 + x * 4;
  PGTK_TRACE("get_pixel(%d,%d)=%08x.", x, y, (unsigned int) (p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0]));
  return p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];   // little endian.
}

void pgtk_image_put_pixel(Emacs_Pixmap pixmap, int x, int y, unsigned long pixel)
{
  struct pgtk_image_t *img = pixmap;
  if (x < 0 || x >= img->width)
    return;
  if (y < 0 || y >= img->height)
    return;
  PGTK_TRACE("put_pixel: (%d,%d)=%08lx.", x, y, pixel);
  unsigned char *p = img->data + img->width * y * 4 + x * 4;
  p[0] = pixel >> 0;
  p[1] = pixel >> 8;
  p[2] = pixel >> 16;
  p[3] = pixel >> 24;
}

Emacs_Pixmap pgtk_image_create_from_xbm(char *bits, unsigned int width, unsigned int height, unsigned long fg, unsigned long bg)
{
  PGTK_TRACE("create_from_xbm: 0");
  struct pgtk_image_t *img = pgtk_image_create(width, height, 32);
  char *p = bits;
  int mask = 0x80;
  if (img == NULL)
    return NULL;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      pgtk_image_put_pixel(img, x, y, (*p & mask) ? fg : bg);
      if ((mask >>= 1) == 0) {
	p++;
	mask = 0x80;
      }
    }
    if (mask != 0x80) {
      p++;
      mask = 0x80;
    }
  }
  return img;
}

#if 0
Emacs_Pixmap pgtk_image_convert_from_xbm(char *bits, unsigned int width, unsigned int height)
{
  struct pgtk_image_t *img = pgtk_image_create(width, height, 1);
  char *p = bits;
  int mask = 0x80;
  if (img == NULL)
    return NULL;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      pgtk_image_put_pixel(img, x, y, (*p & mask) ? 0xffffffff : 0);
      if ((mask >>= 1) == 0) {
	p++;
	mask = 0x80;
      }
    }
    if (mask != 0x80) {
      p++;
      mask = 0x80;
    }
  }
  return img;
}
#endif

void pgtk_image_set_alpha(Emacs_Pixmap pixmap, int x, int y, int alpha)
{
  PGTK_TRACE("set_alpha: (%d,%d)=%d.", x, y, alpha);
  struct pgtk_image_t *img = pixmap;
  if (x < 0 || x >= img->width)
    return;
  if (y < 0 || y >= img->height)
    return;
  unsigned char *p = img->data + img->width * y * 4 + x * 4;
  p[3] = alpha;
}

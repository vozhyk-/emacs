/* Pure Gtk+-3 communication module.      -*- coding: utf-8 -*-

Copyright (C) 1989, 1993-1994, 2005-2006, 2008-2018 Free Software
Foundation, Inc.

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

/* This should be the first include, as it may set up #defines affecting
   interpretation of even the system includes. */
#include <config.h>

#include "pgtkterm.h"

static void im_context_commit_cb(GtkIMContext *imc, gchar *str, gpointer user_data)
{
  struct frame *f = user_data;
  pgtk_enqueue_string(f, str);
}

static gboolean im_context_retrieve_surrounding_cb(GtkIMContext *imc, gpointer user_data)
{
  gtk_im_context_set_surrounding(imc, "", -1, 0);
  return TRUE;
}

static gboolean im_context_delete_surrounding_cb(GtkIMContext *imc, int offset, int n_chars, gpointer user_data)
{
  return TRUE;
}

static void im_context_preedit_changed_cb(GtkIMContext *imc, gpointer user_data)
{
  struct frame *f = user_data;
  char *str;
  PangoAttrList *attrs;
  int pos;

  gtk_im_context_get_preedit_string(imc, &str, &attrs, &pos);

  /* get size */
  PangoLayout *layout = gtk_widget_create_pango_layout(FRAME_GTK_WIDGET(f), str);
  pango_layout_set_attributes(layout, attrs);
  int width, height;
  pango_layout_get_pixel_size(layout, &width, &height);

  Lisp_Object image_data;
  if (width != 0 && height != 0) {
    char *buf = g_new0(char, 5 + 20 + 20 + 10 + 3 * width * height);
    sprintf(buf, "P6\n%d %d\n255\n", width, height);

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
    unsigned char *crbuf = g_new0(unsigned char, stride * height);
    cairo_surface_t *surface = cairo_image_surface_create_for_data(crbuf, CAIRO_FORMAT_RGB24, width, height, stride);

    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);

    unsigned char *sp = crbuf;
    unsigned char *dp = (unsigned char *) buf + strlen(buf);
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
	unsigned int rgb = *(unsigned int *) sp;
	*dp++ = (rgb >> 16) & 0xff;
	*dp++ = (rgb >>  8) & 0xff;
	*dp++ = (rgb >>  0) & 0xff;
	sp += 4;
      }
    }

    image_data = make_unibyte_string(buf, dp - (unsigned char *) buf);

    g_free(crbuf);
    g_free(buf);
  } else
    image_data = Qnil;

  call1(Qpgtk_refresh_preedit, image_data);
  SET_FRAME_GARBAGED (f);

  if (FRAME_X_OUTPUT(f)->im.preedit_str != NULL)
    g_free(FRAME_X_OUTPUT(f)->im.preedit_str);
  FRAME_X_OUTPUT(f)->im.preedit_str = str;

  if (FRAME_X_OUTPUT(f)->im.preedit_attrs != NULL)
    pango_attr_list_unref(FRAME_X_OUTPUT(f)->im.preedit_attrs);
  FRAME_X_OUTPUT(f)->im.preedit_attrs = attrs;
}

static void im_context_preedit_end_cb(GtkIMContext *imc, gpointer user_data)
{
  struct frame *f = user_data;

  call1(Qpgtk_refresh_preedit, Qnil);
  SET_FRAME_GARBAGED (f);

  if (FRAME_X_OUTPUT(f)->im.preedit_str != NULL)
    g_free(FRAME_X_OUTPUT(f)->im.preedit_str);
  FRAME_X_OUTPUT(f)->im.preedit_str = NULL;

  if (FRAME_X_OUTPUT(f)->im.preedit_attrs != NULL)
    pango_attr_list_unref(FRAME_X_OUTPUT(f)->im.preedit_attrs);
  FRAME_X_OUTPUT(f)->im.preedit_attrs = NULL;
}

static void im_context_preedit_start_cb(GtkIMContext *imc, gpointer user_data)
{
}

void pgtk_im_focus_in(struct frame *f)
{
  gtk_im_context_focus_in (FRAME_X_OUTPUT (f)->im.context);
}

void pgtk_im_focus_out(struct frame *f)
{
  gtk_im_context_focus_out (FRAME_X_OUTPUT (f)->im.context);
}

void pgtk_im_init(struct frame *f)
{
  FRAME_X_OUTPUT(f)->im.preedit_str = NULL;
  FRAME_X_OUTPUT(f)->im.preedit_attrs = NULL;

  FRAME_X_OUTPUT(f)->im.context = gtk_im_multicontext_new();
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "commit", G_CALLBACK(im_context_commit_cb), f);
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "retrieve-surrounding", G_CALLBACK(im_context_retrieve_surrounding_cb), f);
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "delete-surrounding", G_CALLBACK(im_context_delete_surrounding_cb), f);
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "preedit-changed", G_CALLBACK(im_context_preedit_changed_cb), f);
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "preedit-end", G_CALLBACK(im_context_preedit_end_cb), f);
  g_signal_connect(FRAME_X_OUTPUT (f)->im.context, "preedit-start", G_CALLBACK(im_context_preedit_start_cb), f);
  gtk_im_context_set_use_preedit (FRAME_X_OUTPUT (f)->im.context, TRUE);
  gtk_im_context_set_client_window (FRAME_X_OUTPUT (f)->im.context, gtk_widget_get_window(FRAME_GTK_WIDGET (f)));
}

void pgtk_im_finish(struct frame *f)
{
  g_object_unref(FRAME_X_OUTPUT(f)->im.context);
  FRAME_X_OUTPUT(f)->im.context = NULL;

  if (FRAME_X_OUTPUT(f)->im.preedit_str != NULL)
    g_free(FRAME_X_OUTPUT(f)->im.preedit_str);
  FRAME_X_OUTPUT(f)->im.preedit_str = NULL;

  if (FRAME_X_OUTPUT(f)->im.preedit_attrs != NULL)
    pango_attr_list_unref(FRAME_X_OUTPUT(f)->im.preedit_attrs);
  FRAME_X_OUTPUT(f)->im.preedit_attrs = NULL;
}

DEFUN ("pgtk-use-im-context", Fpgtk_use_im_context, Spgtk_use_im_context,
       1, 2, 0,
       doc: /* Set whether use Gtk's im context. */)
  (Lisp_Object use_p, Lisp_Object terminal)
{
  struct pgtk_display_info *dpyinfo = check_pgtk_display_info (terminal);

  dpyinfo->use_im_context = !NILP(use_p);

  return Qnil;
}



void
syms_of_pgtkim (void)
{
  defsubr (&Spgtk_use_im_context);

  DEFSYM (Qpgtk_refresh_preedit, "pgtk-refresh-preedit");
}

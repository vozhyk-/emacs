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

static void adjust_preeditarea(struct frame *f);

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

static gboolean preedit_window_draw_cb(GtkWidget *w, cairo_t *cr, gpointer user_data)
{
  struct frame *f = user_data;
  PangoLayout *layout = gtk_widget_create_pango_layout(w, FRAME_X_OUTPUT(f)->im.preedit_str);
  pango_layout_set_attributes(layout, FRAME_X_OUTPUT(f)->im.preedit_attrs);
  int width, height;
  pango_layout_get_pixel_size(layout, &width, &height);

  cairo_save(cr);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 1, 1, 1);
  pango_cairo_update_layout(cr, layout);
  pango_cairo_show_layout(cr, layout);

  cairo_restore(cr);

  g_object_unref(layout);

  return TRUE;
}

static void im_context_preedit_changed_cb(GtkIMContext *imc, gpointer user_data)
{
  struct frame *f = user_data;
  char *str;
  PangoAttrList *attrs;
  int pos;

  gtk_im_context_get_preedit_string(imc, &str, &attrs, &pos);

  if (strlen (str) == 0)
    gtk_widget_hide(FRAME_X_OUTPUT(f)->im.drawing_area);
  else
    gtk_widget_show(FRAME_X_OUTPUT(f)->im.drawing_area);

  /* get size */
  PangoLayout *layout = gtk_widget_create_pango_layout(FRAME_X_OUTPUT(f)->im.drawing_area, str);
  pango_layout_set_attributes(layout, attrs);
  int width, height;
  pango_layout_get_pixel_size(layout, &width, &height);
  gtk_widget_set_size_request(FRAME_X_OUTPUT(f)->im.drawing_area, width, height);
  adjust_preeditarea(f);
  gtk_widget_queue_draw(FRAME_X_OUTPUT(f)->im.drawing_area);
  g_object_unref(layout);

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
  gtk_widget_hide(FRAME_X_OUTPUT(f)->im.drawing_area);
}

static void im_context_preedit_start_cb(GtkIMContext *imc, gpointer user_data)
{
}

static void adjust_preeditarea(struct frame *f)
{
  int pex = FRAME_X_OUTPUT(f)->im.preedit_x;
  int pey = FRAME_X_OUTPUT(f)->im.preedit_y;
  int pew, peh;
  gtk_widget_get_size_request(FRAME_X_OUTPUT(f)->im.drawing_area, &pew, &peh);
  pex = min(pex, FRAME_X_OUTPUT(f)->im.preedit_max_x - pew);
  pey = min(pey, FRAME_X_OUTPUT(f)->im.preedit_max_y - peh);
  pex = max(pex, FRAME_X_OUTPUT(f)->im.preedit_min_x);
  pey = max(pey, FRAME_X_OUTPUT(f)->im.preedit_min_y);
  gtk_fixed_move(GTK_FIXED(FRAME_GTK_WIDGET(f)), FRAME_X_OUTPUT(f)->im.drawing_area, pex, pey);
  gdk_window_raise(gtk_widget_get_window(FRAME_GTK_WIDGET(f)));
}

void pgtk_im_focus_in(struct frame *f)
{
  gtk_im_context_focus_in (FRAME_X_OUTPUT (f)->im.context);
}

void pgtk_im_focus_out(struct frame *f)
{
  gtk_im_context_focus_out (FRAME_X_OUTPUT (f)->im.context);
}

void pgtk_im_set_preeditarea(struct window *w, int x, int y)
{
  struct frame *f = XFRAME (w->frame);

  FRAME_X_OUTPUT(f)->im.preedit_x = WINDOW_TO_FRAME_PIXEL_X (w, x) + WINDOW_LEFT_FRINGE_WIDTH (w);
  FRAME_X_OUTPUT(f)->im.preedit_y = WINDOW_TO_FRAME_PIXEL_Y (w, y) + FONT_BASE (FRAME_FONT (f));

  FRAME_X_OUTPUT(f)->im.preedit_min_x = WINDOW_BOX_LEFT_EDGE_X(w) + WINDOW_LEFT_FRINGE_WIDTH(w);
  FRAME_X_OUTPUT(f)->im.preedit_max_x = WINDOW_BOX_RIGHT_PIXEL_EDGE(w) - WINDOW_RIGHT_FRINGE_WIDTH(w);

  FRAME_X_OUTPUT(f)->im.preedit_min_y = WINDOW_TOP_PIXEL_EDGE(w);
  FRAME_X_OUTPUT(f)->im.preedit_max_y = WINDOW_BOTTOM_PIXEL_EDGE(w);

  adjust_preeditarea(f);
}

gboolean pgtk_im_filter_keypress(struct frame *f, GdkEvent *ev)
{
  if (FRAME_DISPLAY_INFO (f)->use_im_context) {
    if (gtk_im_context_filter_keypress (FRAME_X_OUTPUT (f)->im.context, ev))
      return TRUE;
  }
  return FALSE;
}

void pgtk_im_init(struct frame *f)
{
  FRAME_X_OUTPUT(f)->im.preedit_str = NULL;
  FRAME_X_OUTPUT(f)->im.preedit_attrs = NULL;

  FRAME_X_OUTPUT(f)->im.drawing_area = gtk_drawing_area_new();
  g_signal_connect(FRAME_X_OUTPUT(f)->im.drawing_area, "draw", G_CALLBACK(preedit_window_draw_cb), f);
  gtk_widget_set_size_request(FRAME_X_OUTPUT(f)->im.drawing_area, 1, 1);
  gtk_fixed_put(GTK_FIXED(FRAME_GTK_WIDGET(f)), FRAME_X_OUTPUT(f)->im.drawing_area, 0, 0);

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

  gtk_widget_destroy(FRAME_X_OUTPUT(f)->im.drawing_area);
  FRAME_X_OUTPUT(f)->im.drawing_area = NULL;
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
}

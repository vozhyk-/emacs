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
  struct pgtk_display_info *dpyinfo = user_data;
  struct frame *f = dpyinfo->im.focused_frame;

  if (dpyinfo->im.context == NULL)
    return;
  if (f == NULL)
    return;

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
  struct pgtk_display_info *dpyinfo = user_data;
  struct frame *f = dpyinfo->im.focused_frame;
  char *str;
  PangoAttrList *attrs;
  int pos;

  if (dpyinfo->im.context == NULL)
    return;
  if (f == NULL)
    return;

  gtk_im_context_get_preedit_string(imc, &str, &attrs, &pos);


  /*
   * (
   *   (TEXT (ul . COLOR) (bg . COLOR) (fg . COLOR))
   *   ...
   * )
   */
  Lisp_Object list = Qnil;

  PangoAttrIterator* iter;
  iter = pango_attr_list_get_iterator(attrs);
  do {
    int st, ed;
    int has_underline = 0;
    Lisp_Object part = Qnil;

    pango_attr_iterator_range(iter, &st, &ed);

    if (ed > strlen(str))
      ed = strlen(str);
    if (st >= ed)
      continue;

    Lisp_Object text = make_string(str + st, ed - st);
    part = Fcons(text, part);

    PangoAttrInt *ul = (PangoAttrInt *) pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);
    if (ul != NULL) {
      if (ul->value != PANGO_UNDERLINE_NONE)
	has_underline = 1;
    }

    if (has_underline) {
      PangoAttrColor *ulc = (PangoAttrColor *) pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE_COLOR);
      if (ulc != NULL) {
	char *str = g_strdup_printf("#%02x%02x%02x",
				    ulc->color.red >> 8,
				    ulc->color.green >> 8,
				    ulc->color.blue >> 8);
	part = Fcons(Fcons(intern("ul"), make_string(str, strlen(str))), part);
	g_free(str);
      } else {
	part = Fcons(Fcons(intern("ul"), Qt), part);
      }
    }

    PangoAttrColor *fore = (PangoAttrColor *) pango_attr_iterator_get(iter, PANGO_ATTR_FOREGROUND);
    if (fore != NULL) {
      char *str = g_strdup_printf("#%02x%02x%02x",
				  fore->color.red >> 8,
				  fore->color.green >> 8,
				  fore->color.blue >> 8);
      part = Fcons(Fcons(intern("fg"), make_string(str, strlen(str))), part);
      g_free(str);
    }

    PangoAttrColor *back = (PangoAttrColor *) pango_attr_iterator_get(iter, PANGO_ATTR_BACKGROUND);
    if (back != NULL) {
      char *str = g_strdup_printf("#%02x%02x%02x",
				  back->color.red >> 8,
				  back->color.green >> 8,
				  back->color.blue >> 8);
      part = Fcons(Fcons(intern("bg"), make_string(str, strlen(str))), part);
      g_free(str);
    }

    part = Freverse(part);
    list = Fcons(part, list);
  } while (pango_attr_iterator_next(iter));

  list = Freverse(list);
  pgtk_enqueue_preedit(f, list);

  if (dpyinfo->im.preedit_str != NULL)
    g_free(dpyinfo->im.preedit_str);
  dpyinfo->im.preedit_str = str;

  if (dpyinfo->im.preedit_attrs != NULL)
    pango_attr_list_unref(dpyinfo->im.preedit_attrs);
  dpyinfo->im.preedit_attrs = attrs;
}

static void im_context_preedit_end_cb(GtkIMContext *imc, gpointer user_data)
{
  struct pgtk_display_info *dpyinfo = user_data;
  struct frame *f = dpyinfo->im.focused_frame;

  if (dpyinfo->im.context == NULL)
    return;
  if (f == NULL)
    return;

  pgtk_enqueue_preedit(f, Qnil);

  if (dpyinfo->im.preedit_str != NULL)
    g_free(dpyinfo->im.preedit_str);
  dpyinfo->im.preedit_str = NULL;

  if (dpyinfo->im.preedit_attrs != NULL)
    pango_attr_list_unref(dpyinfo->im.preedit_attrs);
  dpyinfo->im.preedit_attrs = NULL;
}

static void im_context_preedit_start_cb(GtkIMContext *imc, gpointer user_data)
{
}

void pgtk_im_focus_in(struct frame *f)
{
  struct pgtk_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);
  if (dpyinfo->im.context != NULL) {
    gtk_im_context_reset (dpyinfo->im.context);
    gtk_im_context_set_client_window (dpyinfo->im.context, gtk_widget_get_window (FRAME_GTK_WIDGET (f)));
    gtk_im_context_focus_in (dpyinfo->im.context);
  }
  dpyinfo->im.focused_frame = f;
}

void pgtk_im_focus_out(struct frame *f)
{
  struct pgtk_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);
  if (dpyinfo->im.focused_frame == f) {
    if (dpyinfo->im.context != NULL) {
      gtk_im_context_reset (dpyinfo->im.context);
      gtk_im_context_focus_out (dpyinfo->im.context);
      gtk_im_context_set_client_window (dpyinfo->im.context, NULL);
    }
    dpyinfo->im.focused_frame = NULL;
  }
}

void pgtk_im_init(struct pgtk_display_info *dpyinfo)
{
  dpyinfo->im.preedit_str = NULL;
  dpyinfo->im.preedit_attrs = NULL;
  dpyinfo->im.context = NULL;
}

void pgtk_im_finish(struct pgtk_display_info *dpyinfo)
{
  if (dpyinfo->im.context != NULL)
    g_object_unref(dpyinfo->im.context);
  dpyinfo->im.context = NULL;

  if (dpyinfo->im.preedit_str != NULL)
    g_free(dpyinfo->im.preedit_str);
  dpyinfo->im.preedit_str = NULL;

  if (dpyinfo->im.preedit_attrs != NULL)
    pango_attr_list_unref(dpyinfo->im.preedit_attrs);
  dpyinfo->im.preedit_attrs = NULL;
}

DEFUN ("pgtk-use-im-context", Fpgtk_use_im_context, Spgtk_use_im_context,
       1, 2, 0,
       doc: /* Set whether use Gtk's im context. */)
  (Lisp_Object use_p, Lisp_Object terminal)
{
  struct pgtk_display_info *dpyinfo = check_pgtk_display_info (terminal);

  if (NILP(use_p)) {
    if (dpyinfo->im.context != NULL) {
      gtk_im_context_reset (dpyinfo->im.context);
      gtk_im_context_focus_out (dpyinfo->im.context);
      gtk_im_context_set_client_window (dpyinfo->im.context, NULL);

      g_object_unref(dpyinfo->im.context);
      dpyinfo->im.context = NULL;

      if (dpyinfo->im.preedit_str != NULL)
	g_free(dpyinfo->im.preedit_str);
      dpyinfo->im.preedit_str = NULL;

      if (dpyinfo->im.preedit_attrs != NULL)
	pango_attr_list_unref(dpyinfo->im.preedit_attrs);
      dpyinfo->im.preedit_attrs = NULL;
    }
  } else {
    if (dpyinfo->im.context == NULL) {
      dpyinfo->im.preedit_str = NULL;
      dpyinfo->im.preedit_attrs = NULL;

      dpyinfo->im.context = gtk_im_multicontext_new();
      g_signal_connect(dpyinfo->im.context, "commit", G_CALLBACK(im_context_commit_cb), dpyinfo);
      g_signal_connect(dpyinfo->im.context, "retrieve-surrounding", G_CALLBACK(im_context_retrieve_surrounding_cb), dpyinfo);
      g_signal_connect(dpyinfo->im.context, "delete-surrounding", G_CALLBACK(im_context_delete_surrounding_cb), dpyinfo);
      g_signal_connect(dpyinfo->im.context, "preedit-changed", G_CALLBACK(im_context_preedit_changed_cb), dpyinfo);
      g_signal_connect(dpyinfo->im.context, "preedit-end", G_CALLBACK(im_context_preedit_end_cb), dpyinfo);
      g_signal_connect(dpyinfo->im.context, "preedit-start", G_CALLBACK(im_context_preedit_start_cb), dpyinfo);
      gtk_im_context_set_use_preedit (dpyinfo->im.context, TRUE);

      if (dpyinfo->im.focused_frame)
	pgtk_im_focus_in(dpyinfo->im.focused_frame);
    }
  }

  return Qnil;
}

void
syms_of_pgtkim (void)
{
  defsubr (&Spgtk_use_im_context);

  DEFSYM (Qpgtk_refresh_preedit, "pgtk-refresh-preedit");
}

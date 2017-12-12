/* Pure Gtk+-3 communication module.      -*- coding: utf-8 -*-

Copyright (C) 1989, 1993-1994, 2005-2006, 2008-2017 Free Software
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

#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <c-ctype.h>
#include <c-strcase.h>
#include <ftoastr.h>

#include "lisp.h"
#include "blockinput.h"
#include "sysselect.h"
#include "gtkutil.h"
#include "systime.h"
#include "character.h"
#include "fontset.h"
#include "composite.h"
#include "ccl.h"

#include "termhooks.h"
#include "termchar.h"
#include "menu.h"
#include "window.h"
#include "keyboard.h"
#include "buffer.h"
#include "font.h"
#include "xsettings.h"

#define STORE_KEYSYM_FOR_DEBUG(keysym) ((void)0)

#define FRAME_CR_CONTEXT(f)	((f)->output_data.pgtk->cr_context)
#define FRAME_CR_SURFACE(f)	((f)->output_data.pgtk->cr_surface)

struct pgtk_display_info *x_display_list; /* Chain of existing displays */
static int selfds[2] = { -1, -1 };
static pthread_mutex_t select_mutex;
static fd_set select_readfds, select_writefds;

/* Non-zero timeout value means ignore next mouse click if it arrives
   before that timeout elapses (i.e. as part of the same sequence of
   events resulting from clicking on a frame to select it).  */

static Time ignore_next_mouse_click_timeout;

static void pgtk_clear_frame_area(struct frame *f, int x, int y, int width, int height);
static void pgtk_fill_rectangle(struct frame *f, unsigned long color, int x, int y, int width, int height);
static void pgtk_clip_to_row (struct window *w, struct glyph_row *row,
				enum glyph_row_area area, cairo_t *cr);
static void pgtk_cr_destroy_surface(struct frame *f);

char *
x_get_keysym_name (int keysym)
/* --------------------------------------------------------------------------
    Called by keyboard.c.  Not sure if the return val is important, except
    that it be unique.
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_get_ksysym_name");
  static char value[16];
  sprintf (value, "%d", keysym);
  return value;
}

void
frame_set_mouse_pixel_position (struct frame *f, int pix_x, int pix_y)
/* --------------------------------------------------------------------------
     Programmatically reposition mouse pointer in pixel coordinates
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("frame_set_mouse_pixel_position");
}

/* Free X resources of frame F.  */

void
x_free_frame_resources (struct frame *f)
{
  struct pgtk_display_info *dpyinfo;
  Mouse_HLInfo *hlinfo;

  PGTK_TRACE ("x_free_frame_resources");
  check_window_system (f);
  dpyinfo = FRAME_DISPLAY_INFO (f);
  hlinfo = MOUSE_HL_INFO (f);

  block_input ();

  free_frame_menubar (f);
  free_frame_faces (f);

  if (f == dpyinfo->x_focus_frame)
    dpyinfo->x_focus_frame = 0;
  if (f == dpyinfo->x_highlight_frame)
    dpyinfo->x_highlight_frame = 0;
  if (f == hlinfo->mouse_face_mouse_frame)
    reset_mouse_highlight (hlinfo);

  gtk_widget_destroy(FRAME_GTK_OUTER_WIDGET(f));

  xfree (f->output_data.pgtk);

  unblock_input ();
}

void
x_destroy_window (struct frame *f)
/* --------------------------------------------------------------------------
     External: Delete the window
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE ("x_destroy_window");

  check_window_system (f);
  x_free_frame_resources (f);
}

void
x_set_offset (struct frame *f, int xoff, int yoff, int change_grav)
/* --------------------------------------------------------------------------
     External: Position the window
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_set_offset");
#if 0
  NSView *view = FRAME_NS_VIEW (f);
  NSArray *screens = [NSScreen screens];
  NSScreen *fscreen = [screens objectAtIndex: 0];
  NSScreen *screen = [[view window] screen];

  NSTRACE ("x_set_offset");

  block_input ();

  f->left_pos = xoff;
  f->top_pos = yoff;

  if (view != nil && screen && fscreen)
    {
      f->left_pos = f->size_hint_flags & XNegative
        ? [screen visibleFrame].size.width + f->left_pos - FRAME_PIXEL_WIDTH (f)
        : f->left_pos;
      /* We use visibleFrame here to take menu bar into account.
	 Ideally we should also adjust left/top with visibleFrame.origin.  */

      f->top_pos = f->size_hint_flags & YNegative
        ? ([screen visibleFrame].size.height + f->top_pos
           - FRAME_PIXEL_HEIGHT (f) - FRAME_NS_TITLEBAR_HEIGHT (f)
           - FRAME_TOOLBAR_HEIGHT (f))
        : f->top_pos;
#ifdef NS_IMPL_GNUSTEP
      if (FRAME_PARENT_FRAME (f) == NULL)
	{
	  if (f->left_pos < 100)
	    f->left_pos = 100;  /* don't overlap menu */
	}
#endif
      /* Constrain the setFrameTopLeftPoint so we don't move behind the
         menu bar.  */
      NSPoint pt = NSMakePoint (SCREENMAXBOUND (f->left_pos
                                                + NS_PARENT_WINDOW_LEFT_POS (f)),
                                SCREENMAXBOUND (NS_PARENT_WINDOW_TOP_POS (f)
                                                - f->top_pos));
      NSTRACE_POINT ("setFrameTopLeftPoint", pt);
      [[view window] setFrameTopLeftPoint: pt];
      f->size_hint_flags &= ~(XNegative|YNegative);
    }

  unblock_input ();
#endif
}

void
x_set_window_size (struct frame *f,
                   bool change_gravity,
                   int width,
                   int height,
                   bool pixelwise)
/* --------------------------------------------------------------------------
     Adjust window pixel size based on given character grid size
     Impl is a bit more complex than other terms, need to do some
     internal clipping.
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_set_window_size(%dx%d, %s)", width, height, pixelwise ? "pixel" : "char");
  int pixelwidth, pixelheight;

  block_input ();

  gtk_widget_get_size_request(FRAME_GTK_WIDGET(f), &pixelwidth, &pixelheight);
  PGTK_TRACE("old: %dx%d", pixelwidth, pixelheight);

  if (pixelwise)
    {
      pixelwidth = FRAME_TEXT_TO_PIXEL_WIDTH (f, width);
      pixelheight = FRAME_TEXT_TO_PIXEL_HEIGHT (f, height);
    }
  else
    {
      pixelwidth =  FRAME_TEXT_COLS_TO_PIXEL_WIDTH   (f, width);
      pixelheight = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, height);
    }

  frame_size_history_add
    (f, Qx_set_window_size_1, width, height,
     list5 (Fcons (make_number (pixelwidth), make_number (pixelheight)),
	    Fcons (make_number (pixelwidth), make_number (pixelheight)),
	    make_number (f->border_width),
	    make_number (FRAME_PGTK_TITLEBAR_HEIGHT (f)),
	    make_number (FRAME_TOOLBAR_HEIGHT (f))));

  PGTK_TRACE("new: %dx%d", pixelwidth, pixelheight);
  for (GtkWidget *w = FRAME_GTK_WIDGET(f); w != NULL; w = gtk_widget_get_parent(w)) {
    PGTK_TRACE("%p %s %d %d", w, G_OBJECT_TYPE_NAME(w), gtk_widget_get_mapped(w), gtk_widget_get_visible(w));
    gint wd, hi;
    gtk_widget_get_size_request(w, &wd, &hi);
    PGTK_TRACE(" %dx%d", wd, hi);
    GtkAllocation alloc;
    gtk_widget_get_allocation(w, &alloc);
    PGTK_TRACE(" %dx%d+%d+%d", alloc.width, alloc.height, alloc.x, alloc.y);
  }

#if 0
  gtk_widget_set_size_request(FRAME_GTK_WIDGET(f), pixelwidth, pixelheight);
#else
  // gtk_widget_set_size_request(FRAME_GTK_OUTER_WIDGET(f), pixelwidth, pixelheight);
  PGTK_TRACE("x_set_window_size: %p: %dx%d.", f, width, height);
  gtk_window_resize(GTK_WINDOW(FRAME_GTK_OUTER_WIDGET(f)), pixelwidth, pixelheight);
  x_wm_set_size_hint(f, 0, 0);
#endif

  unblock_input ();

#if 0
  EmacsView *view = FRAME_NS_VIEW (f);
  NSWindow *window = [view window];
  NSRect wr = [window frame];
  int pixelwidth, pixelheight;
  int orig_height = wr.size.height;

  NSTRACE ("x_set_window_size");

  if (view == nil)
    return;

  NSTRACE_RECT ("current", wr);
  NSTRACE_MSG ("Width:%d Height:%d Pixelwise:%d", width, height, pixelwise);
  NSTRACE_MSG ("Font %d x %d", FRAME_COLUMN_WIDTH (f), FRAME_LINE_HEIGHT (f));

  block_input ();

  if (pixelwise)
    {
      pixelwidth = FRAME_TEXT_TO_PIXEL_WIDTH (f, width);
      pixelheight = FRAME_TEXT_TO_PIXEL_HEIGHT (f, height);
    }
  else
    {
      pixelwidth =  FRAME_TEXT_COLS_TO_PIXEL_WIDTH   (f, width);
      pixelheight = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, height);
    }

  wr.size.width = pixelwidth + f->border_width;
  wr.size.height = pixelheight;
  if (! [view isFullscreen])
    wr.size.height += FRAME_NS_TITLEBAR_HEIGHT (f)
      + FRAME_TOOLBAR_HEIGHT (f);

  /* Do not try to constrain to this screen.  We may have multiple
     screens, and want Emacs to span those.  Constraining to screen
     prevents that, and that is not nice to the user.  */
 if (f->output_data.ns->zooming)
   f->output_data.ns->zooming = 0;
 else
   wr.origin.y += orig_height - wr.size.height;

 frame_size_history_add
   (f, Qx_set_window_size_1, width, height,
    list5 (Fcons (make_number (pixelwidth), make_number (pixelheight)),
	   Fcons (make_number (wr.size.width), make_number (wr.size.height)),
	   make_number (f->border_width),
	   make_number (FRAME_NS_TITLEBAR_HEIGHT (f)),
	   make_number (FRAME_TOOLBAR_HEIGHT (f))));

  [window setFrame: wr display: YES];

  [view updateFrameSize: NO];
  unblock_input ();
#endif
}

void
x_iconify_frame (struct frame *f)
/* --------------------------------------------------------------------------
     External: Iconify window
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_iconify_frame");
#if 0
  NSView *view;
  struct ns_display_info *dpyinfo;

  NSTRACE ("x_iconify_frame");
  check_window_system (f);
  view = FRAME_NS_VIEW (f);
  dpyinfo = FRAME_DISPLAY_INFO (f);

  if (dpyinfo->x_highlight_frame == f)
    dpyinfo->x_highlight_frame = 0;

  if ([[view window] windowNumber] <= 0)
    {
      /* the window is still deferred.  Make it very small, bring it
         on screen and order it out. */
      NSRect s = { { 100, 100}, {0, 0} };
      NSRect t;
      t = [[view window] frame];
      [[view window] setFrame: s display: NO];
      [[view window] orderBack: NSApp];
      [[view window] orderOut: NSApp];
      [[view window] setFrame: t display: NO];
    }

  /* Processing input while Emacs is being minimized can cause a
     crash, so block it for the duration. */
  block_input();
  [[view window] miniaturize: NSApp];
  unblock_input();
#endif
}

void
x_make_frame_visible (struct frame *f)
/* --------------------------------------------------------------------------
     External: Show the window (X11 semantics)
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_make_frame_visible");
#if 0
  NSTRACE ("x_make_frame_visible");
  /* XXX: at some points in past this was not needed, as the only place that
     called this (frame.c:Fraise_frame ()) also called raise_lower;
     if this ends up the case again, comment this out again. */
  if (!FRAME_VISIBLE_P (f))
    {
      EmacsView *view = (EmacsView *)FRAME_NS_VIEW (f);
      NSWindow *window = [view window];

      SET_FRAME_VISIBLE (f, 1);
      ns_raise_frame (f, ! FRAME_NO_FOCUS_ON_MAP (f));

      /* Making a new frame from a fullscreen frame will make the new frame
         fullscreen also.  So skip handleFS as this will print an error.  */
      if ([view fsIsNative] && f->want_fullscreen == FULLSCREEN_BOTH
          && [view isFullscreen])
        return;

      if (f->want_fullscreen != FULLSCREEN_NONE)
        {
          block_input ();
          [view handleFS];
          unblock_input ();
        }

      /* Making a frame invisible seems to break the parent->child
         relationship, so reinstate it. */
      if ([window parentWindow] == nil && FRAME_PARENT_FRAME (f) != NULL)
        {
          NSWindow *parent = [FRAME_NS_VIEW (FRAME_PARENT_FRAME (f)) window];

          block_input ();
          [parent addChildWindow: window
                         ordered: NSWindowAbove];
          unblock_input ();

          /* If the parent frame moved while the child frame was
             invisible, the child frame's position won't have been
             updated.  Make sure it's in the right place now. */
          x_set_offset(f, f->left_pos, f->top_pos, 0);
        }
    }
#endif
}


void
x_make_frame_invisible (struct frame *f)
/* --------------------------------------------------------------------------
     External: Hide the window (X11 semantics)
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_make_frame_invisible");
#if 0
  NSView *view;
  NSTRACE ("x_make_frame_invisible");
  check_window_system (f);
  view = FRAME_NS_VIEW (f);
  [[view window] orderOut: NSApp];
  SET_FRAME_VISIBLE (f, 0);
  SET_FRAME_ICONIFIED (f, 0);
#endif
}

Lisp_Object
x_new_font (struct frame *f, Lisp_Object font_object, int fontset)
{
  PGTK_TRACE("x_new_font");
  struct font *font = XFONT_OBJECT (font_object);
  // EmacsView *view = FRAME_NS_VIEW (f);
  int font_ascent, font_descent;

  if (fontset < 0)
    fontset = fontset_from_font (font_object);
  FRAME_FONTSET (f) = fontset;

  if (FRAME_FONT (f) == font) {
    /* This font is already set in frame F.  There's nothing more to
       do.  */
    PGTK_TRACE("already set.");
    return font_object;
  }

  FRAME_FONT (f) = font;
  PGTK_TRACE("font:");
  PGTK_TRACE("  %p", font);
  PGTK_TRACE("  name: %s", SSDATA(font_get_name(font_object)));
  PGTK_TRACE("  width: %d..%d", font->min_width, font->max_width);
  PGTK_TRACE("  pixel_size: %d", font->pixel_size);
  PGTK_TRACE("  height: %d", font->height);
  PGTK_TRACE("  space_width: %d", font->space_width);
  PGTK_TRACE("  average_width: %d", font->average_width);
  PGTK_TRACE("  asc/desc: %d,%d", font->ascent, font->descent);
  PGTK_TRACE("  ul thickness: %d", font->underline_thickness);
  PGTK_TRACE("  ul position: %d", font->underline_position);
  PGTK_TRACE("  vertical_centering: %d", font->vertical_centering);
  PGTK_TRACE("  baseline_offset: %d", font->baseline_offset);
  PGTK_TRACE("  relative_compose: %d", font->relative_compose);
  PGTK_TRACE("  default_ascent: %d", font->default_ascent);
  PGTK_TRACE("  encoding_charset: %d", font->encoding_charset);
  PGTK_TRACE("  repertory_charset: %d", font->repertory_charset);

  FRAME_BASELINE_OFFSET (f) = font->baseline_offset;
  FRAME_COLUMN_WIDTH (f) = font->average_width;
  get_font_ascent_descent (font, &font_ascent, &font_descent);
  FRAME_LINE_HEIGHT (f) = font_ascent + font_descent;

  /* Compute the scroll bar width in character columns.  */
  if (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) > 0)
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f)
	= (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) + wid - 1) / wid;
    }
  else
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f) = (14 + wid - 1) / wid;
    }

  /* Compute the scroll bar height in character lines.  */
  if (FRAME_CONFIG_SCROLL_BAR_HEIGHT (f) > 0)
    {
      int height = FRAME_LINE_HEIGHT (f);
      FRAME_CONFIG_SCROLL_BAR_LINES (f)
	= (FRAME_CONFIG_SCROLL_BAR_HEIGHT (f) + height - 1) / height;
    }
  else
    {
      int height = FRAME_LINE_HEIGHT (f);
      FRAME_CONFIG_SCROLL_BAR_LINES (f) = (14 + height - 1) / height;
    }

  /* Now make the frame display the given font.  */
  if (FRAME_GTK_WIDGET (f) != NULL)
    adjust_frame_size (f, FRAME_COLS (f) * FRAME_COLUMN_WIDTH (f),
		       FRAME_LINES (f) * FRAME_LINE_HEIGHT (f), 3,
		       false, Qfont);

  PGTK_TRACE("set new.");
  return font_object;
}

int
x_display_pixel_height (struct pgtk_display_info *dpyinfo)
{
  PGTK_TRACE("x_display_pixel_height");

  GdkDisplay *dpy = gdk_display_get_default();
  GdkScreen *scr = gdk_display_get_default_screen(dpy);
  PGTK_TRACE(" = %d", gdk_screen_get_height(scr));
  return gdk_screen_get_height(scr);
}

int
x_display_pixel_width (struct pgtk_display_info *dpyinfo)
{
  PGTK_TRACE("x_display_pixel_width");

  GdkDisplay *dpy = gdk_display_get_default();
  GdkScreen *scr = gdk_display_get_default_screen(dpy);
  PGTK_TRACE(" = %d", gdk_screen_get_width(scr));
  return gdk_screen_get_width(scr);
}

void
x_set_parent_frame (struct frame *f, Lisp_Object new_value, Lisp_Object old_value)
/* --------------------------------------------------------------------------
     Set frame F's `parent-frame' parameter.  If non-nil, make F a child
     frame of the frame specified by that parameter.  Technically, this
     makes F's window-system window a child window of the parent frame's
     window-system window.  If nil, make F's window-system window a
     top-level window--a child of its display's root window.

     A child frame's `left' and `top' parameters specify positions
     relative to the top-left corner of its parent frame's native
     rectangle.  On macOS moving a parent frame moves all its child
     frames too, keeping their position relative to the parent
     unaltered.  When a parent frame is iconified or made invisible, its
     child frames are made invisible.  When a parent frame is deleted,
     its child frames are deleted too.

     Whether a child frame has a tool bar may be window-system or window
     manager dependent.  It's advisable to disable it via the frame
     parameter settings.

     Some window managers may not honor this parameter.
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("x_set_parent_frame");
#if 0
  struct frame *p = NULL;
  NSWindow *parent, *child;

  NSTRACE ("x_set_parent_frame");

  if (!NILP (new_value)
      && (!FRAMEP (new_value)
	  || !FRAME_LIVE_P (p = XFRAME (new_value))
	  || !FRAME_X_P (p)))
    {
      store_frame_param (f, Qparent_frame, old_value);
      error ("Invalid specification of `parent-frame'");
    }

  if (p != FRAME_PARENT_FRAME (f))
    {
      parent = [FRAME_NS_VIEW (p) window];
      child = [FRAME_NS_VIEW (f) window];

      block_input ();
      [parent addChildWindow: child
                     ordered: NSWindowAbove];
      unblock_input ();

      fset_parent_frame (f, new_value);
    }
#endif
}

void
x_set_no_focus_on_map (struct frame *f, Lisp_Object new_value, Lisp_Object old_value)
/* Set frame F's `no-focus-on-map' parameter which, if non-nil, means
 * that F's window-system window does not want to receive input focus
 * when it is mapped.  (A frame's window is mapped when the frame is
 * displayed for the first time and when the frame changes its state
 * from `iconified' or `invisible' to `visible'.)
 *
 * Some window managers may not honor this parameter. */
{
  PGTK_TRACE("x_set_no_accept_focus_on_map");
#if 0
  NSTRACE ("x_set_no_focus_on_map");

  if (!EQ (new_value, old_value))
    {
      FRAME_NO_FOCUS_ON_MAP (f) = !NILP (new_value);
    }
#endif
}

void
x_set_no_accept_focus (struct frame *f, Lisp_Object new_value, Lisp_Object old_value)
/*  Set frame F's `no-accept-focus' parameter which, if non-nil, hints
 * that F's window-system window does not want to receive input focus
 * via mouse clicks or by moving the mouse into it.
 *
 * If non-nil, this may have the unwanted side-effect that a user cannot
 * scroll a non-selected frame with the mouse.
 *
 * Some window managers may not honor this parameter. */
{
  PGTK_TRACE("x_set_no_accept_focus");
#if 0
  NSTRACE ("x_set_no_accept_focus");

  if (!EQ (new_value, old_value))
    FRAME_NO_ACCEPT_FOCUS (f) = !NILP (new_value);
#endif
}

void
x_set_z_group (struct frame *f, Lisp_Object new_value, Lisp_Object old_value)
/* Set frame F's `z-group' parameter.  If `above', F's window-system
   window is displayed above all windows that do not have the `above'
   property set.  If nil, F's window is shown below all windows that
   have the `above' property set and above all windows that have the
   `below' property set.  If `below', F's window is displayed below
   all windows that do.

   Some window managers may not honor this parameter. */
{
  PGTK_TRACE("x_set_z_group");
#if 0
  EmacsView *view = (EmacsView *)FRAME_NS_VIEW (f);
  NSWindow *window = [view window];

  NSTRACE ("x_set_z_group");

  if (NILP (new_value))
    {
      window.level = NSNormalWindowLevel;
      FRAME_Z_GROUP (f) = z_group_none;
    }
  else if (EQ (new_value, Qabove))
    {
      window.level = NSNormalWindowLevel + 1;
      FRAME_Z_GROUP (f) = z_group_above;
    }
  else if (EQ (new_value, Qabove_suspended))
    {
      /* Not sure what level this should be. */
      window.level = NSNormalWindowLevel + 1;
      FRAME_Z_GROUP (f) = z_group_above_suspended;
    }
  else if (EQ (new_value, Qbelow))
    {
      window.level = NSNormalWindowLevel - 1;
      FRAME_Z_GROUP (f) = z_group_below;
    }
  else
    error ("Invalid z-group specification");
#endif
}

static void
pgtk_initialize_display_info (struct pgtk_display_info *dpyinfo)
/* --------------------------------------------------------------------------
      Initialize global info and storage for display.
   -------------------------------------------------------------------------- */
{
    dpyinfo->resx = 72.27; /* used 75.0, but this makes pt == pixel, expected */
    dpyinfo->resy = 72.27;
    dpyinfo->color_p = 1;
    dpyinfo->n_planes = 32;
    dpyinfo->color_table = xmalloc (sizeof *dpyinfo->color_table);
    // dpyinfo->color_table->colors = NULL;
    dpyinfo->root_window = 42; /* a placeholder.. */
    dpyinfo->x_highlight_frame = dpyinfo->x_focus_frame = NULL;
    dpyinfo->n_fonts = 0;
    dpyinfo->smallest_font_height = 1;
    dpyinfo->smallest_char_width = 1;

    reset_mouse_highlight (&dpyinfo->mouse_highlight);
}

/* Set S->gc to a suitable GC for drawing glyph string S in cursor
   face.  */

static void
x_set_cursor_gc (struct glyph_string *s)
{
  PGTK_TRACE("x_set_cursor_gc.");
  if (s->font == FRAME_FONT (s->f)
      && s->face->background == FRAME_BACKGROUND_PIXEL (s->f)
      && s->face->foreground == FRAME_FOREGROUND_PIXEL (s->f)
      && !s->cmp)
    PGTK_TRACE("x_set_cursor_gc: 1."),
    s->xgcv = s->f->output_data.pgtk->cursor_xgcv;
  else
    {
      /* Cursor on non-default face: must merge.  */
      XGCValues xgcv;

      PGTK_TRACE("x_set_cursor_gc: 2.");
      xgcv.background = s->f->output_data.pgtk->cursor_color;
      xgcv.foreground = s->face->background;
      PGTK_TRACE("x_set_cursor_gc: 3. %08lx, %08lx.", xgcv.background, xgcv.foreground);

      /* If the glyph would be invisible, try a different foreground.  */
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;
      PGTK_TRACE("x_set_cursor_gc: 4. %08lx, %08lx.", xgcv.background, xgcv.foreground);
#if 0
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->f->output_data.pgtk->cursor_foreground_pixel;
#endif
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;
      PGTK_TRACE("x_set_cursor_gc: 5. %08lx, %08lx.", xgcv.background, xgcv.foreground);

      /* Make sure the cursor is distinct from text in this face.  */
      if (xgcv.background == s->face->background
	  && xgcv.foreground == s->face->foreground)
	{
	  xgcv.background = s->face->foreground;
	  xgcv.foreground = s->face->background;
	}
      PGTK_TRACE("x_set_cursor_gc: 6. %08lx, %08lx.", xgcv.background, xgcv.foreground);

      IF_DEBUG (x_check_font (s->f, s->font));

      s->xgcv = xgcv;
    }
}


/* Set up S->gc of glyph string S for drawing text in mouse face.  */

static void
x_set_mouse_face_gc (struct glyph_string *s)
{
  int face_id;
  struct face *face;

  /* What face has to be used last for the mouse face?  */
  face_id = MOUSE_HL_INFO (s->f)->mouse_face_face_id;
  face = FACE_FROM_ID_OR_NULL (s->f, face_id);
  if (face == NULL)
    face = FACE_FROM_ID (s->f, MOUSE_FACE_ID);

  if (s->first_glyph->type == CHAR_GLYPH)
    face_id = FACE_FOR_CHAR (s->f, face, s->first_glyph->u.ch, -1, Qnil);
  else
    face_id = FACE_FOR_CHAR (s->f, face, 0, -1, Qnil);
  s->face = FACE_FROM_ID (s->f, face_id);
  prepare_face_for_display (s->f, s->face);

  if (s->font == s->face->font) {
    s->xgcv.foreground = s->face->foreground;
    s->xgcv.background = s->face->background;
  } else
    {
      /* Otherwise construct scratch_cursor_gc with values from FACE
	 except for FONT.  */
      XGCValues xgcv;

      xgcv.background = s->face->background;
      xgcv.foreground = s->face->foreground;

      s->xgcv = xgcv;

    }
}


/* Set S->gc of glyph string S to a GC suitable for drawing a mode line.
   Faces to use in the mode line have already been computed when the
   matrix was built, so there isn't much to do, here.  */

static void
x_set_mode_line_face_gc (struct glyph_string *s)
{
  s->xgcv.foreground = s->face->foreground;
  s->xgcv.background = s->face->background;
}


/* Set S->gc of glyph string S for drawing that glyph string.  Set
   S->stippled_p to a non-zero value if the face of S has a stipple
   pattern.  */

static void
x_set_glyph_string_gc (struct glyph_string *s)
{
  PGTK_TRACE("x_set_glyph_string_gc: s->f:    %08lx, %08lx", s->f->background_pixel, s->f->foreground_pixel);
  PGTK_TRACE("x_set_glyph_string_gc: s->face: %08lx, %08lx", s->face->background, s->face->foreground);
  prepare_face_for_display (s->f, s->face);
  PGTK_TRACE("x_set_glyph_string_gc: s->face: %08lx, %08lx", s->face->background, s->face->foreground);

  if (s->hl == DRAW_NORMAL_TEXT)
    {
      s->xgcv.foreground = s->face->foreground;
      s->xgcv.background = s->face->background;
      s->stippled_p = s->face->stipple != 0;
      PGTK_TRACE("x_set_glyph_string_gc: %08lx, %08lx", s->xgcv.background, s->xgcv.foreground);
    }
  else if (s->hl == DRAW_INVERSE_VIDEO)
    {
      x_set_mode_line_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
      PGTK_TRACE("x_set_glyph_string_gc: %08lx, %08lx", s->xgcv.background, s->xgcv.foreground);
    }
  else if (s->hl == DRAW_CURSOR)
    {
      x_set_cursor_gc (s);
      s->stippled_p = false;
      PGTK_TRACE("x_set_glyph_string_gc: %08lx, %08lx", s->xgcv.background, s->xgcv.foreground);
    }
  else if (s->hl == DRAW_MOUSE_FACE)
    {
      x_set_mouse_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
      PGTK_TRACE("x_set_glyph_string_gc: %08lx, %08lx", s->xgcv.background, s->xgcv.foreground);
    }
  else if (s->hl == DRAW_IMAGE_RAISED
	   || s->hl == DRAW_IMAGE_SUNKEN)
    {
      s->xgcv.foreground = s->face->foreground;
      s->xgcv.background = s->face->background;
      s->stippled_p = s->face->stipple != 0;
      PGTK_TRACE("x_set_glyph_string_gc: %08lx, %08lx", s->xgcv.background, s->xgcv.foreground);
    }
  else
    emacs_abort ();
}


/* Set clipping for output of glyph string S.  S may be part of a mode
   line or menu if we don't have X toolkit support.  */

static void
x_set_glyph_string_clipping (struct glyph_string *s, cairo_t *cr)
{
  XRectangle r[2];
  int n = get_glyph_string_clip_rects (s, r, 2);
  PGTK_TRACE("x_set_glyph_string_clipping: n=%d.", n);

  for (int i = 0; i < n; i++) {
    PGTK_TRACE("x_set_glyph_string_clipping: r[%d]: %ux%u+%d+%d.",
		 i, r[i].width, r[i].height, r[i].x, r[i].y);
    cairo_rectangle(cr, r[i].x, r[i].y, r[i].width, r[i].height);
    cairo_clip(cr);
  }
}


/* Set SRC's clipping for output of glyph string DST.  This is called
   when we are drawing DST's left_overhang or right_overhang only in
   the area of SRC.  */

static void
x_set_glyph_string_clipping_exactly (struct glyph_string *src, struct glyph_string *dst, cairo_t *cr)
{
  dst->clip[0].x = src->x;
  dst->clip[0].y = src->y;
  dst->clip[0].width = src->width;
  dst->clip[0].height = src->height;
  dst->num_clips = 1;

  cairo_rectangle(cr, src->x, src->y, src->width, src->height);
  cairo_clip(cr);
}


/* RIF:
   Compute left and right overhang of glyph string S.  */

static void
x_compute_glyph_string_overhangs (struct glyph_string *s)
{
  if (s->cmp == NULL
      && (s->first_glyph->type == CHAR_GLYPH
	  || s->first_glyph->type == COMPOSITE_GLYPH))
    {
      struct font_metrics metrics;

      if (s->first_glyph->type == CHAR_GLYPH)
	{
	  unsigned *code = alloca (sizeof (unsigned) * s->nchars);
	  struct font *font = s->font;
	  int i;

	  for (i = 0; i < s->nchars; i++)
	    code[i] = s->char2b[i];
	  font->driver->text_extents (font, code, s->nchars, &metrics);
	}
      else
	{
	  Lisp_Object gstring = composition_gstring_from_id (s->cmp_id);

	  composition_gstring_width (gstring, s->cmp_from, s->cmp_to, &metrics);
	}
      s->right_overhang = (metrics.rbearing > metrics.width
			   ? metrics.rbearing - metrics.width : 0);
      s->left_overhang = metrics.lbearing < 0 ? - metrics.lbearing : 0;
    }
  else if (s->cmp)
    {
      s->right_overhang = s->cmp->rbearing - s->cmp->pixel_width;
      s->left_overhang = - s->cmp->lbearing;
    }
}


/* Fill rectangle X, Y, W, H with background color of glyph string S.  */

static void
x_clear_glyph_string_rect (struct glyph_string *s, int x, int y, int w, int h)
{
  pgtk_fill_rectangle(s->f, s->xgcv.background, x, y, w, h);
}


/* Draw the background of glyph_string S.  If S->background_filled_p
   is non-zero don't draw it.  FORCE_P non-zero means draw the
   background even if it wouldn't be drawn normally.  This is used
   when a string preceding S draws into the background of S, or S
   contains the first component of a composition.  */

static void
x_draw_glyph_string_background (struct glyph_string *s, bool force_p)
{
  PGTK_TRACE("x_draw_glyph_string_background: 0.");
  /* Nothing to do if background has already been drawn or if it
     shouldn't be drawn in the first place.  */
  if (!s->background_filled_p)
    {
      PGTK_TRACE("x_draw_glyph_string_background: 1.");
      int box_line_width = max (s->face->box_line_width, 0);

      PGTK_TRACE("x_draw_glyph_string_background: 2. %d, %d.",
		   FONT_HEIGHT (s->font), s->height - 2 * box_line_width);
      PGTK_TRACE("x_draw_glyph_string_background: 2. %d.", FONT_TOO_HIGH(s->font));
      PGTK_TRACE("x_draw_glyph_string_background: 2. %d.", s->font_not_found_p);
      PGTK_TRACE("x_draw_glyph_string_background: 2. %d.", s->extends_to_end_of_line_p);
      PGTK_TRACE("x_draw_glyph_string_background: 2. %d.", force_p);
#if 0
      if (s->stippled_p)
	{
	  /* Fill background with a stipple pattern.  */
	  XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
	  x_fill_rectangle (s->f, s->gc, s->x,
			  s->y + box_line_width,
			  s->background_width,
			  s->height - 2 * box_line_width);
	  XSetFillStyle (s->display, s->gc, FillSolid);
	  s->background_filled_p = true;
	}
      else
#endif
	if (FONT_HEIGHT (s->font) < s->height - 2 * box_line_width
	       /* When xdisp.c ignores FONT_HEIGHT, we cannot trust
		  font dimensions, since the actual glyphs might be
		  much smaller.  So in that case we always clear the
		  rectangle with background color.  */
	       || FONT_TOO_HIGH (s->font)
	       || s->font_not_found_p
	       || s->extends_to_end_of_line_p
	       || force_p)
	{
	  PGTK_TRACE("x_draw_glyph_string_background: 3.");
	  x_clear_glyph_string_rect (s, s->x, s->y + box_line_width,
				     s->background_width,
				     s->height - 2 * box_line_width);
	  s->background_filled_p = true;
	}
    }
}


static void
pgtk_draw_rectangle (struct frame *f, unsigned long color, int x, int y, int width, int height)
{
  cairo_t *cr;

  cr = pgtk_begin_cr_clip (f, NULL);
  pgtk_set_cr_source_with_color (f, color);
  cairo_rectangle (cr, x + 0.5, y + 0.5, width, height);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
  pgtk_end_cr_clip (f);
}

/* Draw the foreground of glyph string S.  */

static void
x_draw_glyph_string_foreground (struct glyph_string *s)
{
  int i, x;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  /* Draw characters of S as rectangles if S's font could not be
     loaded.  */
  if (s->font_not_found_p)
    {
      for (i = 0; i < s->nchars; ++i)
	{
	  struct glyph *g = s->first_glyph + i;
	  pgtk_draw_rectangle (s->f,
				 s->face->foreground, x, s->y, g->pixel_width - 1,
				 s->height - 1);
	  x += g->pixel_width;
	}
    }
  else
    {
      struct font *font = s->font;
      int boff = font->baseline_offset;
      int y;

      if (font->vertical_centering)
	boff = VCENTER_BASELINE_OFFSET (font, s->f) - boff;

      y = s->ybase - boff;
      if (s->for_overlaps
	  || (s->background_filled_p && s->hl != DRAW_CURSOR))
	font->driver->draw (s, 0, s->nchars, x, y, false);
      else
	font->driver->draw (s, 0, s->nchars, x, y, true);
      if (s->face->overstrike)
	font->driver->draw (s, 0, s->nchars, x + 1, y, false);
    }
}

/* Draw the foreground of composite glyph string S.  */

static void
x_draw_composite_glyph_string_foreground (struct glyph_string *s)
{
  int i, j, x;
  struct font *font = s->font;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face && s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  /* S is a glyph string for a composition.  S->cmp_from is the index
     of the first character drawn for glyphs of this composition.
     S->cmp_from == 0 means we are drawing the very first character of
     this composition.  */

  /* Draw a rectangle for the composition if the font for the very
     first character of the composition could not be loaded.  */
  if (s->font_not_found_p)
    {
      if (s->cmp_from == 0)
	pgtk_draw_rectangle (s->f, s->face->foreground, x, s->y,
			       s->width - 1, s->height - 1);
    }
  else if (! s->first_glyph->u.cmp.automatic)
    {
      int y = s->ybase;

      for (i = 0, j = s->cmp_from; i < s->nchars; i++, j++)
	/* TAB in a composition means display glyphs with padding
	   space on the left or right.  */
	if (COMPOSITION_GLYPH (s->cmp, j) != '\t')
	  {
	    int xx = x + s->cmp->offsets[j * 2];
	    int yy = y - s->cmp->offsets[j * 2 + 1];

	    font->driver->draw (s, j, j + 1, xx, yy, false);
	    if (s->face->overstrike)
	      font->driver->draw (s, j, j + 1, xx + 1, yy, false);
	  }
    }
  else
    {
      Lisp_Object gstring = composition_gstring_from_id (s->cmp_id);
      Lisp_Object glyph;
      int y = s->ybase;
      int width = 0;

      for (i = j = s->cmp_from; i < s->cmp_to; i++)
	{
	  glyph = LGSTRING_GLYPH (gstring, i);
	  if (NILP (LGLYPH_ADJUSTMENT (glyph)))
	    width += LGLYPH_WIDTH (glyph);
	  else
	    {
	      int xoff, yoff, wadjust;

	      if (j < i)
		{
		  font->driver->draw (s, j, i, x, y, false);
		  if (s->face->overstrike)
		    font->driver->draw (s, j, i, x + 1, y, false);
		  x += width;
		}
	      xoff = LGLYPH_XOFF (glyph);
	      yoff = LGLYPH_YOFF (glyph);
	      wadjust = LGLYPH_WADJUST (glyph);
	      font->driver->draw (s, i, i + 1, x + xoff, y + yoff, false);
	      if (s->face->overstrike)
		font->driver->draw (s, i, i + 1, x + xoff + 1, y + yoff,
				    false);
	      x += wadjust;
	      j = i + 1;
	      width = 0;
	    }
	}
      if (j < i)
	{
	  font->driver->draw (s, j, i, x, y, false);
	  if (s->face->overstrike)
	    font->driver->draw (s, j, i, x + 1, y, false);
	}
    }
}


/* Draw the foreground of glyph string S for glyphless characters.  */

static void
x_draw_glyphless_glyph_string_foreground (struct glyph_string *s)
{
  struct glyph *glyph = s->first_glyph;
  XChar2b char2b[8];
  int x, i, j;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face && s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + eabs (s->face->box_line_width);
  else
    x = s->x;

  s->char2b = char2b;

  for (i = 0; i < s->nchars; i++, glyph++)
    {
      char buf[7], *str = NULL;
      int len = glyph->u.glyphless.len;

      if (glyph->u.glyphless.method == GLYPHLESS_DISPLAY_ACRONYM)
	{
	  if (len > 0
	      && CHAR_TABLE_P (Vglyphless_char_display)
	      && (CHAR_TABLE_EXTRA_SLOTS (XCHAR_TABLE (Vglyphless_char_display))
		  >= 1))
	    {
	      Lisp_Object acronym
		= (! glyph->u.glyphless.for_no_font
		   ? CHAR_TABLE_REF (Vglyphless_char_display,
				     glyph->u.glyphless.ch)
		   : XCHAR_TABLE (Vglyphless_char_display)->extras[0]);
	      if (STRINGP (acronym))
		str = SSDATA (acronym);
	    }
	}
      else if (glyph->u.glyphless.method == GLYPHLESS_DISPLAY_HEX_CODE)
	{
	  unsigned int ch = glyph->u.glyphless.ch;
	  eassume (ch <= MAX_CHAR);
	  sprintf (buf, "%0*X", ch < 0x10000 ? 4 : 6, ch);
	  str = buf;
	}

      if (str)
	{
	  int upper_len = (len + 1) / 2;
	  unsigned code;

	  /* It is assured that all LEN characters in STR is ASCII.  */
	  for (j = 0; j < len; j++)
	    {
	      code = s->font->driver->encode_char (s->font, str[j]);
	      STORE_XCHAR2B (char2b + j, code >> 8, code & 0xFF);
	    }
	  s->font->driver->draw (s, 0, upper_len,
				 x + glyph->slice.glyphless.upper_xoff,
				 s->ybase + glyph->slice.glyphless.upper_yoff,
				 false);
	  s->font->driver->draw (s, upper_len, len,
				 x + glyph->slice.glyphless.lower_xoff,
				 s->ybase + glyph->slice.glyphless.lower_yoff,
				 false);
	}
      if (glyph->u.glyphless.method != GLYPHLESS_DISPLAY_THIN_SPACE)
	pgtk_draw_rectangle (s->f, s->face->foreground,
			       x, s->ybase - glyph->ascent,
			       glyph->pixel_width - 1,
			       glyph->ascent + glyph->descent - 1);
      x += glyph->pixel_width;
   }
}

/* Brightness beyond which a color won't have its highlight brightness
   boosted.

   Nominally, highlight colors for `3d' faces are calculated by
   brightening an object's color by a constant scale factor, but this
   doesn't yield good results for dark colors, so for colors who's
   brightness is less than this value (on a scale of 0-65535) have an
   use an additional additive factor.

   The value here is set so that the default menu-bar/mode-line color
   (grey75) will not have its highlights changed at all.  */
#define HIGHLIGHT_COLOR_DARK_BOOST_LIMIT 48000


/* Allocate a color which is lighter or darker than *PIXEL by FACTOR
   or DELTA.  Try a color with RGB values multiplied by FACTOR first.
   If this produces the same color as PIXEL, try a color where all RGB
   values have DELTA added.  Return the allocated color in *PIXEL.
   DISPLAY is the X display, CMAP is the colormap to operate on.
   Value is non-zero if successful.  */

static bool
x_alloc_lighter_color (struct frame *f, unsigned long *pixel, double factor, int delta)
{
  XColor color, new;
  long bright;
  bool success_p;

  /* Get RGB color values.  */
  color.pixel = *pixel;
  pgtk_query_color (f, &color);

  /* Change RGB values by specified FACTOR.  Avoid overflow!  */
  eassert (factor >= 0);
  new.red = min (0xffff, factor * color.red);
  new.green = min (0xffff, factor * color.green);
  new.blue = min (0xffff, factor * color.blue);

  /* Calculate brightness of COLOR.  */
  bright = (2 * color.red + 3 * color.green + color.blue) / 6;

  /* We only boost colors that are darker than
     HIGHLIGHT_COLOR_DARK_BOOST_LIMIT.  */
  if (bright < HIGHLIGHT_COLOR_DARK_BOOST_LIMIT)
    /* Make an additive adjustment to NEW, because it's dark enough so
       that scaling by FACTOR alone isn't enough.  */
    {
      /* How far below the limit this color is (0 - 1, 1 being darker).  */
      double dimness = 1 - (double)bright / HIGHLIGHT_COLOR_DARK_BOOST_LIMIT;
      /* The additive adjustment.  */
      int min_delta = delta * dimness * factor / 2;

      if (factor < 1)
	{
	  new.red =   max (0, new.red -   min_delta);
	  new.green = max (0, new.green - min_delta);
	  new.blue =  max (0, new.blue -  min_delta);
	}
      else
	{
	  new.red =   min (0xffff, min_delta + new.red);
	  new.green = min (0xffff, min_delta + new.green);
	  new.blue =  min (0xffff, min_delta + new.blue);
	}
    }

  /* Try to allocate the color.  */
  new.pixel = new.red >> 8 << 16 | new.green >> 8 << 8 | new.blue >> 8;
  success_p = true;
  if (success_p)
    {
      if (new.pixel == *pixel)
	{
	  /* If we end up with the same color as before, try adding
	     delta to the RGB values.  */
	  new.red = min (0xffff, delta + color.red);
	  new.green = min (0xffff, delta + color.green);
	  new.blue = min (0xffff, delta + color.blue);
	  new.pixel = new.red >> 8 << 16 | new.green >> 8 << 8 | new.blue >> 8;
	  success_p = true;
	}
      else
	success_p = true;
      *pixel = new.pixel;
    }

  return success_p;
}

static void
x_fill_trapezoid_for_relief (struct frame *f, unsigned long color, int x, int y,
			     int width, int height, int top_p)
{
  cairo_t *cr;

  cr = pgtk_begin_cr_clip (f, NULL);
  pgtk_set_cr_source_with_color (f, color);
  cairo_move_to (cr, top_p ? x : x + height, y);
  cairo_line_to (cr, x, y + height);
  cairo_line_to (cr, top_p ? x + width - height : x + width, y + height);
  cairo_line_to (cr, x + width, y);
  cairo_fill (cr);
  pgtk_end_cr_clip (f);
}

enum corners
  {
    CORNER_BOTTOM_RIGHT,	/* 0 -> pi/2 */
    CORNER_BOTTOM_LEFT,		/* pi/2 -> pi */
    CORNER_TOP_LEFT,		/* pi -> 3pi/2 */
    CORNER_TOP_RIGHT,		/* 3pi/2 -> 2pi */
    CORNER_LAST
  };

static void
x_erase_corners_for_relief (struct frame *f, unsigned long color, int x, int y,
			    int width, int height,
			    double radius, double margin, int corners)
{
  cairo_t *cr;
  int i;

  cr = pgtk_begin_cr_clip (f, NULL);
  pgtk_set_cr_source_with_color (f, color);
  for (i = 0; i < CORNER_LAST; i++)
    if (corners & (1 << i))
      {
	double xm, ym, xc, yc;

	if (i == CORNER_TOP_LEFT || i == CORNER_BOTTOM_LEFT)
	  xm = x - margin, xc = xm + radius;
	else
	  xm = x + width + margin, xc = xm - radius;
	if (i == CORNER_TOP_LEFT || i == CORNER_TOP_RIGHT)
	  ym = y - margin, yc = ym + radius;
	else
	  ym = y + height + margin, yc = ym - radius;

	cairo_move_to (cr, xm, ym);
	cairo_arc (cr, xc, yc, radius, i * M_PI_2, (i + 1) * M_PI_2);
      }
  cairo_clip (cr);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);
  pgtk_end_cr_clip (f);
}

/* Set up the foreground color for drawing relief lines of glyph
   string S.  RELIEF is a pointer to a struct relief containing the GC
   with which lines will be drawn.  Use a color that is FACTOR or
   DELTA lighter or darker than the relief's background which is found
   in S->f->output_data.x->relief_background.  If such a color cannot
   be allocated, use DEFAULT_PIXEL, instead.  */

static void
x_setup_relief_color (struct frame *f, struct relief *relief, double factor,
		      int delta, unsigned long default_pixel)
{
  XGCValues xgcv;
  struct pgtk_output *di = f->output_data.pgtk;
  unsigned long pixel;
  unsigned long background = di->relief_background;
  struct pgtk_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);
  Display *dpy = FRAME_X_DISPLAY (f);

  // xgcv.line_width = 1;

  /* Allocate new color.  */
  xgcv.foreground = default_pixel;
  pixel = background;
  if (x_alloc_lighter_color (f, &pixel, factor, delta))
    xgcv.foreground = relief->pixel = pixel;

  relief->xgcv = xgcv;
}

/* Set up colors for the relief lines around glyph string S.  */

static void
x_setup_relief_colors (struct glyph_string *s)
{
  struct pgtk_output *di = s->f->output_data.pgtk;
  unsigned long color;

  if (s->face->use_box_color_for_shadows_p)
    color = s->face->box_color;
  else if (s->first_glyph->type == IMAGE_GLYPH
	   && s->img->pixmap
	   && !IMAGE_BACKGROUND_TRANSPARENT (s->img, s->f, 0))
    color = IMAGE_BACKGROUND (s->img, s->f, 0);
  else
    {
      /* Get the background color of the face.  */
      color = s->xgcv.background;
    }

  if (TRUE)
    {
      di->relief_background = color;
      x_setup_relief_color (s->f, &di->white_relief, 1.2, 0x8000,
			    WHITE_PIX_DEFAULT (s->f));
      x_setup_relief_color (s->f, &di->black_relief, 0.6, 0x4000,
			    BLACK_PIX_DEFAULT (s->f));
    }
}


static void
x_set_clip_rectangles (struct frame *f, cairo_t *cr, XRectangle *rectangles, int n)
{
  for (int i = 0; i < n; i++) {
    cairo_rectangle(cr,
		    rectangles[i].x,
		    rectangles[i].y,
		    rectangles[i].width,
		    rectangles[i].height);
    cairo_clip(cr);
  }
}

/* Draw a relief on frame F inside the rectangle given by LEFT_X,
   TOP_Y, RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the relief
   to draw, it must be >= 0.  RAISED_P means draw a raised
   relief.  LEFT_P means draw a relief on the left side of
   the rectangle.  RIGHT_P means draw a relief on the right
   side of the rectangle.  CLIP_RECT is the clipping rectangle to use
   when drawing.  */

static void
x_draw_relief_rect (struct frame *f,
		    int left_x, int top_y, int right_x, int bottom_y,
		    int width, bool raised_p, bool top_p, bool bot_p,
		    bool left_p, bool right_p,
		    XRectangle *clip_rect)
{
  unsigned long top_left_color, bottom_right_color;
  int corners = 0;

  cairo_t *cr = pgtk_begin_cr_clip(f, NULL);

  if (raised_p)
    {
      top_left_color = f->output_data.pgtk->white_relief.xgcv.foreground;
      bottom_right_color = f->output_data.pgtk->black_relief.xgcv.foreground;
    }
  else
    {
      top_left_color = f->output_data.pgtk->black_relief.xgcv.foreground;
      bottom_right_color = f->output_data.pgtk->white_relief.xgcv.foreground;
    }

  x_set_clip_rectangles (f, cr, clip_rect, 1);

  if (left_p)
    {
      pgtk_fill_rectangle (f, top_left_color, left_x, top_y,
			     width, bottom_y + 1 - top_y);
      if (top_p)
	corners |= 1 << CORNER_TOP_LEFT;
      if (bot_p)
	corners |= 1 << CORNER_BOTTOM_LEFT;
    }
  if (right_p)
    {
      pgtk_fill_rectangle (f, bottom_right_color, right_x + 1 - width, top_y,
			     width, bottom_y + 1 - top_y);
      if (top_p)
	corners |= 1 << CORNER_TOP_RIGHT;
      if (bot_p)
	corners |= 1 << CORNER_BOTTOM_RIGHT;
    }
  if (top_p)
    {
      if (!right_p)
	pgtk_fill_rectangle (f, top_left_color, left_x, top_y,
			       right_x + 1 - left_x, width);
      else
	x_fill_trapezoid_for_relief (f, top_left_color, left_x, top_y,
				     right_x + 1 - left_x, width, 1);
    }
  if (bot_p)
    {
      if (!left_p)
	pgtk_fill_rectangle (f, bottom_right_color, left_x, bottom_y + 1 - width,
			       right_x + 1 - left_x, width);
      else
	x_fill_trapezoid_for_relief (f, bottom_right_color,
				     left_x, bottom_y + 1 - width,
				     right_x + 1 - left_x, width, 0);
    }
  if (left_p && width != 1)
    pgtk_fill_rectangle (f, bottom_right_color, left_x, top_y,
			   1, bottom_y + 1 - top_y);
  if (top_p && width != 1)
    pgtk_fill_rectangle (f, bottom_right_color, left_x, top_y,
			   right_x + 1 - left_x, 1);
  if (corners)
    {
      x_erase_corners_for_relief (f, FRAME_BACKGROUND_PIXEL (f), left_x, top_y,
				  right_x - left_x + 1, bottom_y - top_y + 1,
				  6, 1, corners);
    }

  pgtk_end_cr_clip(f);
}

/* Draw a box on frame F inside the rectangle given by LEFT_X, TOP_Y,
   RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the lines to
   draw, it must be >= 0.  LEFT_P means draw a line on the
   left side of the rectangle.  RIGHT_P means draw a line
   on the right side of the rectangle.  CLIP_RECT is the clipping
   rectangle to use when drawing.  */

static void
x_draw_box_rect (struct glyph_string *s,
		 int left_x, int top_y, int right_x, int bottom_y, int width,
		 bool left_p, bool right_p, XRectangle *clip_rect)
{
  unsigned long foreground_backup;

  cairo_t *cr = pgtk_begin_cr_clip(s->f, NULL);

  foreground_backup = s->xgcv.foreground;
  s->xgcv.foreground = s->face->box_color;

  x_set_clip_rectangles (s->f, cr, clip_rect, 1);

  /* Top.  */
  pgtk_fill_rectangle (s->f, s->xgcv.foreground,
			 left_x, top_y, right_x - left_x + 1, width);

  /* Left.  */
  if (left_p)
    pgtk_fill_rectangle (s->f, s->xgcv.foreground,
			   left_x, top_y, width, bottom_y - top_y + 1);

  /* Bottom.  */
  pgtk_fill_rectangle (s->f, s->xgcv.foreground,
			 left_x, bottom_y - width + 1, right_x - left_x + 1, width);

  /* Right.  */
  if (right_p)
    pgtk_fill_rectangle (s->f, s->xgcv.foreground,
			   right_x - width + 1, top_y, width, bottom_y - top_y + 1);

  s->xgcv.foreground = foreground_backup;

  pgtk_end_cr_clip(s->f);
}


/* Draw a box around glyph string S.  */

static void
x_draw_glyph_string_box (struct glyph_string *s)
{
  int width, left_x, right_x, top_y, bottom_y, last_x;
  bool raised_p, left_p, right_p;
  struct glyph *last_glyph;
  XRectangle clip_rect;

  last_x = ((s->row->full_width_p && !s->w->pseudo_window_p)
	    ? WINDOW_RIGHT_EDGE_X (s->w)
	    : window_box_right (s->w, s->area));

  /* The glyph that may have a right box line.  */
  last_glyph = (s->cmp || s->img
		? s->first_glyph
		: s->first_glyph + s->nchars - 1);

  width = eabs (s->face->box_line_width);
  raised_p = s->face->box == FACE_RAISED_BOX;
  left_x = s->x;
  right_x = (s->row->full_width_p && s->extends_to_end_of_line_p
	     ? last_x - 1
	     : min (last_x, s->x + s->background_width) - 1);
  top_y = s->y;
  bottom_y = top_y + s->height - 1;

  left_p = (s->first_glyph->left_box_line_p
	    || (s->hl == DRAW_MOUSE_FACE
		&& (s->prev == NULL
		    || s->prev->hl != s->hl)));
  right_p = (last_glyph->right_box_line_p
	     || (s->hl == DRAW_MOUSE_FACE
		 && (s->next == NULL
		     || s->next->hl != s->hl)));

  get_glyph_string_clip_rect (s, &clip_rect);

  if (s->face->box == FACE_SIMPLE_BOX)
    x_draw_box_rect (s, left_x, top_y, right_x, bottom_y, width,
		     left_p, right_p, &clip_rect);
  else
    {
      x_setup_relief_colors (s);
      x_draw_relief_rect (s->f, left_x, top_y, right_x, bottom_y,
			  width, raised_p, true, true, left_p, right_p,
			  &clip_rect);
    }
}

static void
x_get_scale_factor(int *scale_x, int *scale_y)
{
  *scale_x = *scale_y = 1;
}

static void
x_draw_horizontal_wave (struct frame *f, unsigned long color, int x, int y,
			int width, int height, int wave_length)
{
  cairo_t *cr;
  double dx = wave_length, dy = height - 1;
  int xoffset, n;

  cr = pgtk_begin_cr_clip (f, NULL);
  pgtk_set_cr_source_with_gc_foreground (f, NULL);
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  if (x >= 0)
    {
      xoffset = x % (wave_length * 2);
      if (xoffset == 0)
	xoffset = wave_length * 2;
    }
  else
    xoffset = x % (wave_length * 2) + wave_length * 2;
  n = (width + xoffset) / wave_length + 1;
  if (xoffset > wave_length)
    {
      xoffset -= wave_length;
      --n;
      y += height - 1;
      dy = -dy;
    }

  cairo_move_to (cr, x - xoffset + 0.5, y + 0.5);
  while (--n >= 0)
    {
      cairo_rel_line_to (cr, dx, dy);
      dy = -dy;
    }
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
  pgtk_end_cr_clip (f);
}

/*
   Draw a wavy line under S. The wave fills wave_height pixels from y0.

                    x0         wave_length = 2
                                 --
                y0   *   *   *   *   *
                     |* * * * * * * * *
    wave_height = 3  | *   *   *   *

*/
static void
x_draw_underwave (struct glyph_string *s, unsigned long color)
{
  /* Adjust for scale/HiDPI.  */
  int scale_x, scale_y;

  x_get_scale_factor (&scale_x, &scale_y);

  int wave_height = 3 * scale_y, wave_length = 2 * scale_x, thickness = scale_y;

  x_draw_horizontal_wave (s->f, color, s->x, s->ybase - wave_height + 3,
			  s->width, wave_height, wave_length);
}

/* Draw the foreground of image glyph string S to PIXMAP.  */

static void
x_draw_image_foreground_1 (struct glyph_string *s, cairo_surface_t *surface)
{
  int x = 0;
  int y = s->ybase - s->y - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += eabs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

#if 0
  if (s->img->pixmap)
    {
      if (s->img->mask)
	{
	  /* We can't set both a clip mask and use XSetClipRectangles
	     because the latter also sets a clip mask.  We also can't
	     trust on the shape extension to be available
	     (XShapeCombineRegion).  So, compute the rectangle to draw
	     manually.  */
	  unsigned long mask = (GCClipMask | GCClipXOrigin | GCClipYOrigin
				| GCFunction);
	  XGCValues xgcv;

	  xgcv.clip_mask = s->img->mask;
	  xgcv.clip_x_origin = x - s->slice.x;
	  xgcv.clip_y_origin = y - s->slice.y;
	  xgcv.function = GXcopy;
	  XChangeGC (s->display, s->gc, mask, &xgcv);

	  XCopyArea (s->display, s->img->pixmap, pixmap, s->gc,
		     s->slice.x, s->slice.y,
		     s->slice.width, s->slice.height, x, y);
	  XSetClipMask (s->display, s->gc, None);
	}
      else
	{
	  XCopyArea (s->display, s->img->pixmap, pixmap, s->gc,
		     s->slice.x, s->slice.y,
		     s->slice.width, s->slice.height, x, y);

	  /* When the image has a mask, we can expect that at
	     least part of a mouse highlight or a block cursor will
	     be visible.  If the image doesn't have a mask, make
	     a block cursor visible by drawing a rectangle around
	     the image.  I believe it's looking better if we do
	     nothing here for mouse-face.  */
	  if (s->hl == DRAW_CURSOR)
	    {
	      int r = eabs (s->img->relief);
	      x_draw_rectangle (s->f, s->gc, x - r, y - r,
			      s->slice.width + r*2 - 1,
			      s->slice.height + r*2 - 1);
	    }
	}
    }
  else
#endif
    /* Draw a rectangle if image could not be loaded.  */
    pgtk_draw_rectangle (s->f, s->xgcv.foreground, x, y,
			   s->slice.width - 1, s->slice.height - 1);
}

/* Draw a relief around the image glyph string S.  */

static void
x_draw_image_relief (struct glyph_string *s)
{
  int x1, y1, thick;
  bool raised_p, top_p, bot_p, left_p, right_p;
  int extra_x, extra_y;
  XRectangle r;
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += eabs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

  if (s->hl == DRAW_IMAGE_SUNKEN
      || s->hl == DRAW_IMAGE_RAISED)
    {
      thick = tool_bar_button_relief >= 0 ? tool_bar_button_relief : DEFAULT_TOOL_BAR_BUTTON_RELIEF;
      raised_p = s->hl == DRAW_IMAGE_RAISED;
    }
  else
    {
      thick = eabs (s->img->relief);
      raised_p = s->img->relief > 0;
    }

  x1 = x + s->slice.width - 1;
  y1 = y + s->slice.height - 1;

  extra_x = extra_y = 0;
  if (s->face->id == TOOL_BAR_FACE_ID)
    {
      if (CONSP (Vtool_bar_button_margin)
	  && INTEGERP (XCAR (Vtool_bar_button_margin))
	  && INTEGERP (XCDR (Vtool_bar_button_margin)))
	{
	  extra_x = XINT (XCAR (Vtool_bar_button_margin));
	  extra_y = XINT (XCDR (Vtool_bar_button_margin));
	}
      else if (INTEGERP (Vtool_bar_button_margin))
	extra_x = extra_y = XINT (Vtool_bar_button_margin);
    }

  top_p = bot_p = left_p = right_p = false;

  if (s->slice.x == 0)
    x -= thick + extra_x, left_p = true;
  if (s->slice.y == 0)
    y -= thick + extra_y, top_p = true;
  if (s->slice.x + s->slice.width == s->img->width)
    x1 += thick + extra_x, right_p = true;
  if (s->slice.y + s->slice.height == s->img->height)
    y1 += thick + extra_y, bot_p = true;

  x_setup_relief_colors (s);
  get_glyph_string_clip_rect (s, &r);
  x_draw_relief_rect (s->f, x, y, x1, y1, thick, raised_p,
		      top_p, bot_p, left_p, right_p, &r);
}

/* Draw part of the background of glyph string S.  X, Y, W, and H
   give the rectangle to draw.  */

static void
x_draw_glyph_string_bg_rect (struct glyph_string *s, int x, int y, int w, int h)
{
#if 0
  if (s->stippled_p)
    {
      /* Fill background with a stipple pattern.  */
      XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
      x_fill_rectangle (s->f, s->gc, x, y, w, h);
      XSetFillStyle (s->display, s->gc, FillSolid);
    }
  else
#endif
    x_clear_glyph_string_rect (s, x, y, w, h);
}

/* Draw foreground of image glyph string S.  */

static void
x_draw_image_foreground (struct glyph_string *s)
{
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += eabs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

#if 0
  if (s->img->pixmap)
    {
      if (s->img->mask)
	{
	  /* We can't set both a clip mask and use XSetClipRectangles
	     because the latter also sets a clip mask.  We also can't
	     trust on the shape extension to be available
	     (XShapeCombineRegion).  So, compute the rectangle to draw
	     manually.  */
	  unsigned long mask = (GCClipMask | GCClipXOrigin | GCClipYOrigin
				| GCFunction);
	  XGCValues xgcv;
	  XRectangle clip_rect, image_rect, r;

	  xgcv.clip_mask = s->img->mask;
	  xgcv.clip_x_origin = x;
	  xgcv.clip_y_origin = y;
	  xgcv.function = GXcopy;
	  XChangeGC (s->display, s->gc, mask, &xgcv);

	  get_glyph_string_clip_rect (s, &clip_rect);
	  image_rect.x = x;
	  image_rect.y = y;
	  image_rect.width = s->slice.width;
	  image_rect.height = s->slice.height;
	  if (x_intersect_rectangles (&clip_rect, &image_rect, &r))
            XCopyArea (s->display, s->img->pixmap,
                       FRAME_X_DRAWABLE (s->f), s->gc,
		       s->slice.x + r.x - x, s->slice.y + r.y - y,
		       r.width, r.height, r.x, r.y);
	}
      else
	{
	  XRectangle clip_rect, image_rect, r;

	  get_glyph_string_clip_rect (s, &clip_rect);
	  image_rect.x = x;
	  image_rect.y = y;
	  image_rect.width = s->slice.width;
	  image_rect.height = s->slice.height;
	  if (x_intersect_rectangles (&clip_rect, &image_rect, &r))
            XCopyArea (s->display, s->img->pixmap,
                       FRAME_X_DRAWABLE (s->f), s->gc,
		       s->slice.x + r.x - x, s->slice.y + r.y - y,
		       r.width, r.height, r.x, r.y);

	  /* When the image has a mask, we can expect that at
	     least part of a mouse highlight or a block cursor will
	     be visible.  If the image doesn't have a mask, make
	     a block cursor visible by drawing a rectangle around
	     the image.  I believe it's looking better if we do
	     nothing here for mouse-face.  */
	  if (s->hl == DRAW_CURSOR)
	    {
	      int relief = eabs (s->img->relief);
	      x_draw_rectangle (s->f, s->gc,
			      x - relief, y - relief,
			      s->slice.width + relief*2 - 1,
			      s->slice.height + relief*2 - 1);
	    }
	}
    }
  else
#endif
    /* Draw a rectangle if image could not be loaded.  */
    pgtk_draw_rectangle (s->f, s->xgcv.foreground, x, y,
			   s->slice.width - 1, s->slice.height - 1);
}

/* Draw image glyph string S.

            s->y
   s->x      +-------------------------
	     |   s->face->box
	     |
	     |     +-------------------------
	     |     |  s->img->margin
	     |     |
	     |     |       +-------------------
	     |     |       |  the image

 */

static void
x_draw_image_glyph_string (struct glyph_string *s)
{
  int box_line_hwidth = eabs (s->face->box_line_width);
  int box_line_vwidth = max (s->face->box_line_width, 0);
  int height;
  cairo_surface_t *surface = NULL;

  height = s->height;
  if (s->slice.y == 0)
    height -= box_line_vwidth;
  if (s->slice.y + s->slice.height >= s->img->height)
    height -= box_line_vwidth;

  /* Fill background with face under the image.  Do it only if row is
     taller than image or if image has a clip mask to reduce
     flickering.  */
  s->stippled_p = s->face->stipple != 0;
  if (height > s->slice.height
      || s->img->hmargin
      || s->img->vmargin
      || s->img->mask
      || s->img->pixmap == 0
      || s->width != s->background_width)
    {
      if (s->img->mask)
	{
	  /* Create a pixmap as large as the glyph string.  Fill it
	     with the background color.  Copy the image to it, using
	     its mask.  Copy the temporary pixmap to the display.  */

	  /* Create a pixmap as large as the glyph string.  */
	  surface = cairo_surface_create_similar(FRAME_CR_SURFACE(s->f), CAIRO_CONTENT_COLOR_ALPHA,
						 s->background_width,
						 s->height);

	  /* Don't clip in the following because we're working on the
	     pixmap.  */
	  // XSetClipMask (s->display, s->gc, None);

	  /* Fill the pixmap with the background color/stipple.  */
#if 0
	  if (s->stippled_p)
	    {
	      /* Fill background with a stipple pattern.  */
	      XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
	      XSetTSOrigin (s->display, s->gc, - s->x, - s->y);
	      XFillRectangle (s->display, pixmap, s->gc,
			      0, 0, s->background_width, s->height);
	      XSetFillStyle (s->display, s->gc, FillSolid);
	      XSetTSOrigin (s->display, s->gc, 0, 0);
	    }
	  else
#endif
	    {
	      cairo_t *cr = cairo_create(surface);
	      int red = (s->xgcv.background >> 16) & 0xff;
	      int green = (s->xgcv.background >> 8) & 0xff;
	      int blue = (s->xgcv.background >> 0) & 0xff;
	      cairo_set_source_rgb (cr, red / 255.0, green / 255.0, blue / 255.0);
	      cairo_rectangle(cr, 0, 0, s->background_width, s->height);
	      cairo_fill(cr);
	      cairo_destroy(cr);
	    }
	}
      else
	{
	  int x = s->x;
	  int y = s->y;
	  int width = s->background_width;

	  if (s->first_glyph->left_box_line_p
	      && s->slice.x == 0)
	    {
	      x += box_line_hwidth;
	      width -= box_line_hwidth;
	    }

	  if (s->slice.y == 0)
	    y += box_line_vwidth;

	  x_draw_glyph_string_bg_rect (s, x, y, width, height);
	}

      s->background_filled_p = true;
    }

  /* Draw the foreground.  */
  if (s->img->cr_data)
    {
      cairo_t *cr = pgtk_begin_cr_clip (s->f, NULL);

      int x = s->x + s->img->hmargin;
      int y = s->y + s->img->vmargin;
      int width = s->background_width;

      cairo_set_source_surface (cr, s->img->cr_data,
                                x - s->slice.x,
                                y - s->slice.y);
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
      pgtk_end_cr_clip (s->f);
    }
  else
    if (surface != NULL)
    {
      cairo_t *cr = pgtk_begin_cr_clip(s->f, NULL);

      x_draw_image_foreground_1 (s, surface);
      x_set_glyph_string_clipping (s, cr);

      cairo_set_source_surface(cr, surface, 0, 0);
      cairo_rectangle(cr, s->x, s->y, s->background_width, s->height);
      pgtk_end_cr_clip(s->f);
    }
  else
    x_draw_image_foreground (s);

  /* If we must draw a relief around the image, do it.  */
  if (s->img->relief
      || s->hl == DRAW_IMAGE_RAISED
      || s->hl == DRAW_IMAGE_SUNKEN)
    x_draw_image_relief (s);

  if (surface != NULL)
    cairo_surface_destroy(surface);
}

/* Draw stretch glyph string S.  */

static void
x_draw_stretch_glyph_string (struct glyph_string *s)
{
  eassert (s->first_glyph->type == STRETCH_GLYPH);

  if (s->hl == DRAW_CURSOR
      && !x_stretch_cursor_p)
    {
      /* If `x-stretch-cursor' is nil, don't draw a block cursor as
	 wide as the stretch glyph.  */
      int width, background_width = s->background_width;
      int x = s->x;

      if (!s->row->reversed_p)
	{
	  int left_x = window_box_left_offset (s->w, TEXT_AREA);

	  if (x < left_x)
	    {
	      background_width -= left_x - x;
	      x = left_x;
	    }
	}
      else
	{
	  /* In R2L rows, draw the cursor on the right edge of the
	     stretch glyph.  */
	  int right_x = window_box_right (s->w, TEXT_AREA);

	  if (x + background_width > right_x)
	    background_width -= x - right_x;
	  x += background_width;
	}
      width = min (FRAME_COLUMN_WIDTH (s->f), background_width);
      if (s->row->reversed_p)
	x -= width;

      /* Draw cursor.  */
      x_draw_glyph_string_bg_rect (s, x, s->y, width, s->height);

      /* Clear rest using the GC of the original non-cursor face.  */
      if (width < background_width)
	{
	  int y = s->y;
	  int w = background_width - width, h = s->height;
	  XRectangle r;
	  unsigned long color;

	  if (!s->row->reversed_p)
	    x += width;
	  else
	    x = s->x;
	  if (s->row->mouse_face_p
	      && cursor_in_mouse_face_p (s->w))
	    {
	      x_set_mouse_face_gc (s);
	      color = s->xgcv.foreground;
	    }
	  else
	    color = s->face->foreground;

	  cairo_t *cr = pgtk_begin_cr_clip(s->f, NULL);

	  get_glyph_string_clip_rect (s, &r);
	  x_set_clip_rectangles (s->f, cr, &r, 1);

#if 0
	  if (s->face->stipple)
	    {
	      /* Fill background with a stipple pattern.  */
	      XSetFillStyle (s->display, gc, FillOpaqueStippled);
	      x_fill_rectangle (s->f, gc, x, y, w, h);
	      XSetFillStyle (s->display, gc, FillSolid);
	    }
	  else
#endif
	    {
	      pgtk_fill_rectangle(s->f, color, x, y, w, h);
	    }

	  pgtk_end_cr_clip(s->f);
	}
    }
  else if (!s->background_filled_p)
    {
      int background_width = s->background_width;
      int x = s->x, left_x = window_box_left_offset (s->w, TEXT_AREA);

      /* Don't draw into left margin, fringe or scrollbar area
         except for header line and mode line.  */
      if (x < left_x && !s->row->mode_line_p)
	{
	  background_width -= left_x - x;
	  x = left_x;
	}
      if (background_width > 0)
	x_draw_glyph_string_bg_rect (s, x, s->y, background_width, s->height);
    }

  s->background_filled_p = true;
}

static void pgtk_draw_glyph_string(struct glyph_string *s)
{
  PGTK_TRACE("draw_glyph_string.");
  PGTK_TRACE("draw_glyph_string: x=%d, y=%d, width=%d, height=%d.",
	       s->x, s->y, s->width, s->height);

  bool relief_drawn_p = false;

  /* If S draws into the background of its successors, draw the
     background of the successors first so that S can draw into it.
     This makes S->next use XDrawString instead of XDrawImageString.  */
  if (s->next && s->right_overhang && !s->for_overlaps)
    {
      int width;
      struct glyph_string *next;

      for (width = 0, next = s->next;
	   next && width < s->right_overhang;
	   width += next->width, next = next->next)
	if (next->first_glyph->type != IMAGE_GLYPH)
	  {
	    cairo_t *cr = pgtk_begin_cr_clip(next->f, NULL);
	    PGTK_TRACE("pgtk_draw_glyph_string: 1.");
	    x_set_glyph_string_gc (next);
	    x_set_glyph_string_clipping (next, cr);
	    if (next->first_glyph->type == STRETCH_GLYPH)
	      x_draw_stretch_glyph_string (next);
	    else
	      x_draw_glyph_string_background (next, true);
	    next->num_clips = 0;
	    pgtk_end_cr_clip(next->f);
	  }
    }

  /* Set up S->gc, set clipping and draw S.  */
  PGTK_TRACE("pgtk_draw_glyph_string: 2.");
  x_set_glyph_string_gc (s);

  cairo_t *cr = pgtk_begin_cr_clip(s->f, NULL);

  /* Draw relief (if any) in advance for char/composition so that the
     glyph string can be drawn over it.  */
  if (!s->for_overlaps
      && s->face->box != FACE_NO_BOX
      && (s->first_glyph->type == CHAR_GLYPH
	  || s->first_glyph->type == COMPOSITE_GLYPH))

    {
      PGTK_TRACE("pgtk_draw_glyph_string: 2.1.");
      x_set_glyph_string_clipping (s, cr);
      x_draw_glyph_string_background (s, true);
      x_draw_glyph_string_box (s);
      x_set_glyph_string_clipping (s, cr);
      relief_drawn_p = true;
    }
  else if (!s->clip_head /* draw_glyphs didn't specify a clip mask. */
	   && !s->clip_tail
	   && ((s->prev && s->prev->hl != s->hl && s->left_overhang)
	       || (s->next && s->next->hl != s->hl && s->right_overhang)))
    /* We must clip just this glyph.  left_overhang part has already
       drawn when s->prev was drawn, and right_overhang part will be
       drawn later when s->next is drawn. */
    PGTK_TRACE("pgtk_draw_glyph_string: 2.2."),
    x_set_glyph_string_clipping_exactly (s, s, cr);
  else
    PGTK_TRACE("pgtk_draw_glyph_string: 2.3."),
    x_set_glyph_string_clipping (s, cr);

  switch (s->first_glyph->type)
    {
    case IMAGE_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.4.");
      x_draw_image_glyph_string (s);
      break;

    case XWIDGET_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.5.");
#if 0
      x_draw_xwidget_glyph_string (s);
#endif
      break;

    case STRETCH_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.6.");
      x_draw_stretch_glyph_string (s);
      break;

    case CHAR_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.7.");
      if (s->for_overlaps)
	s->background_filled_p = true;
      else
	x_draw_glyph_string_background (s, false);
      x_draw_glyph_string_foreground (s);
      break;

    case COMPOSITE_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.8.");
      if (s->for_overlaps || (s->cmp_from > 0
			      && ! s->first_glyph->u.cmp.automatic))
	s->background_filled_p = true;
      else
	x_draw_glyph_string_background (s, true);
      x_draw_composite_glyph_string_foreground (s);
      break;

    case GLYPHLESS_GLYPH:
      PGTK_TRACE("pgtk_draw_glyph_string: 2.9.");
      if (s->for_overlaps)
	s->background_filled_p = true;
      else
	x_draw_glyph_string_background (s, true);
      x_draw_glyphless_glyph_string_foreground (s);
      break;

    default:
      emacs_abort ();
    }

  if (!s->for_overlaps)
    {
      /* Draw underline.  */
      if (s->face->underline_p)
        {
          if (s->face->underline_type == FACE_UNDER_WAVE)
            {
              if (s->face->underline_defaulted_p)
                x_draw_underwave (s, s->xgcv.foreground);
              else
                {
                  x_draw_underwave (s, s->face->underline_color);
                }
            }
          else if (s->face->underline_type == FACE_UNDER_LINE)
            {
              unsigned long thickness, position;
              int y;

              if (s->prev && s->prev->face->underline_p
		  && s->prev->face->underline_type == FACE_UNDER_LINE)
                {
                  /* We use the same underline style as the previous one.  */
                  thickness = s->prev->underline_thickness;
                  position = s->prev->underline_position;
                }
              else
                {
		  struct font *font = font_for_underline_metrics (s);

                  /* Get the underline thickness.  Default is 1 pixel.  */
                  if (font && font->underline_thickness > 0)
                    thickness = font->underline_thickness;
                  else
                    thickness = 1;
                  if (x_underline_at_descent_line)
                    position = (s->height - thickness) - (s->ybase - s->y);
                  else
                    {
                      /* Get the underline position.  This is the recommended
                         vertical offset in pixels from the baseline to the top of
                         the underline.  This is a signed value according to the
                         specs, and its default is

                         ROUND ((maximum descent) / 2), with
                         ROUND(x) = floor (x + 0.5)  */

                      if (x_use_underline_position_properties
                          && font && font->underline_position >= 0)
                        position = font->underline_position;
                      else if (font)
                        position = (font->descent + 1) / 2;
                      else
                        position = underline_minimum_offset;
                    }
                  position = max (position, underline_minimum_offset);
                }
              /* Check the sanity of thickness and position.  We should
                 avoid drawing underline out of the current line area.  */
              if (s->y + s->height <= s->ybase + position)
                position = (s->height - 1) - (s->ybase - s->y);
              if (s->y + s->height < s->ybase + position + thickness)
                thickness = (s->y + s->height) - (s->ybase + position);
              s->underline_thickness = thickness;
              s->underline_position = position;
              y = s->ybase + position;
              if (s->face->underline_defaulted_p)
                pgtk_fill_rectangle (s->f, s->xgcv.foreground,
				       s->x, y, s->width, thickness);
              else
                {
                  pgtk_fill_rectangle (s->f, s->face->underline_color,
					 s->x, y, s->width, thickness);
                }
            }
        }
      /* Draw overline.  */
      if (s->face->overline_p)
	{
	  unsigned long dy = 0, h = 1;

	  if (s->face->overline_color_defaulted_p)
	    pgtk_fill_rectangle (s->f, s->xgcv.foreground, s->x, s->y + dy,
				   s->width, h);
	  else
	    {
	      pgtk_fill_rectangle (s->f, s->face->overline_color, s->x, s->y + dy,
				     s->width, h);
	    }
	}

      /* Draw strike-through.  */
      if (s->face->strike_through_p)
	{
	  /* Y-coordinate and height of the glyph string's first
	     glyph.  We cannot use s->y and s->height because those
	     could be larger if there are taller display elements
	     (e.g., characters displayed with a larger font) in the
	     same glyph row.  */
	  int glyph_y = s->ybase - s->first_glyph->ascent;
	  int glyph_height = s->first_glyph->ascent + s->first_glyph->descent;
	  /* Strike-through width and offset from the glyph string's
	     top edge.  */
          unsigned long h = 1;
          unsigned long dy = (glyph_height - h) / 2;

	  if (s->face->strike_through_color_defaulted_p)
	    pgtk_fill_rectangle (s->f, s->xgcv.foreground, s->x, glyph_y + dy,
				   s->width, h);
	  else
	    {
	      pgtk_fill_rectangle (s->f, s->face->strike_through_color, s->x, glyph_y + dy,
				     s->width, h);
	    }
	}

      /* Draw relief if not yet drawn.  */
      if (!relief_drawn_p && s->face->box != FACE_NO_BOX)
	x_draw_glyph_string_box (s);

      if (s->prev)
	{
	  struct glyph_string *prev;

	  for (prev = s->prev; prev; prev = prev->prev)
	    if (prev->hl != s->hl
		&& prev->x + prev->width + prev->right_overhang > s->x)
	      {
		/* As prev was drawn while clipped to its own area, we
		   must draw the right_overhang part using s->hl now.  */
		enum draw_glyphs_face save = prev->hl;

		prev->hl = s->hl;
		PGTK_TRACE("pgtk_draw_glyph_string: 3.");
		x_set_glyph_string_gc (prev);
		cairo_save(cr);
		x_set_glyph_string_clipping_exactly (s, prev, cr);
		if (prev->first_glyph->type == CHAR_GLYPH)
		  x_draw_glyph_string_foreground (prev);
		else
		  x_draw_composite_glyph_string_foreground (prev);
		prev->hl = save;
		prev->num_clips = 0;
		cairo_restore(cr);
	      }
	}

      if (s->next)
	{
	  struct glyph_string *next;

	  for (next = s->next; next; next = next->next)
	    if (next->hl != s->hl
		&& next->x - next->left_overhang < s->x + s->width)
	      {
		/* As next will be drawn while clipped to its own area,
		   we must draw the left_overhang part using s->hl now.  */
		enum draw_glyphs_face save = next->hl;

		next->hl = s->hl;
		PGTK_TRACE("pgtk_draw_glyph_string: 4.");
		x_set_glyph_string_gc (next);
		cairo_save(cr);
		x_set_glyph_string_clipping_exactly (s, next, cr);
		if (next->first_glyph->type == CHAR_GLYPH)
		  x_draw_glyph_string_foreground (next);
		else
		  x_draw_composite_glyph_string_foreground (next);
		cairo_restore(cr);
		next->hl = save;
		next->num_clips = 0;
		next->clip_head = s->next;
	      }
	}
    }

  /* Reset clipping.  */
  pgtk_end_cr_clip(s->f);
  s->num_clips = 0;
}

/* RIF: Define cursor CURSOR on frame F.  */

static void
pgtk_define_frame_cursor (struct frame *f, Cursor cursor)
{
#if 0
  if (!f->pointer_invisible
      && f->output_data.x->current_cursor != cursor)
    XDefineCursor (FRAME_X_DISPLAY (f), FRAME_X_WINDOW (f), cursor);
  f->output_data.x->current_cursor = cursor;
#endif
}

static void pgtk_after_update_window_line(struct window *w, struct glyph_row *desired_row)
{
  PGTK_TRACE("after_update_window_line.");

  struct frame *f;
  int width, height;

  /* begin copy from other terms */
  eassert (w);

  if (!desired_row->mode_line_p && !w->pseudo_window_p)
    desired_row->redraw_fringe_bitmaps_p = 1;

  /* When a window has disappeared, make sure that no rest of
     full-width rows stays visible in the internal border.  */
  if (windows_or_buffers_changed
      && desired_row->full_width_p
      && (f = XFRAME (w->frame),
	  width = FRAME_INTERNAL_BORDER_WIDTH (f),
	  width != 0)
      && (height = desired_row->visible_height,
	  height > 0))
    {
      int y = WINDOW_TO_FRAME_PIXEL_Y (w, max (0, desired_row->y));

      block_input ();
      pgtk_clear_frame_area (f, 0, y, width, height);
      pgtk_clear_frame_area (f,
			       FRAME_PIXEL_WIDTH (f) - width,
			       y, width, height);
      unblock_input ();
    }
}

static void pgtk_clear_frame_area(struct frame *f, int x, int y, int width, int height)
{
  PGTK_TRACE("clear_frame_area.");
  pgtk_clear_area (f, x, y, width, height);
}

/* Draw a hollow box cursor on window W in glyph row ROW.  */

static void
x_draw_hollow_cursor (struct window *w, struct glyph_row *row)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct pgtk_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);
  Display *dpy = FRAME_X_DISPLAY (f);
  int x, y, wd, h;
  XGCValues xgcv;
  struct glyph *cursor_glyph;

  /* Get the glyph the cursor is on.  If we can't tell because
     the current matrix is invalid or such, give up.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* Compute frame-relative coordinates for phys cursor.  */
  get_phys_cursor_geometry (w, row, cursor_glyph, &x, &y, &h);
  wd = w->phys_cursor_width - 1;

  /* The foreground of cursor_gc is typically the same as the normal
     background color, which can cause the cursor box to be invisible.  */
  cairo_t *cr = pgtk_begin_cr_clip(f, NULL);
  pgtk_set_cr_source_with_color(f, f->output_data.pgtk->cursor_color);

  /* When on R2L character, show cursor at the right edge of the
     glyph, unless the cursor box is as wide as the glyph or wider
     (the latter happens when x-stretch-cursor is non-nil).  */
  if ((cursor_glyph->resolved_level & 1) != 0
      && cursor_glyph->pixel_width > wd)
    {
      x += cursor_glyph->pixel_width - wd;
      if (wd > 0)
	wd -= 1;
    }
  /* Set clipping, draw the rectangle, and reset clipping again.  */
  pgtk_clip_to_row (w, row, TEXT_AREA, cr);
  pgtk_draw_rectangle (f, f->output_data.pgtk->cursor_color, x, y, wd, h - 1);
  pgtk_end_cr_clip(f);
}

/* Draw a bar cursor on window W in glyph row ROW.

   Implementation note: One would like to draw a bar cursor with an
   angle equal to the one given by the font property XA_ITALIC_ANGLE.
   Unfortunately, I didn't find a font yet that has this property set.
   --gerd.  */

static void
x_draw_bar_cursor (struct window *w, struct glyph_row *row, int width, enum text_cursor_kinds kind)
{
  struct frame *f = XFRAME (w->frame);
  struct glyph *cursor_glyph;

  /* If cursor is out of bounds, don't draw garbage.  This can happen
     in mini-buffer windows when switching between echo area glyphs
     and mini-buffer.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* Experimental avoidance of cursor on xwidget.  */
  if (cursor_glyph->type == XWIDGET_GLYPH)
    return;

  /* If on an image, draw like a normal cursor.  That's usually better
     visible than drawing a bar, esp. if the image is large so that
     the bar might not be in the window.  */
  if (cursor_glyph->type == IMAGE_GLYPH)
    {
      struct glyph_row *r;
      r = MATRIX_ROW (w->current_matrix, w->phys_cursor.vpos);
      draw_phys_cursor_glyph (w, r, DRAW_CURSOR);
    }
  else
    {
      struct face *face = FACE_FROM_ID (f, cursor_glyph->face_id);
      unsigned long color;

      cairo_t *cr = pgtk_begin_cr_clip(f, NULL);

      /* If the glyph's background equals the color we normally draw
	 the bars cursor in, the bar cursor in its normal color is
	 invisible.  Use the glyph's foreground color instead in this
	 case, on the assumption that the glyph's colors are chosen so
	 that the glyph is legible.  */
      if (face->background == f->output_data.pgtk->cursor_color)
	color = face->foreground;
      else
	color = f->output_data.pgtk->cursor_color;

      pgtk_clip_to_row (w, row, TEXT_AREA, cr);

      if (kind == BAR_CURSOR)
	{
	  int x = WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x);

	  if (width < 0)
	    width = FRAME_CURSOR_WIDTH (f);
	  width = min (cursor_glyph->pixel_width, width);

	  w->phys_cursor_width = width;

	  /* If the character under cursor is R2L, draw the bar cursor
	     on the right of its glyph, rather than on the left.  */
	  if ((cursor_glyph->resolved_level & 1) != 0)
	    x += cursor_glyph->pixel_width - width;

	  pgtk_fill_rectangle (f, color, x,
				 WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y),
				 width, row->height);
	}
      else /* HBAR_CURSOR */
	{
	  int dummy_x, dummy_y, dummy_h;
	  int x = WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x);

	  if (width < 0)
	    width = row->height;

	  width = min (row->height, width);

	  get_phys_cursor_geometry (w, row, cursor_glyph, &dummy_x,
				    &dummy_y, &dummy_h);

	  if ((cursor_glyph->resolved_level & 1) != 0
	      && cursor_glyph->pixel_width > w->phys_cursor_width - 1)
	    x += cursor_glyph->pixel_width - w->phys_cursor_width + 1;
	  pgtk_fill_rectangle (f, color, x,
				 WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y +
							  row->height - width),
				 w->phys_cursor_width - 1, width);
	}

      pgtk_end_cr_clip(f);
    }
}

/* RIF: Draw cursor on window W.  */

static void
pgtk_draw_window_cursor (struct window *w, struct glyph_row *glyph_row, int x,
		      int y, enum text_cursor_kinds cursor_type,
		      int cursor_width, bool on_p, bool active_p)
{
  PGTK_TRACE("draw_window_cursor: %d, %d, %d, %d, %d, %d.",
	       x, y, cursor_type, cursor_width, on_p, active_p);
  struct frame *f = XFRAME (WINDOW_FRAME (w));

  if (on_p)
    {
      w->phys_cursor_type = cursor_type;
      w->phys_cursor_on_p = true;

      if (glyph_row->exact_window_width_line_p
	  && (glyph_row->reversed_p
	      ? (w->phys_cursor.hpos < 0)
	      : (w->phys_cursor.hpos >= glyph_row->used[TEXT_AREA])))
	{
	  glyph_row->cursor_in_fringe_p = true;
	  draw_fringe_bitmap (w, glyph_row, glyph_row->reversed_p);
	}
      else
	{
	  switch (cursor_type)
	    {
	    case HOLLOW_BOX_CURSOR:
	      x_draw_hollow_cursor (w, glyph_row);
	      break;

	    case FILLED_BOX_CURSOR:
	      draw_phys_cursor_glyph (w, glyph_row, DRAW_CURSOR);
	      break;

	    case BAR_CURSOR:
	      x_draw_bar_cursor (w, glyph_row, cursor_width, BAR_CURSOR);
	      break;

	    case HBAR_CURSOR:
	      x_draw_bar_cursor (w, glyph_row, cursor_width, HBAR_CURSOR);
	      break;

	    case NO_CURSOR:
	      w->phys_cursor_width = 0;
	      break;

	    default:
	      emacs_abort ();
	    }
	}

#ifdef HAVE_X_I18N
      if (w == XWINDOW (f->selected_window))
	if (FRAME_XIC (f) && (FRAME_XIC_STYLE (f) & XIMPreeditPosition))
	  xic_set_preeditarea (w, x, y);
#endif
    }

  gtk_widget_queue_draw(FRAME_GTK_WIDGET(f));
}

/* Scroll part of the display as described by RUN.  */

static void
pgtk_scroll_run (struct window *w, struct run *run)
{
  struct frame *f = XFRAME (w->frame);
  int x, y, width, height, from_y, to_y, bottom_y;

  /* Get frame-relative bounding box of the text display area of W,
     without mode lines.  Include in this box the left and right
     fringe of W.  */
  window_box (w, ANY_AREA, &x, &y, &width, &height);

  from_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->current_y);
  to_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->desired_y);
  bottom_y = y + height;

  if (to_y < from_y)
    {
      /* Scrolling up.  Make sure we don't copy part of the mode
	 line at the bottom.  */
      if (from_y + run->height > bottom_y)
	height = bottom_y - from_y;
      else
	height = run->height;
    }
  else
    {
      /* Scrolling down.  Make sure we don't copy over the mode line.
	 at the bottom.  */
      if (to_y + run->height > bottom_y)
	height = bottom_y - to_y;
      else
	height = run->height;
    }

  block_input ();

  /* Cursor off.  Will be switched on again in x_update_window_end.  */
  // x_clear_cursor (w);

  SET_FRAME_GARBAGED (f);

  unblock_input ();
}

/***********************************************************************
		    Starting and ending an update
 ***********************************************************************/

/* Start an update of frame F.  This function is installed as a hook
   for update_begin, i.e. it is called when update_begin is called.
   This function is called prior to calls to x_update_window_begin for
   each window being updated.  Currently, there is nothing to do here
   because all interesting stuff is done on a window basis.  */

static void
pgtk_update_begin (struct frame *f)
{
  if (! NILP (tip_frame) && XFRAME (tip_frame) == f
      && ! FRAME_VISIBLE_P (f))
    return;

  if (! FRAME_CR_SURFACE (f))
    {
      int width, height;
      if (FRAME_GTK_WIDGET (f))
        {
          GdkWindow *w = gtk_widget_get_window (FRAME_GTK_WIDGET (f));
          width = gdk_window_get_width (w);
          height = gdk_window_get_height (w);
        }
      else
        {
          width = FRAME_PIXEL_WIDTH (f);
          height = FRAME_PIXEL_HEIGHT (f);
          if (! FRAME_EXTERNAL_TOOL_BAR (f))
            height += FRAME_TOOL_BAR_HEIGHT (f);
          if (! FRAME_EXTERNAL_MENU_BAR (f))
            height += FRAME_MENU_BAR_HEIGHT (f);
        }

      if (width > 0 && height > 0)
        {
          block_input();
          FRAME_CR_SURFACE (f) = cairo_image_surface_create
            (CAIRO_FORMAT_ARGB32, width, height);
          unblock_input();
        }
    }
}

/* Start update of window W.  */

static void
pgtk_update_window_begin (struct window *w)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  Mouse_HLInfo *hlinfo = MOUSE_HL_INFO (f);

  w->output_cursor = w->cursor;

  block_input ();

  if (f == hlinfo->mouse_face_mouse_frame)
    {
      /* Don't do highlighting for mouse motion during the update.  */
      hlinfo->mouse_face_defer = true;

      /* If F needs to be redrawn, simply forget about any prior mouse
	 highlighting.  */
      if (FRAME_GARBAGED_P (f))
	hlinfo->mouse_face_window = Qnil;
    }

  unblock_input ();
}


/* Draw a vertical window border from (x,y0) to (x,y1)  */

static void
pgtk_draw_vertical_window_border (struct window *w, int x, int y0, int y1)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct face *face;
  cairo_t *cr;

  cr = pgtk_begin_cr_clip (f, NULL);

  face = FACE_FROM_ID_OR_NULL (f, VERTICAL_BORDER_FACE_ID);
  if (face)
    pgtk_set_cr_source_with_color (f, face->foreground);

  cairo_rectangle (cr, x, y0, 1, y1 - y0);
  cairo_fill (cr);

  pgtk_end_cr_clip (f);
}

/* Draw a window divider from (x0,y0) to (x1,y1)  */

static void
pgtk_draw_window_divider (struct window *w, int x0, int x1, int y0, int y1)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct face *face = FACE_FROM_ID_OR_NULL (f, WINDOW_DIVIDER_FACE_ID);
  struct face *face_first
    = FACE_FROM_ID_OR_NULL (f, WINDOW_DIVIDER_FIRST_PIXEL_FACE_ID);
  struct face *face_last
    = FACE_FROM_ID_OR_NULL (f, WINDOW_DIVIDER_LAST_PIXEL_FACE_ID);
  unsigned long color = face ? face->foreground : FRAME_FOREGROUND_PIXEL (f);
  unsigned long color_first = (face_first
			       ? face_first->foreground
			       : FRAME_FOREGROUND_PIXEL (f));
  unsigned long color_last = (face_last
			      ? face_last->foreground
			      : FRAME_FOREGROUND_PIXEL (f));
  cairo_t *cr = pgtk_begin_cr_clip (f, NULL);

  if (y1 - y0 > x1 - x0 && x1 - x0 > 2)
    /* Vertical.  */
    {
      pgtk_set_cr_source_with_color (f, color_first);
      cairo_rectangle (cr, x0, y0, 1, y1 - y0);
      cairo_fill(cr);
      pgtk_set_cr_source_with_color (f, color);
      cairo_rectangle (cr, x0 + 1, y0, x1 - x0 - 2, y1 - y0);
      cairo_fill(cr);
      pgtk_set_cr_source_with_color (f, color_last);
      cairo_rectangle (cr, x1 - 1, y0, 1, y1 - y0);
      cairo_fill(cr);
    }
  else if (x1 - x0 > y1 - y0 && y1 - y0 > 3)
    /* Horizontal.  */
    {
      pgtk_set_cr_source_with_color (f, color_first);
      cairo_rectangle (cr, x0, y0, x1 - x0, 1);
      cairo_fill(cr);
      pgtk_set_cr_source_with_color (f, color);
      cairo_rectangle (cr, x0, y0 + 1, x1 - x0, y1 - y0 - 2);
      cairo_fill(cr);
      pgtk_set_cr_source_with_color (f, color_last);
      cairo_rectangle (cr, x0, y1 - 1, x1 - x0, 1);
      cairo_fill(cr);
    }
  else
    {
      pgtk_set_cr_source_with_color (f, color);
      cairo_rectangle (cr, x0, y0, x1 - x0, y1 - y0);
      cairo_fill(cr);
    }

  pgtk_end_cr_clip (f);
}

/* End update of window W.

   Draw vertical borders between horizontally adjacent windows, and
   display W's cursor if CURSOR_ON_P is non-zero.

   MOUSE_FACE_OVERWRITTEN_P non-zero means that some row containing
   glyphs in mouse-face were overwritten.  In that case we have to
   make sure that the mouse-highlight is properly redrawn.

   W may be a menu bar pseudo-window in case we don't have X toolkit
   support.  Such windows don't have a cursor, so don't display it
   here.  */

static void
pgtk_update_window_end (struct window *w, bool cursor_on_p,
		     bool mouse_face_overwritten_p)
{
  if (!w->pseudo_window_p)
    {
      block_input ();

      if (cursor_on_p)
	display_and_set_cursor (w, true,
				w->output_cursor.hpos, w->output_cursor.vpos,
				w->output_cursor.x, w->output_cursor.y);

      if (draw_window_fringes (w, true))
	{
	  if (WINDOW_RIGHT_DIVIDER_WIDTH (w))
	    x_draw_right_divider (w);
	  else
	    x_draw_vertical_border (w);
	}

      unblock_input ();
    }

  /* If a row with mouse-face was overwritten, arrange for
     XTframe_up_to_date to redisplay the mouse highlight.  */
  if (mouse_face_overwritten_p)
    {
      Mouse_HLInfo *hlinfo = MOUSE_HL_INFO (XFRAME (w->frame));

      hlinfo->mouse_face_beg_row = hlinfo->mouse_face_beg_col = -1;
      hlinfo->mouse_face_end_row = hlinfo->mouse_face_end_col = -1;
      hlinfo->mouse_face_window = Qnil;
    }
}

/* End update of frame F.  This function is installed as a hook in
   update_end.  */

static void
pgtk_update_end (struct frame *f)
{
  /* Mouse highlight may be displayed again.  */
  MOUSE_HL_INFO (f)->mouse_face_defer = false;

  if (FRAME_CR_SURFACE (f))
    {
      block_input();
      gtk_widget_queue_draw(FRAME_GTK_WIDGET(f));
      unblock_input ();
    }
}

/* Return the current position of the mouse.
   *FP should be a frame which indicates which display to ask about.

   If the mouse movement started in a scroll bar, set *FP, *BAR_WINDOW,
   and *PART to the frame, window, and scroll bar part that the mouse
   is over.  Set *X and *Y to the portion and whole of the mouse's
   position on the scroll bar.

   If the mouse movement started elsewhere, set *FP to the frame the
   mouse is on, *BAR_WINDOW to nil, and *X and *Y to the character cell
   the mouse is over.

   Set *TIMESTAMP to the server time-stamp for the time at which the mouse
   was at this position.

   Don't store anything if we don't have a valid set of values to report.

   This clears the mouse_moved flag, so we can wait for the next mouse
   movement.  */

static void
pgtk_mouse_position (struct frame **fp, int insist, Lisp_Object *bar_window,
		     enum scroll_bar_part *part, Lisp_Object *x, Lisp_Object *y,
		     Time *timestamp)
{
  struct frame *f1;
  struct pgtk_display_info *dpyinfo = FRAME_DISPLAY_INFO (*fp);

  block_input ();

#if 0
  if (dpyinfo->last_mouse_scroll_bar && insist == 0)
    {
      struct scroll_bar *bar = dpyinfo->last_mouse_scroll_bar;

      if (bar->horizontal)
	x_horizontal_scroll_bar_report_motion (fp, bar_window, part, x, y, timestamp);
      else
	x_scroll_bar_report_motion (fp, bar_window, part, x, y, timestamp);
    }
  else
#endif
    {
      Window root;
      int root_x, root_y;

      Window dummy_window;
      int dummy;

      Lisp_Object frame, tail;

      /* Clear the mouse-moved flag for every frame on this display.  */
      FOR_EACH_FRAME (tail, frame)
	if (FRAME_PGTK_P (XFRAME (frame))
            && FRAME_X_DISPLAY (XFRAME (frame)) == FRAME_X_DISPLAY (*fp))
	  XFRAME (frame)->mouse_moved = false;

      dpyinfo->last_mouse_scroll_bar = NULL;

#if 0
      /* Figure out which root window we're on.  */
      XQueryPointer (FRAME_X_DISPLAY (*fp),
		     DefaultRootWindow (FRAME_X_DISPLAY (*fp)),

		     /* The root window which contains the pointer.  */
		     &root,

		     /* Trash which we can't trust if the pointer is on
			a different screen.  */
		     &dummy_window,

		     /* The position on that root window.  */
		     &root_x, &root_y,

		     /* More trash we can't trust.  */
		     &dummy, &dummy,

		     /* Modifier keys and pointer buttons, about which
			we don't care.  */
		     (unsigned int *) &dummy);

      /* Now we have a position on the root; find the innermost window
	 containing the pointer.  */
      {
	Window win, child;
	Window first_win = 0;
	int win_x, win_y;
	int parent_x = 0, parent_y = 0;

	win = root;

	/* XTranslateCoordinates can get errors if the window
	   structure is changing at the same time this function
	   is running.  So at least we must not crash from them.  */

	x_catch_errors (FRAME_X_DISPLAY (*fp));

	if (x_mouse_grabbed (dpyinfo))
	  {
	    /* If mouse was grabbed on a frame, give coords for that frame
	       even if the mouse is now outside it.  */
	    XTranslateCoordinates (FRAME_X_DISPLAY (*fp),

				   /* From-window.  */
				   root,

				   /* To-window.  */
				   FRAME_X_WINDOW (dpyinfo->last_mouse_frame),

				   /* From-position, to-position.  */
				   root_x, root_y, &win_x, &win_y,

				   /* Child of win.  */
				   &child);
	    f1 = dpyinfo->last_mouse_frame;
	  }
	else
	  {
	    while (true)
	      {
		XTranslateCoordinates (FRAME_X_DISPLAY (*fp),

				       /* From-window, to-window.  */
				       root, win,

				       /* From-position, to-position.  */
				       root_x, root_y, &win_x, &win_y,

				       /* Child of win.  */
				       &child);

		if (child == None || child == win)
		  {
		    /* On GTK we have not inspected WIN yet.  If it has
		       a frame and that frame has a parent, use it.  */
		    struct frame *f = x_window_to_frame (dpyinfo, win);

		    if (f && FRAME_PARENT_FRAME (f))
		      first_win = win;
		    break;
		  }
		/* We don't wan't to know the innermost window.  We
		   want the edit window.  For non-Gtk+ the innermost
		   window is the edit window.  For Gtk+ it might not
		   be.  It might be the tool bar for example.  */
		if (x_window_to_frame (dpyinfo, win))
		  /* But don't hurry.  We might find a child frame
		     beneath.  */
		  first_win = win;
		win = child;
		parent_x = win_x;
		parent_y = win_y;
	      }

	    if (first_win)
	      win = first_win;

	    /* Now we know that:
	       win is the innermost window containing the pointer
	       (XTC says it has no child containing the pointer),
	       win_x and win_y are the pointer's position in it
	       (XTC did this the last time through), and
	       parent_x and parent_y are the pointer's position in win's parent.
	       (They are what win_x and win_y were when win was child.
	       If win is the root window, it has no parent, and
	       parent_{x,y} are invalid, but that's okay, because we'll
	       never use them in that case.)  */

	    /* We don't wan't to know the innermost window.  We
	       want the edit window.  */
	    f1 = x_window_to_frame (dpyinfo, win);
	  }

	if (x_had_errors_p (FRAME_X_DISPLAY (*fp)))
	  f1 = 0;

	x_uncatch_errors_after_check ();

#if 0
	/* If not, is it one of our scroll bars?  */
	if (! f1)
	  {
	    struct scroll_bar *bar;

            bar = x_window_to_scroll_bar (FRAME_X_DISPLAY (*fp), win, 2);

	    if (bar)
	      {
		f1 = XFRAME (WINDOW_FRAME (XWINDOW (bar->window)));
		win_x = parent_x;
		win_y = parent_y;
	      }
	  }
#endif

	if (f1 == 0 && insist > 0)
	  f1 = SELECTED_FRAME ();

	if (f1)
	  {
	    /* Ok, we found a frame.  Store all the values.
	       last_mouse_glyph is a rectangle used to reduce the
	       generation of mouse events.  To not miss any motion
	       events, we must divide the frame into rectangles of the
	       size of the smallest character that could be displayed
	       on it, i.e. into the same rectangles that matrices on
	       the frame are divided into.  */

	    /* FIXME: what if F1 is not an X frame?  */
	    dpyinfo = FRAME_DISPLAY_INFO (f1);
	    remember_mouse_glyph (f1, win_x, win_y, &dpyinfo->last_mouse_glyph);
	    dpyinfo->last_mouse_glyph_frame = f1;

	    *bar_window = Qnil;
	    *part = 0;
	    *fp = f1;
	    XSETINT (*x, win_x);
	    XSETINT (*y, win_y);
	    *timestamp = dpyinfo->last_mouse_movement_time;
	  }
      }
#endif
    }

  unblock_input ();
}

/* Fringe bitmaps.  */

static int max_fringe_bmp = 0;
static cairo_pattern_t **fringe_bmp = 0;

static void
pgtk_define_fringe_bitmap (int which, unsigned short *bits, int h, int wd)
{
  int i, stride;
  cairo_surface_t *surface;
  unsigned char *data;
  cairo_pattern_t *pattern;

  if (which >= max_fringe_bmp)
    {
      i = max_fringe_bmp;
      max_fringe_bmp = which + 20;
      fringe_bmp = (cairo_pattern_t **) xrealloc (fringe_bmp, max_fringe_bmp * sizeof (cairo_pattern_t *));
      while (i < max_fringe_bmp)
	fringe_bmp[i++] = 0;
    }

  block_input ();

  surface = cairo_image_surface_create (CAIRO_FORMAT_A1, wd, h);
  stride = cairo_image_surface_get_stride (surface);
  data = cairo_image_surface_get_data (surface);

  for (i = 0; i < h; i++)
    {
      *((unsigned short *) data) = bits[i];
      data += stride;
    }

  cairo_surface_mark_dirty (surface);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_surface_destroy (surface);

  unblock_input ();

  fringe_bmp[which] = pattern;
}

static void
pgtk_destroy_fringe_bitmap (int which)
{
  if (which >= max_fringe_bmp)
    return;

  if (fringe_bmp[which])
    {
      block_input ();
      cairo_pattern_destroy (fringe_bmp[which]);
      unblock_input ();
    }
  fringe_bmp[which] = 0;
}

static void
pgtk_clip_to_row (struct window *w, struct glyph_row *row,
		    enum glyph_row_area area, cairo_t *cr)
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  int window_x, window_y, window_width;
  cairo_rectangle_int_t rect;

  window_box (w, area, &window_x, &window_y, &window_width, 0);

  rect.x = window_x;
  rect.y = WINDOW_TO_FRAME_PIXEL_Y (w, max (0, row->y));
  rect.y = max (rect.y, window_y);
  rect.width = window_width;
  rect.height = row->visible_height;

  cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
  cairo_clip(cr);
}

static void
pgtk_cr_draw_image (struct frame *f, GC gc, cairo_pattern_t *image,
		 int src_x, int src_y, int width, int height,
		 int dest_x, int dest_y, bool overlay_p)
{
  cairo_t *cr;
  cairo_matrix_t matrix;
  cairo_surface_t *surface;
  cairo_format_t format;

  PGTK_TRACE("pgtk_cr_draw_image: 0: %d,%d,%d,%d,%d,%d,%d.", src_x, src_y, width, height, dest_x, dest_y, overlay_p);
  cr = pgtk_begin_cr_clip (f, gc);
  if (overlay_p)
    cairo_rectangle (cr, dest_x, dest_y, width, height);
  else
    {
      pgtk_set_cr_source_with_gc_background (f, gc);
      cairo_rectangle (cr, dest_x, dest_y, width, height);
      cairo_fill_preserve (cr);
    }
  cairo_clip (cr);
  cairo_matrix_init_translate (&matrix, src_x - dest_x, src_y - dest_y);
  cairo_pattern_set_matrix (image, &matrix);
  cairo_pattern_get_surface (image, &surface);
  format = cairo_image_surface_get_format (surface);
  if (format != CAIRO_FORMAT_A8 && format != CAIRO_FORMAT_A1)
    {
      PGTK_TRACE("other format.");
      cairo_set_source (cr, image);
      cairo_fill (cr);
    }
  else
    {
      if (format == CAIRO_FORMAT_A8)
	PGTK_TRACE("format A8.");
      else if (format == CAIRO_FORMAT_A1)
	PGTK_TRACE("format A1.");
      else
	PGTK_TRACE("format ??.");
      pgtk_set_cr_source_with_gc_foreground (f, gc);
      cairo_rectangle_list_t *rects = cairo_copy_clip_rectangle_list(cr);
      PGTK_TRACE("rects:");
      PGTK_TRACE(" status: %u", rects->status);
      PGTK_TRACE(" rectangles:");
      for (int i = 0; i < rects->num_rectangles; i++) {
	PGTK_TRACE("  %fx%f+%f+%f",
		rects->rectangles[i].width,
		rects->rectangles[i].height,
		rects->rectangles[i].x,
		rects->rectangles[i].y);
      }
      cairo_rectangle_list_destroy(rects);
      cairo_mask (cr, image);
    }
  pgtk_end_cr_clip (f);
  PGTK_TRACE("pgtk_cr_draw_image: 9.");
}

static void
pgtk_draw_fringe_bitmap (struct window *w, struct glyph_row *row, struct draw_fringe_bitmap_params *p)
{
  PGTK_TRACE("draw_fringe_bitmap.");

  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct face *face = p->face;

  cairo_t *cr = pgtk_begin_cr_clip(f, NULL);
  cairo_save(cr);

  /* Must clip because of partially visible lines.  */
  pgtk_clip_to_row (w, row, ANY_AREA, cr);

  if (p->bx >= 0 && !p->overlay_p)
    {
      /* In case the same realized face is used for fringes and
	 for something displayed in the text (e.g. face `region' on
	 mono-displays, the fill style may have been changed to
	 FillSolid in x_draw_glyph_string_background.  */
#if 0
      if (face->stipple)
	XSetFillStyle (display, face->gc, FillOpaqueStippled);
      else
#endif
	pgtk_set_cr_source_with_color(f, face->background);

      cairo_rectangle(cr, p->bx, p->by, p->nx, p->ny);
      cairo_fill(cr);
    }

  PGTK_TRACE("which: %d, max_fringe_bmp: %d.", p->which, max_fringe_bmp);
  if (p->which && p->which < max_fringe_bmp)
    {
      XGCValues gcv;

      PGTK_TRACE("cursor_p=%d.", p->cursor_p);
      PGTK_TRACE("overlay_p_p=%d.", p->overlay_p);
      PGTK_TRACE("background=%08lx.", face->background);
      PGTK_TRACE("cursor_color=%08lx.", f->output_data.pgtk->cursor_color);
      PGTK_TRACE("foreground=%08lx.", face->foreground);
      gcv.foreground = (p->cursor_p
		       ? (p->overlay_p ? face->background
			  : f->output_data.pgtk->cursor_color)
		       : face->foreground);
      gcv.background = face->background;
      pgtk_cr_draw_image (f, &gcv, fringe_bmp[p->which], 0, p->dh,
		       p->wd, p->h, p->x, p->y, p->overlay_p);
    }

  cairo_restore(cr);
}



extern frame_parm_handler pgtk_frame_parm_handlers[];

static struct redisplay_interface pgtk_redisplay_interface =
{
  pgtk_frame_parm_handlers,
  x_produce_glyphs,
  x_write_glyphs,
  x_insert_glyphs,
  x_clear_end_of_line,
  pgtk_scroll_run,
  pgtk_after_update_window_line,
  pgtk_update_window_begin,
  pgtk_update_window_end,
  0, /* flush_display */
  x_clear_window_mouse_face,
  x_get_glyph_overhangs,
  x_fix_overlapping_area,
  pgtk_draw_fringe_bitmap,
  pgtk_define_fringe_bitmap,
  pgtk_destroy_fringe_bitmap,
  NULL, // pgtk_compute_glyph_string_overhangs,
  pgtk_draw_glyph_string,
  pgtk_define_frame_cursor,
  pgtk_clear_frame_area,
  pgtk_draw_window_cursor,
  NULL, // pgtk_draw_vertical_window_border,
  NULL, // pgtk_draw_window_divider,
  NULL, // pgtk_shift_glyphs_for_insert,
  NULL, // pgtk_show_hourglass,
  NULL, // pgtk_hide_hourglass
};

static void
pgtk_redraw_scroll_bars (struct frame *f)
{
  PGTK_TRACE("pgtk_redraw_scroll_bars");
}

void
pgtk_clear_frame (struct frame *f)
/* --------------------------------------------------------------------------
      External (hook): Erase the entire frame
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("pgtk_clear_frame");
 /* comes on initial frame because we have
    after-make-frame-functions = select-frame */
  if (!FRAME_DEFAULT_FACE (f))
    return;

  // mark_window_cursors_off (XWINDOW (FRAME_ROOT_WINDOW (f)));

  block_input ();

  pgtk_clear_area(f, 0, 0, FRAME_PIXEL_WIDTH(f), FRAME_PIXEL_HEIGHT(f));

  /* as of 2006/11 or so this is now needed */
  pgtk_redraw_scroll_bars (f);
  unblock_input ();
}

/* Read events coming from the X server.
   Return as soon as there are no more events to be read.

   Return the number of characters stored into the buffer,
   thus pretending to be `read' (except the characters we store
   in the keyboard buffer can be multibyte, so are not necessarily
   C chars).  */

static int
pgtk_read_socket (struct terminal *terminal, struct input_event *hold_quit)
{
  PGTK_TRACE("pgtk_read_socket");
  int count = 0;
  bool event_found = false;
  struct pgtk_display_info *dpyinfo = terminal->display_info.pgtk;

  PGTK_TRACE("pgtk_read_socket: 1: errno=%d.", errno);
  if (input_blocked_p ()) {
    PGTK_TRACE("pgtk_read_socket: 2: errno=%d.", errno);
    block_input ();
    PGTK_TRACE("pgtk_read_socket: 3: errno=%d.", errno);

#if 0
  /* For debugging, this gives a way to fake an I/O error.  */
  if (dpyinfo == XTread_socket_fake_io_error)
    {
      XTread_socket_fake_io_error = 0;
      x_io_error_quitter (dpyinfo->display);
    }
#endif

  /* For GTK we must use the GTK event loop.  But XEvents gets passed
     to our filter function above, and then to the big event switch.
     We use a bunch of globals to communicate with our filter function,
     that is kind of ugly, but it works.

     There is no way to do one display at the time, GTK just does events
     from all displays.  */

  static int ctr = 0;

  // PGTK_TRACE("gtk main... %d.", ctr++);
  while (gtk_events_pending ())
    {
#if 0
      current_count = count;
      current_hold_quit = hold_quit;
#endif

      gtk_main_iteration ();

#if 0
      count = current_count;
      current_count = -1;
      current_hold_quit = 0;

      if (current_finish == X_EVENT_GOTO_OUT)
        break;
#endif
    }
  // PGTK_TRACE("gtk main... end.");

#if 0
  /* On some systems, an X bug causes Emacs to get no more events
     when the window is destroyed.  Detect that.  (1994.)  */
  if (! event_found)
    {
      /* Emacs and the X Server eats up CPU time if XNoOp is done every time.
	 One XNOOP in 100 loops will make Emacs terminate.
	 B. Bretthauer, 1994 */
      x_noop_count++;
      if (x_noop_count >= 100)
	{
	  x_noop_count=0;

	  if (next_noop_dpyinfo == 0)
	    next_noop_dpyinfo = x_display_list;

	  XNoOp (next_noop_dpyinfo->display);

	  /* Each time we get here, cycle through the displays now open.  */
	  next_noop_dpyinfo = next_noop_dpyinfo->next;
	}
    }
#endif

#if 0
  /* If the focus was just given to an auto-raising frame,
     raise it now.  FIXME: handle more than one such frame.  */
  if (dpyinfo->x_pending_autoraise_frame)
    {
      x_raise_frame (dpyinfo->x_pending_autoraise_frame);
      dpyinfo->x_pending_autoraise_frame = NULL;
    }
#endif

    PGTK_TRACE("pgtk_read_socket: 7: errno=%d.", errno);
    unblock_input ();
  }
  PGTK_TRACE("pgtk_read_socket: 8: errno=%d.", errno);

  PGTK_TRACE("pgtk_read_socket: 9: errno=%d.", errno);
  return count;
}



static struct terminal *
pgtk_create_terminal (struct pgtk_display_info *dpyinfo)
/* --------------------------------------------------------------------------
      Set up use of NS before we make the first connection.
   -------------------------------------------------------------------------- */
{
  struct terminal *terminal;

  terminal = create_terminal (output_pgtk, &pgtk_redisplay_interface);

  terminal->display_info.pgtk = dpyinfo;
  dpyinfo->terminal = terminal;

  terminal->clear_frame_hook = pgtk_clear_frame;
  // terminal->ring_bell_hook = pgtk_ring_bell;
  terminal->update_begin_hook = pgtk_update_begin;
  terminal->update_end_hook = pgtk_update_end;
  terminal->read_socket_hook = pgtk_read_socket;
  // terminal->frame_up_to_date_hook = pgtk_frame_up_to_date;
  terminal->mouse_position_hook = pgtk_mouse_position;
  // terminal->frame_rehighlight_hook = pgtk_frame_rehighlight;
  // terminal->frame_raise_lower_hook = pgtk_frame_raise_lower;
  // terminal->fullscreen_hook = pgtk_fullscreen_hook;
  // terminal->menu_show_hook = pgtk_menu_show;
  // terminal->popup_dialog_hook = pgtk_popup_dialog;
  // terminal->set_vertical_scroll_bar_hook = pgtk_set_vertical_scroll_bar;
  // terminal->set_horizontal_scroll_bar_hook = pgtk_set_horizontal_scroll_bar;
  // terminal->condemn_scroll_bars_hook = pgtk_condemn_scroll_bars;
  // terminal->redeem_scroll_bar_hook = pgtk_redeem_scroll_bar;
  // terminal->judge_scroll_bars_hook = pgtk_judge_scroll_bars;
  terminal->delete_frame_hook = x_destroy_window;
  // terminal->delete_terminal_hook = pgtk_delete_terminal;
  /* Other hooks are NULL by default.  */

  return terminal;
}

/* Like x_window_to_frame but also compares the window with the widget's
   windows.  */
static struct frame *
pgtk_any_window_to_frame (GdkWindow *window)
{
  Lisp_Object tail, frame;
  struct frame *f, *found = NULL;
  struct pgtk_output *x;

  if (window == NULL)
    return NULL;

  FOR_EACH_FRAME (tail, frame)
    {
      if (found)
        break;
      f = XFRAME (frame);
      if (FRAME_PGTK_P (f))
	{
#if 1
	  if (FRAME_GTK_OUTER_WIDGET(f) && gtk_widget_get_window(FRAME_GTK_OUTER_WIDGET(f)) == window)
	    found = f;
	  if (FRAME_GTK_WIDGET(f) && gtk_widget_get_window(FRAME_GTK_WIDGET(f)) == window)
	    found = f;
#else
	  /* This frame matches if the window is any of its widgets.  */
	  x = f->output_data.pgtk;
	  if (x->hourglass_window == wdesc)
	    found = f;
	  else if (x->widget)
	    {
              GtkWidget *gwdesc = xg_win_to_widget (dpyinfo->display, wdesc);
              if (gwdesc != 0
                  && gtk_widget_get_toplevel (gwdesc) == x->widget)
                found = f;
	    }
	  else if (FRAME_GTK_WIDGET (f) == widget)
	    /* A tooltip frame.  */
	    found = f;
#endif
	}
    }

  return found;
}

#if 0
/* Handles the XEvent EVENT on display DPYINFO.

   *FINISH is X_EVENT_GOTO_OUT if caller should stop reading events.
   *FINISH is zero if caller should continue reading events.
   *FINISH is X_EVENT_DROP if event should not be passed to the toolkit.
   *EVENT is unchanged unless we're processing KeyPress event.

   We return the number of characters stored into the buffer.  */

static int
handle_one_event (struct x_display_info *dpyinfo,
		  const GdkEvent *event,
		  int *finish, struct input_event *hold_quit)
{
  union buffered_input_event inev;
  int count = 0;
  int do_help = 0;
  ptrdiff_t nbytes = 0;
  struct frame *any, *f = NULL;
  struct coding_system coding;
  Mouse_HLInfo *hlinfo = &dpyinfo->mouse_highlight;
  /* This holds the state XLookupString needs to implement dead keys
     and other tricks known as "compose processing".  _X Window System_
     says that a portable program can't use this, but Stephen Gildea assures
     me that letting the compiler initialize it to zeros will work okay.  */
  static XComposeStatus compose_status;
  XEvent configureEvent;
  XEvent next_event;

  USE_SAFE_ALLOCA;

  *finish = X_EVENT_NORMAL;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  any = x_any_window_to_frame (dpyinfo, event->xany.window);

  if (any && any->wait_event_type == event->type)
    any->wait_event_type = 0; /* Indicates we got it.  */

  switch (event->type)
    {
    case ClientMessage:
      {
        if (event->xclient.message_type == dpyinfo->Xatom_wm_protocols
            && event->xclient.format == 32)
          {
            if (event->xclient.data.l[0] == dpyinfo->Xatom_wm_take_focus)
              {
                /* Use the value returned by x_any_window_to_frame
		   because this could be the shell widget window
		   if the frame has no title bar.  */
                f = any;
		goto done;
              }

            if (event->xclient.data.l[0] == dpyinfo->Xatom_wm_save_yourself)
              {
                /* Save state modify the WM_COMMAND property to
                   something which can reinstate us.  This notifies
                   the session manager, who's looking for such a
                   PropertyNotify.  Can restart processing when
                   a keyboard or mouse event arrives.  */
                /* If we have a session manager, don't set this.
                   KDE will then start two Emacsen, one for the
                   session manager and one for this. */
#ifdef HAVE_X_SM
                if (! x_session_have_connection ())
#endif
                  {
                    f = x_top_window_to_frame (dpyinfo,
                                               event->xclient.window);
                    /* This is just so we only give real data once
                       for a single Emacs process.  */
                    if (f == SELECTED_FRAME ())
                      XSetCommand (FRAME_X_DISPLAY (f),
                                   event->xclient.window,
                                   initial_argv, initial_argc);
                    else if (f)
                      XSetCommand (FRAME_X_DISPLAY (f),
                                   event->xclient.window,
                                   0, 0);
                  }
		goto done;
              }

            if (event->xclient.data.l[0] == dpyinfo->Xatom_wm_delete_window)
              {
                f = any;
                if (!f)
		  goto OTHER; /* May be a dialog that is to be removed  */

		inev.ie.kind = DELETE_WINDOW_EVENT;
		XSETFRAME (inev.ie.frame_or_window, f);
		goto done;
              }

	    goto done;
          }

        if (event->xclient.message_type == dpyinfo->Xatom_wm_configure_denied)
	  goto done;

        if (event->xclient.message_type == dpyinfo->Xatom_wm_window_moved)
          {
            int new_x, new_y;
	    f = x_window_to_frame (dpyinfo, event->xclient.window);

            new_x = event->xclient.data.s[0];
            new_y = event->xclient.data.s[1];

            if (f)
              {
                f->left_pos = new_x;
                f->top_pos = new_y;
              }
	    goto done;
          }

        if (event->xclient.message_type == dpyinfo->Xatom_DONE
	    || event->xclient.message_type == dpyinfo->Xatom_PAGE)
          {
            /* Ghostview job completed.  Kill it.  We could
               reply with "Next" if we received "Page", but we
               currently never do because we are interested in
               images, only, which should have 1 page.  */
            Pixmap pixmap = (Pixmap) event->xclient.data.l[1];
	    f = x_window_to_frame (dpyinfo, event->xclient.window);
	    if (!f)
	      goto OTHER;
            x_kill_gs_process (pixmap, f);
            expose_frame (f, 0, 0, 0, 0);
	    goto done;
          }

	/* XEmbed messages from the embedder (if any).  */
        if (event->xclient.message_type == dpyinfo->Xatom_XEMBED)
          {
	    enum xembed_message msg = event->xclient.data.l[1];
	    if (msg == XEMBED_FOCUS_IN || msg == XEMBED_FOCUS_OUT)
	      x_detect_focus_change (dpyinfo, any, event, &inev.ie);

	    *finish = X_EVENT_GOTO_OUT;
            goto done;
          }

        xft_settings_event (dpyinfo, event);

	f = any;
	if (!f)
	  goto OTHER;
	if (x_handle_dnd_message (f, &event->xclient, dpyinfo, &inev.ie))
	  *finish = X_EVENT_DROP;
      }
      break;

    case SelectionNotify:
      x_display_set_last_user_time (dpyinfo, event->xselection.time);
      x_handle_selection_notify (&event->xselection);
      break;

    case SelectionClear:	/* Someone has grabbed ownership.  */
      x_display_set_last_user_time (dpyinfo, event->xselectionclear.time);
      {
        const XSelectionClearEvent *eventp = &event->xselectionclear;

        inev.sie.kind = SELECTION_CLEAR_EVENT;
        SELECTION_EVENT_DPYINFO (&inev.sie) = dpyinfo;
        SELECTION_EVENT_SELECTION (&inev.sie) = eventp->selection;
        SELECTION_EVENT_TIME (&inev.sie) = eventp->time;
      }
      break;

    case SelectionRequest:	/* Someone wants our selection.  */
      x_display_set_last_user_time (dpyinfo, event->xselectionrequest.time);
      {
	const XSelectionRequestEvent *eventp = &event->xselectionrequest;

	inev.sie.kind = SELECTION_REQUEST_EVENT;
	SELECTION_EVENT_DPYINFO (&inev.sie) = dpyinfo;
	SELECTION_EVENT_REQUESTOR (&inev.sie) = eventp->requestor;
	SELECTION_EVENT_SELECTION (&inev.sie) = eventp->selection;
	SELECTION_EVENT_TARGET (&inev.sie) = eventp->target;
	SELECTION_EVENT_PROPERTY (&inev.sie) = eventp->property;
	SELECTION_EVENT_TIME (&inev.sie) = eventp->time;
      }
      break;

    case PropertyNotify:
      x_display_set_last_user_time (dpyinfo, event->xproperty.time);
      f = x_top_window_to_frame (dpyinfo, event->xproperty.window);
      if (f && event->xproperty.atom == dpyinfo->Xatom_net_wm_state)
	{
          bool not_hidden = x_handle_net_wm_state (f, &event->xproperty);
	  if (not_hidden && FRAME_ICONIFIED_P (f))
	    {
	      /* Gnome shell does not iconify us when C-z is pressed.
		 It hides the frame.  So if our state says we aren't
		 hidden anymore, treat it as deiconified.  */
	      SET_FRAME_VISIBLE (f, 1);
	      SET_FRAME_ICONIFIED (f, false);
	      f->output_data.x->has_been_visible = true;
	      inev.ie.kind = DEICONIFY_EVENT;
	      XSETFRAME (inev.ie.frame_or_window, f);
	    }
	  else if (! not_hidden && ! FRAME_ICONIFIED_P (f))
	    {
	      SET_FRAME_VISIBLE (f, 0);
	      SET_FRAME_ICONIFIED (f, true);
	      inev.ie.kind = ICONIFY_EVENT;
	      XSETFRAME (inev.ie.frame_or_window, f);
	    }
	}

      x_handle_property_notify (&event->xproperty);
      xft_settings_event (dpyinfo, event);
      goto OTHER;

    case ReparentNotify:
      f = x_top_window_to_frame (dpyinfo, event->xreparent.window);
      if (f)
        {
	  /* Maybe we shouldn't set this for child frames ??  */
	  f->output_data.x->parent_desc = event->xreparent.parent;
	  if (!FRAME_PARENT_FRAME (f))
	    x_real_positions (f, &f->left_pos, &f->top_pos);
	  else
	    {
	      Window root;
	      unsigned int dummy_uint;

	      block_input ();
	      XGetGeometry (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f),
			    &root, &f->left_pos, &f->top_pos,
			    &dummy_uint, &dummy_uint, &dummy_uint, &dummy_uint);
	      unblock_input ();
	    }

          /* Perhaps reparented due to a WM restart.  Reset this.  */
          FRAME_DISPLAY_INFO (f)->wm_type = X_WMTYPE_UNKNOWN;
          FRAME_DISPLAY_INFO (f)->net_supported_window = 0;

          x_set_frame_alpha (f);
        }
      goto OTHER;

    case Expose:
      f = x_window_to_frame (dpyinfo, event->xexpose.window);
      if (f)
        {
          if (!FRAME_VISIBLE_P (f))
            {
              block_input ();
              SET_FRAME_VISIBLE (f, 1);
              SET_FRAME_ICONIFIED (f, false);
              if (FRAME_X_DOUBLE_BUFFERED_P (f))
                font_drop_xrender_surfaces (f);
              f->output_data.x->has_been_visible = true;
              SET_FRAME_GARBAGED (f);
              unblock_input ();
            }
          else if (FRAME_GARBAGED_P (f))
            {
              /* Go around the back buffer and manually clear the
                 window the first time we show it.  This way, we avoid
                 showing users the sanity-defying horror of whatever
                 GtkWindow is rendering beneath us.  We've garbaged
                 the frame, so we'll redraw the whole thing on next
                 redisplay anyway.  Yuck.  */
              x_clear_area1 (
                FRAME_X_DISPLAY (f),
                FRAME_X_WINDOW (f),
                event->xexpose.x, event->xexpose.y,
                event->xexpose.width, event->xexpose.height,
                0);
	      x_clear_under_internal_border (f);
            }


          if (!FRAME_GARBAGED_P (f))
            {
              /* This seems to be needed for GTK 2.6 and later, see
                 https://debbugs.gnu.org/cgi/bugreport.cgi?bug=15398.  */
              x_clear_area (f,
                            event->xexpose.x, event->xexpose.y,
                            event->xexpose.width, event->xexpose.height);
              expose_frame (f, event->xexpose.x, event->xexpose.y,
			    event->xexpose.width, event->xexpose.height);
	      x_clear_under_internal_border (f);
            }

          if (!FRAME_GARBAGED_P (f))
            show_back_buffer (f);
        }
      else
        {
          struct scroll_bar *bar;

          bar = x_window_to_scroll_bar (event->xexpose.display,
                                        event->xexpose.window, 2);

          if (bar)
            x_scroll_bar_expose (bar, event);
        }
      break;

    case GraphicsExpose:	/* This occurs when an XCopyArea's
                                   source area was obscured or not
                                   available.  */
      f = x_window_to_frame (dpyinfo, event->xgraphicsexpose.drawable);
      if (f)
        {
          expose_frame (f, event->xgraphicsexpose.x,
                        event->xgraphicsexpose.y,
                        event->xgraphicsexpose.width,
                        event->xgraphicsexpose.height);
	  x_clear_under_internal_border (f);
	  show_back_buffer (f);
        }
      break;

    case NoExpose:		/* This occurs when an XCopyArea's
                                   source area was completely
                                   available.  */
      break;

    case UnmapNotify:
      /* Redo the mouse-highlight after the tooltip has gone.  */
      if (event->xunmap.window == tip_window)
        {
          tip_window = 0;
          x_redo_mouse_highlight (dpyinfo);
        }

      f = x_top_window_to_frame (dpyinfo, event->xunmap.window);
      if (f)		/* F may no longer exist if
                           the frame was deleted.  */
        {
	  bool visible = FRAME_VISIBLE_P (f);
          /* While a frame is unmapped, display generation is
             disabled; you don't want to spend time updating a
             display that won't ever be seen.  */
          SET_FRAME_VISIBLE (f, 0);
          /* We can't distinguish, from the event, whether the window
             has become iconified or invisible.  So assume, if it
             was previously visible, than now it is iconified.
             But x_make_frame_invisible clears both
             the visible flag and the iconified flag;
             and that way, we know the window is not iconified now.  */
          if (visible || FRAME_ICONIFIED_P (f))
            {
              SET_FRAME_ICONIFIED (f, true);
              inev.ie.kind = ICONIFY_EVENT;
              XSETFRAME (inev.ie.frame_or_window, f);
            }
        }
      goto OTHER;

    case MapNotify:
      /* We use x_top_window_to_frame because map events can
         come for sub-windows and they don't mean that the
         frame is visible.  */
      f = x_top_window_to_frame (dpyinfo, event->xmap.window);
      if (f)
        {
	  bool iconified = FRAME_ICONIFIED_P (f);

          /* Check if fullscreen was specified before we where mapped the
             first time, i.e. from the command line.  */
          if (!f->output_data.x->has_been_visible)
	    {

	      x_check_fullscreen (f);
	    }

	  if (!iconified)
	    {
	      /* The `z-group' is reset every time a frame becomes
		 invisible.  Handle this here.  */
	      if (FRAME_Z_GROUP (f) == z_group_above)
		x_set_z_group (f, Qabove, Qnil);
	      else if (FRAME_Z_GROUP (f) == z_group_below)
		x_set_z_group (f, Qbelow, Qnil);
	    }

          SET_FRAME_VISIBLE (f, 1);
          SET_FRAME_ICONIFIED (f, false);
          f->output_data.x->has_been_visible = true;

          if (iconified)
            {
              inev.ie.kind = DEICONIFY_EVENT;
              XSETFRAME (inev.ie.frame_or_window, f);
            }
          else if (! NILP (Vframe_list) && ! NILP (XCDR (Vframe_list)))
            /* Force a redisplay sooner or later to update the
	       frame titles in case this is the second frame.  */
            record_asynch_buffer_change ();
        }
      goto OTHER;

    case KeyPress:

      x_display_set_last_user_time (dpyinfo, event->xkey.time);
      ignore_next_mouse_click_timeout = 0;

      /* Dispatch KeyPress events when in menu.  */
      if (popup_activated ())
        goto OTHER;

      f = any;

      /* If mouse-highlight is an integer, input clears out
	 mouse highlighting.  */
      if (!hlinfo->mouse_face_hidden && INTEGERP (Vmouse_highlight)
	  )
        {
          clear_mouse_face (hlinfo);
          hlinfo->mouse_face_hidden = true;
        }

      if (f != 0)
        {
          KeySym keysym, orig_keysym;
          /* al%imercury@uunet.uu.net says that making this 81
             instead of 80 fixed a bug whereby meta chars made
             his Emacs hang.

             It seems that some version of XmbLookupString has
             a bug of not returning XBufferOverflow in
             status_return even if the input is too long to
             fit in 81 bytes.  So, we must prepare sufficient
             bytes for copy_buffer.  513 bytes (256 chars for
             two-byte character set) seems to be a fairly good
             approximation.  -- 2000.8.10 handa@etl.go.jp  */
          unsigned char copy_buffer[513];
          unsigned char *copy_bufptr = copy_buffer;
          int copy_bufsiz = sizeof (copy_buffer);
          int modifiers;
          Lisp_Object coding_system = Qlatin_1;
	  Lisp_Object c;
	  /* Event will be modified.  */
	  XKeyEvent xkey = event->xkey;

          /* Don't pass keys to GTK.  A Tab will shift focus to the
             tool bar in GTK 2.4.  Keys will still go to menus and
             dialogs because in that case popup_activated is nonzero
             (see above).  */
          *finish = X_EVENT_DROP;

          xkey.state |= x_emacs_to_x_modifiers (FRAME_DISPLAY_INFO (f),
						extra_keyboard_modifiers);
          modifiers = xkey.state;

          /* This will have to go some day...  */

          /* make_lispy_event turns chars into control chars.
             Don't do it here because XLookupString is too eager.  */
          xkey.state &= ~ControlMask;
          xkey.state &= ~(dpyinfo->meta_mod_mask
			  | dpyinfo->super_mod_mask
			  | dpyinfo->hyper_mod_mask
			  | dpyinfo->alt_mod_mask);

          /* In case Meta is ComposeCharacter,
             clear its status.  According to Markus Ehrnsperger
             Markus.Ehrnsperger@lehrstuhl-bross.physik.uni-muenchen.de
             this enables ComposeCharacter to work whether or
             not it is combined with Meta.  */
          if (modifiers & dpyinfo->meta_mod_mask)
            memset (&compose_status, 0, sizeof (compose_status));

          nbytes = XLookupString (&xkey, (char *) copy_bufptr,
                                  copy_bufsiz, &keysym,
                                  &compose_status);

          /* If not using XIM/XIC, and a compose sequence is in progress,
             we break here.  Otherwise, chars_matched is always 0.  */
          if (compose_status.chars_matched > 0 && nbytes == 0)
            break;

          memset (&compose_status, 0, sizeof (compose_status));
          orig_keysym = keysym;

	  /* Common for all keysym input events.  */
	  XSETFRAME (inev.ie.frame_or_window, f);
	  inev.ie.modifiers
	    = x_x_to_emacs_modifiers (FRAME_DISPLAY_INFO (f), modifiers);
	  inev.ie.timestamp = xkey.time;

	  /* First deal with keysyms which have defined
	     translations to characters.  */
	  if (keysym >= 32 && keysym < 128)
	    /* Avoid explicitly decoding each ASCII character.  */
	    {
	      inev.ie.kind = ASCII_KEYSTROKE_EVENT;
	      inev.ie.code = keysym;
	      goto done_keysym;
	    }

	  /* Keysyms directly mapped to Unicode characters.  */
	  if (keysym >= 0x01000000 && keysym <= 0x0110FFFF)
	    {
	      if (keysym < 0x01000080)
		inev.ie.kind = ASCII_KEYSTROKE_EVENT;
	      else
		inev.ie.kind = MULTIBYTE_CHAR_KEYSTROKE_EVENT;
	      inev.ie.code = keysym & 0xFFFFFF;
	      goto done_keysym;
	    }

	  /* Now non-ASCII.  */
	  if (HASH_TABLE_P (Vx_keysym_table)
	      && (c = Fgethash (make_number (keysym),
				Vx_keysym_table,
				Qnil),
		  NATNUMP (c)))
	    {
	      inev.ie.kind = (SINGLE_BYTE_CHAR_P (XFASTINT (c))
                              ? ASCII_KEYSTROKE_EVENT
                              : MULTIBYTE_CHAR_KEYSTROKE_EVENT);
	      inev.ie.code = XFASTINT (c);
	      goto done_keysym;
	    }

	  /* Random non-modifier sorts of keysyms.  */
	  if (((keysym >= XK_BackSpace && keysym <= XK_Escape)
                        || keysym == XK_Delete
#ifdef XK_ISO_Left_Tab
                        || (keysym >= XK_ISO_Left_Tab
                            && keysym <= XK_ISO_Enter)
#endif
                        || IsCursorKey (keysym) /* 0xff50 <= x < 0xff60 */
                        || IsMiscFunctionKey (keysym) /* 0xff60 <= x < VARIES */
#ifdef HPUX
                        /* This recognizes the "extended function
                           keys".  It seems there's no cleaner way.
                           Test IsModifierKey to avoid handling
                           mode_switch incorrectly.  */
                        || (XK_Select <= keysym && keysym < XK_KP_Space)
#endif
#ifdef XK_dead_circumflex
                        || orig_keysym == XK_dead_circumflex
#endif
#ifdef XK_dead_grave
                        || orig_keysym == XK_dead_grave
#endif
#ifdef XK_dead_tilde
                        || orig_keysym == XK_dead_tilde
#endif
#ifdef XK_dead_diaeresis
                        || orig_keysym == XK_dead_diaeresis
#endif
#ifdef XK_dead_macron
                        || orig_keysym == XK_dead_macron
#endif
#ifdef XK_dead_degree
                        || orig_keysym == XK_dead_degree
#endif
#ifdef XK_dead_acute
                        || orig_keysym == XK_dead_acute
#endif
#ifdef XK_dead_cedilla
                        || orig_keysym == XK_dead_cedilla
#endif
#ifdef XK_dead_breve
                        || orig_keysym == XK_dead_breve
#endif
#ifdef XK_dead_ogonek
                        || orig_keysym == XK_dead_ogonek
#endif
#ifdef XK_dead_caron
                        || orig_keysym == XK_dead_caron
#endif
#ifdef XK_dead_doubleacute
                        || orig_keysym == XK_dead_doubleacute
#endif
#ifdef XK_dead_abovedot
                        || orig_keysym == XK_dead_abovedot
#endif
                        || IsKeypadKey (keysym) /* 0xff80 <= x < 0xffbe */
                        || IsFunctionKey (keysym) /* 0xffbe <= x < 0xffe1 */
                        /* Any "vendor-specific" key is ok.  */
                        || (orig_keysym & (1 << 28))
                        || (keysym != NoSymbol && nbytes == 0))
                       && ! (IsModifierKey (orig_keysym)
                             /* The symbols from XK_ISO_Lock
                                to XK_ISO_Last_Group_Lock
                                don't have real modifiers but
                                should be treated similarly to
                                Mode_switch by Emacs. */
#if defined XK_ISO_Lock && defined XK_ISO_Last_Group_Lock
                             || (XK_ISO_Lock <= orig_keysym
				 && orig_keysym <= XK_ISO_Last_Group_Lock)
#endif
                             ))
	    {
	      STORE_KEYSYM_FOR_DEBUG (keysym);
	      /* make_lispy_event will convert this to a symbolic
		 key.  */
	      inev.ie.kind = NON_ASCII_KEYSTROKE_EVENT;
	      inev.ie.code = keysym;
	      goto done_keysym;
	    }

	  {	/* Raw bytes, not keysym.  */
	    ptrdiff_t i;
	    int nchars, len;

	    for (i = 0, nchars = 0; i < nbytes; i++)
	      {
		if (ASCII_CHAR_P (copy_bufptr[i]))
		  nchars++;
		STORE_KEYSYM_FOR_DEBUG (copy_bufptr[i]);
	      }

	    if (nchars < nbytes)
	      {
		/* Decode the input data.  */

		/* The input should be decoded with `coding_system'
		   which depends on which X*LookupString function
		   we used just above and the locale.  */
		setup_coding_system (coding_system, &coding);
		coding.src_multibyte = false;
		coding.dst_multibyte = true;
		/* The input is converted to events, thus we can't
		   handle composition.  Anyway, there's no XIM that
		   gives us composition information.  */
		coding.common_flags &= ~CODING_ANNOTATION_MASK;

		SAFE_NALLOCA (coding.destination, MAX_MULTIBYTE_LENGTH,
			      nbytes);
		coding.dst_bytes = MAX_MULTIBYTE_LENGTH * nbytes;
		coding.mode |= CODING_MODE_LAST_BLOCK;
		decode_coding_c_string (&coding, copy_bufptr, nbytes, Qnil);
		nbytes = coding.produced;
		nchars = coding.produced_char;
		copy_bufptr = coding.destination;
	      }

	    /* Convert the input data to a sequence of
	       character events.  */
	    for (i = 0; i < nbytes; i += len)
	      {
		int ch;
		if (nchars == nbytes)
		  ch = copy_bufptr[i], len = 1;
		else
		  ch = STRING_CHAR_AND_LENGTH (copy_bufptr + i, len);
		inev.ie.kind = (SINGLE_BYTE_CHAR_P (ch)
				? ASCII_KEYSTROKE_EVENT
				: MULTIBYTE_CHAR_KEYSTROKE_EVENT);
		inev.ie.code = ch;
		kbd_buffer_store_buffered_event (&inev, hold_quit);
	      }

	    count += nchars;

	    inev.ie.kind = NO_EVENT;  /* Already stored above.  */

	    if (keysym == NoSymbol)
	      break;
	  }
	  /* FIXME: check side effects and remove this.  */
	  ((XEvent *) event)->xkey = xkey;
        }
    done_keysym:
      goto OTHER;

    case KeyRelease:
      x_display_set_last_user_time (dpyinfo, event->xkey.time);
      goto OTHER;

    case EnterNotify:
      x_display_set_last_user_time (dpyinfo, event->xcrossing.time);
      x_detect_focus_change (dpyinfo, any, event, &inev.ie);

      f = any;

      if (f && x_mouse_click_focus_ignore_position)
	ignore_next_mouse_click_timeout = event->xmotion.time + 200;

      /* EnterNotify counts as mouse movement,
	 so update things that depend on mouse position.  */
      if (f && !f->output_data.x->hourglass_p)
	note_mouse_movement (f, &event->xmotion);
      /* We may get an EnterNotify on the buttons in the toolbar.  In that
         case we moved out of any highlighted area and need to note this.  */
      if (!f && dpyinfo->last_mouse_glyph_frame)
        note_mouse_movement (dpyinfo->last_mouse_glyph_frame, &event->xmotion);
      goto OTHER;

    case FocusIn:
      x_detect_focus_change (dpyinfo, any, event, &inev.ie);
      goto OTHER;

    case LeaveNotify:
      x_display_set_last_user_time (dpyinfo, event->xcrossing.time);
      x_detect_focus_change (dpyinfo, any, event, &inev.ie);

      f = x_top_window_to_frame (dpyinfo, event->xcrossing.window);
      if (f)
        {
          if (f == hlinfo->mouse_face_mouse_frame)
            {
              /* If we move outside the frame, then we're
                 certainly no longer on any text in the frame.  */
              clear_mouse_face (hlinfo);
              hlinfo->mouse_face_mouse_frame = 0;
            }

          /* Generate a nil HELP_EVENT to cancel a help-echo.
             Do it only if there's something to cancel.
             Otherwise, the startup message is cleared when
             the mouse leaves the frame.  */
          if (any_help_event_p)
	    do_help = -1;
        }
      /* See comment in EnterNotify above */
      else if (dpyinfo->last_mouse_glyph_frame)
        note_mouse_movement (dpyinfo->last_mouse_glyph_frame, &event->xmotion);
      goto OTHER;

    case FocusOut:
      x_detect_focus_change (dpyinfo, any, event, &inev.ie);
      goto OTHER;

    case MotionNotify:
      {
        x_display_set_last_user_time (dpyinfo, event->xmotion.time);
        previous_help_echo_string = help_echo_string;
        help_echo_string = Qnil;

	f = (x_mouse_grabbed (dpyinfo) ? dpyinfo->last_mouse_frame
	     : x_window_to_frame (dpyinfo, event->xmotion.window));

        if (hlinfo->mouse_face_hidden)
          {
            hlinfo->mouse_face_hidden = false;
            clear_mouse_face (hlinfo);
          }

        if (f && xg_event_is_for_scrollbar (f, event))
          f = 0;
        if (f)
          {
	    /* Maybe generate a SELECT_WINDOW_EVENT for
	       `mouse-autoselect-window' but don't let popup menus
	       interfere with this (Bug#1261).  */
            if (!NILP (Vmouse_autoselect_window)
		&& !popup_activated ()
		/* Don't switch if we're currently in the minibuffer.
		   This tries to work around problems where the
		   minibuffer gets unselected unexpectedly, and where
		   you then have to move your mouse all the way down to
		   the minibuffer to select it.  */
		&& !MINI_WINDOW_P (XWINDOW (selected_window))
		/* With `focus-follows-mouse' non-nil create an event
		   also when the target window is on another frame.  */
		&& (f == XFRAME (selected_frame)
		    || !NILP (focus_follows_mouse)))
	      {
		static Lisp_Object last_mouse_window;
		Lisp_Object window = window_from_coordinates
		  (f, event->motion.x, event->motion.y, 0, false);

		/* A window will be autoselected only when it is not
		   selected now and the last mouse movement event was
		   not in it.  The remainder of the code is a bit vague
		   wrt what a "window" is.  For immediate autoselection,
		   the window is usually the entire window but for GTK
		   where the scroll bars don't count.  For delayed
		   autoselection the window is usually the window's text
		   area including the margins.  */
		if (WINDOWP (window)
		    && !EQ (window, last_mouse_window)
		    && !EQ (window, selected_window))
		  {
		    inev.ie.kind = SELECT_WINDOW_EVENT;
		    inev.ie.frame_or_window = window;
		  }

		/* Remember the last window where we saw the mouse.  */
		last_mouse_window = window;
	      }

            if (!note_mouse_movement (f, &event->motion))
	      help_echo_string = previous_help_echo_string;
          }
        else
          {
            /* If we move outside the frame, then we're
               certainly no longer on any text in the frame.  */
            clear_mouse_face (hlinfo);
          }

        /* If the contents of the global variable help_echo_string
           has changed, generate a HELP_EVENT.  */
        if (!NILP (help_echo_string)
            || !NILP (previous_help_echo_string))
	  do_help = 1;
        goto OTHER;
      }

    case ConfigureNotify:
      /* An opaque move can generate a stream of events as the window
         is dragged around.  If the connection round trip time isn't
         really short, they may come faster than we can respond to
         them, given the multiple queries we can do to check window
         manager state, translate coordinates, etc.

         So if this ConfigureNotify is immediately followed by another
         for the same window, use the info from the latest update, and
         consider the events all handled.  */
      /* Opaque resize may be trickier; ConfigureNotify events are
         mixed with Expose events for multiple windows.  */
      configureEvent = *event;
      while (XPending (dpyinfo->display))
        {
          XNextEvent (dpyinfo->display, &next_event);
          if (next_event.type != ConfigureNotify
              || next_event.xconfigure.window != event->xconfigure.window
              /* Skipping events with different sizes can lead to a
                 mispositioned mode line at initial window creation.
                 Only drop window motion events for now.  */
              || next_event.xconfigure.width != event->xconfigure.width
              || next_event.xconfigure.height != event->xconfigure.height)
            {
              XPutBackEvent (dpyinfo->display, &next_event);
              break;
            }
          else
	    configureEvent = next_event;
        }

      f = x_top_window_to_frame (dpyinfo, configureEvent.xconfigure.window);
      /* Unfortunately, we need to call font_drop_xrender_surfaces for
         _all_ ConfigureNotify events, otherwise we miss some and
         flicker.  Don't try to optimize these calls by looking only
         for size changes: that's not sufficient.  We miss some
         surface invalidations and flicker.  */
      block_input ();
      if (f && FRAME_X_DOUBLE_BUFFERED_P (f))
        font_drop_xrender_surfaces (f);
      unblock_input ();
      if (f) x_cr_destroy_surface (f);
      if (!f
          && (f = any)
          && configureEvent.xconfigure.window == FRAME_X_WINDOW (f))
        {
          block_input ();
          if (FRAME_X_DOUBLE_BUFFERED_P (f))
            font_drop_xrender_surfaces (f);
          unblock_input ();
          xg_frame_resized (f, configureEvent.xconfigure.width,
                            configureEvent.xconfigure.height);
          x_cr_destroy_surface (f);
          f = 0;
        }
      if (f)
        {
	  /* For GTK+ don't call x_net_wm_state for the scroll bar
	     window.  (Bug#24963, Bug#25887) */
	  if (configureEvent.xconfigure.window == FRAME_X_WINDOW (f))
	    x_net_wm_state (f, configureEvent.xconfigure.window);

          /* GTK creates windows but doesn't map them.
             Only get real positions when mapped.  */
          if (FRAME_GTK_OUTER_WIDGET (f)
              && gtk_widget_get_mapped (FRAME_GTK_OUTER_WIDGET (f)))
	    {
	      int old_left = f->left_pos;
	      int old_top = f->top_pos;
	      Lisp_Object frame = Qnil;

	      XSETFRAME (frame, f);

	      if (!FRAME_PARENT_FRAME (f))
		x_real_positions (f, &f->left_pos, &f->top_pos);
	      else
		{
		  Window root;
		  unsigned int dummy_uint;

		  block_input ();
		  XGetGeometry (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f),
				&root, &f->left_pos, &f->top_pos,
				&dummy_uint, &dummy_uint, &dummy_uint, &dummy_uint);
		  unblock_input ();
		}

	      if (old_left != f->left_pos || old_top != f->top_pos)
		{
		  inev.ie.kind = MOVE_FRAME_EVENT;
		  XSETFRAME (inev.ie.frame_or_window, f);
		}
	    }


        }
      goto OTHER;

    case ButtonRelease:
    case ButtonPress:
      {
        /* If we decide we want to generate an event to be seen
           by the rest of Emacs, we put it here.  */
        bool tool_bar_p = false;

	memset (&compose_status, 0, sizeof (compose_status));
	dpyinfo->last_mouse_glyph_frame = NULL;
	x_display_set_last_user_time (dpyinfo, event->xbutton.time);

	if (x_mouse_grabbed (dpyinfo))
	  f = dpyinfo->last_mouse_frame;
	else
	  {
	    f = x_window_to_frame (dpyinfo, event->xbutton.window);

	    if (f && event->xbutton.type == ButtonPress
		&& !popup_activated ()
		&& !x_window_to_scroll_bar (event->xbutton.display,
					    event->xbutton.window, 2)
		&& !FRAME_NO_ACCEPT_FOCUS (f))
	      {
		/* When clicking into a child frame or when clicking
		   into a parent frame with the child frame selected and
		   `no-accept-focus' is not set, select the clicked
		   frame.  */
		struct frame *hf = dpyinfo->x_highlight_frame;

		if (FRAME_PARENT_FRAME (f) || (hf && frame_ancestor_p (f, hf)))
		  {
		    block_input ();
		    XSetInputFocus (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f),
				    RevertToParent, CurrentTime);
		    if (FRAME_PARENT_FRAME (f))
		      XRaiseWindow (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f));
		    unblock_input ();
		  }
	      }
	  }

        if (f && xg_event_is_for_scrollbar (f, event))
          f = 0;
        if (f)
          {
            if (!tool_bar_p)
              if (! popup_activated ())
                {
                  if (ignore_next_mouse_click_timeout)
                    {
                      if (event->type == ButtonPress
                          && event->xbutton.time > ignore_next_mouse_click_timeout)
                        {
                          ignore_next_mouse_click_timeout = 0;
                          construct_mouse_click (&inev.ie, &event->xbutton, f);
                        }
                      if (event->type == ButtonRelease)
                        ignore_next_mouse_click_timeout = 0;
                    }
                  else
                    construct_mouse_click (&inev.ie, &event->xbutton, f);
                }
            if (FRAME_X_EMBEDDED_P (f))
              xembed_send_message (f, event->xbutton.time,
                                   XEMBED_REQUEST_FOCUS, 0, 0, 0);
          }
        else
          {
            struct scroll_bar *bar
              = x_window_to_scroll_bar (event->xbutton.display,
                                        event->xbutton.window, 2);

            if (bar)
              x_scroll_bar_handle_click (bar, event, &inev.ie);
          }

        if (event->type == ButtonPress)
          {
            dpyinfo->grabbed |= (1 << event->xbutton.button);
            dpyinfo->last_mouse_frame = f;
          }
        else
          dpyinfo->grabbed &= ~(1 << event->xbutton.button);

	/* Ignore any mouse motion that happened before this event;
	   any subsequent mouse-movement Emacs events should reflect
	   only motion after the ButtonPress/Release.  */
	if (f != 0)
	  f->mouse_moved = false;

        f = x_menubar_window_to_frame (dpyinfo, event);
        /* For a down-event in the menu bar,
           don't pass it to Xt right now.
           Instead, save it away
           and we will pass it to Xt from kbd_buffer_get_event.
           That way, we can run some Lisp code first.  */
        if (! popup_activated ()
            /* Gtk+ menus only react to the first three buttons. */
            && event->xbutton.button < 3
            && f && event->type == ButtonPress
            /* Verify the event is really within the menu bar
               and not just sent to it due to grabbing.  */
            && event->xbutton.x >= 0
            && event->xbutton.x < FRAME_PIXEL_WIDTH (f)
            && event->xbutton.y >= 0
            && event->xbutton.y < FRAME_MENUBAR_HEIGHT (f)
            && event->xbutton.same_screen)
          {
	    if (!f->output_data.x->saved_menu_event)
	      f->output_data.x->saved_menu_event = xmalloc (sizeof *event);
	    *f->output_data.x->saved_menu_event = *event;
	    inev.ie.kind = MENU_BAR_ACTIVATE_EVENT;
	    XSETFRAME (inev.ie.frame_or_window, f);
	    *finish = X_EVENT_DROP;
          }
        else
          goto OTHER;
      }
      break;

    case CirculateNotify:
      goto OTHER;

    case CirculateRequest:
      goto OTHER;

    case VisibilityNotify:
      goto OTHER;

    case MappingNotify:
      /* Someone has changed the keyboard mapping - update the
         local cache.  */
      switch (event->xmapping.request)
        {
        case MappingModifier:
          x_find_modifier_meanings (dpyinfo);
	  FALLTHROUGH;
        case MappingKeyboard:
          XRefreshKeyboardMapping ((XMappingEvent *) &event->xmapping);
        }
      goto OTHER;

    case DestroyNotify:
      xft_settings_event (dpyinfo, event);
      break;

    default:
    OTHER:
    break;
    }

 done:
  if (inev.ie.kind != NO_EVENT)
    {
      kbd_buffer_store_buffered_event (&inev, hold_quit);
      count++;
    }

  if (do_help
      && !(hold_quit && hold_quit->kind != NO_EVENT))
    {
      Lisp_Object frame;

      if (f)
	XSETFRAME (frame, f);
      else
	frame = Qnil;

      if (do_help > 0)
	{
	  any_help_event_p = true;
	  gen_help_event (help_echo_string, frame, help_echo_window,
			  help_echo_object, help_echo_pos);
	}
      else
	{
	  help_echo_string = Qnil;
	  gen_help_event (Qnil, frame, Qnil, Qnil, 0);
	}
      count++;
    }

  /* Sometimes event processing draws to the frame outside redisplay.
     To ensure that these changes become visible, draw them here.  */
  flush_dirty_back_buffers ();
  SAFE_FREE ();
  return count;
}
#endif

#if 0

/* Handles the XEvent EVENT on display DISPLAY.
   This is used for event loops outside the normal event handling,
   i.e. looping while a popup menu or a dialog is posted.

   Returns the value handle_one_xevent sets in the finish argument.  */
int
x_dispatch_event (XEvent *event, Display *display)
{
  struct x_display_info *dpyinfo;
  int finish = X_EVENT_NORMAL;

  dpyinfo = x_display_info_for_display (display);

  if (dpyinfo)
    handle_one_xevent (dpyinfo, event, &finish, 0);

  return finish;
}
#endif

#if 0
/* This is the filter function invoked by the GTK event loop.
   It is invoked before the XEvent is translated to a GdkEvent,
   so we have a chance to act on the event before GTK.  */
static GdkFilterReturn
event_handler_gdk (GdkXEvent *gxev, GdkEvent *ev, gpointer data)
{
  XEvent *xev = (XEvent *) gxev;

  block_input ();
  if (current_count >= 0)
    {
      struct x_display_info *dpyinfo;

      dpyinfo = x_display_info_for_display (xev->xany.display);

      if (! dpyinfo)
        current_finish = X_EVENT_NORMAL;
      else
	current_count
	  += handle_one_xevent (dpyinfo, xev, &current_finish,
				current_hold_quit);
    }
  else
    current_finish = x_dispatch_event (xev, xev->xany.display);

  unblock_input ();

  if (current_finish == X_EVENT_GOTO_OUT || current_finish == X_EVENT_DROP)
    return GDK_FILTER_REMOVE;

  return GDK_FILTER_CONTINUE;
}
#endif

/*
  event の処理、どうしようかなぁ。

  g_signal_connect() で処理するしかないよね…
  どの GdkWindow / GtkWidget で起きたか → frame に変換。

  ClientMessage:
    delete は処理しようか。

  SelectionNotify:
  SelectionClear:
  SelectionRequest:
    何だっけ? あぁ、コピペか。

  PropertyNotify:
    iconify/deiconify された時。

  ReparentNotify:
    そんなことあるの?

  Expose:
    やっぱりここで描画するんだな。

  GraphicsExpose:
    要らんだろ。

  NoExpose:
    要らんだろ。

  UnmapNotify:
    まぁ要るんだろうなぁ。

  MapNotify:
    よく解らん。

  KeyPress:
    これは大変そう… でも必要。

  KeyRelease:
    必要。

  EnterNotify:
    必要。

  FocusIn:
    必要。

  LeaveNotify:
    必要。

  FocusOut:
    必要。

  MotionNotify:
    ちょっと大変そう。
    xg_event_is_for_scrollbar とかが必要になる。

  ConfigureNotify:
    resize 時の処理は必要だなぁ。

  ButtonRelease:
  ButtonPress:
    後回しか。

  CirculateNotify:
  CirculateRequest:
  VisibilityNotify:
    後回し。

  MappingNotify:
    これも後回しだな。

  DestroyNotify:
    何故 xft?

 */

static gboolean
pgtk_handle_event(GtkWidget *widget, GdkEvent *event, gpointer *data)
{
  struct frame *f;
  union buffered_input_event inev;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  switch (event->type) {
  case GDK_NOTHING:               PGTK_TRACE("GDK_NOTHING"); break;
  case GDK_DELETE:                PGTK_TRACE("GDK_DELETE"); break;
  case GDK_DESTROY:               PGTK_TRACE("GDK_DESTROY"); break;
  case GDK_EXPOSE:
    PGTK_TRACE("GDK_EXPOSE");
    f = pgtk_any_window_to_frame (event->expose.window);
#if 0
    if (f)
      {
	if (!FRAME_VISIBLE_P (f))
	  {
	    block_input ();
	    SET_FRAME_VISIBLE (f, 1);
	    SET_FRAME_ICONIFIED (f, false);
	    if (FRAME_X_DOUBLE_BUFFERED_P (f))
	      font_drop_xrender_surfaces (f);
	    f->output_data.x->has_been_visible = true;
	    SET_FRAME_GARBAGED (f);
	    unblock_input ();
	  }
	else if (FRAME_GARBAGED_P (f))
	  {
	    /* Go around the back buffer and manually clear the
	       window the first time we show it.  This way, we avoid
	       showing users the sanity-defying horror of whatever
	       GtkWindow is rendering beneath us.  We've garbaged
	       the frame, so we'll redraw the whole thing on next
	       redisplay anyway.  Yuck.  */
	    x_clear_area1 (
	      FRAME_X_DISPLAY (f),
	      FRAME_X_WINDOW (f),
	      event->xexpose.x, event->xexpose.y,
	      event->xexpose.width, event->xexpose.height,
	      0);
	    x_clear_under_internal_border (f);
	  }


	if (!FRAME_GARBAGED_P (f))
	  {
	    /* This seems to be needed for GTK 2.6 and later, see
	       https://debbugs.gnu.org/cgi/bugreport.cgi?bug=15398.  */
	    x_clear_area (f,
			  event->xexpose.x, event->xexpose.y,
			  event->xexpose.width, event->xexpose.height);
	    expose_frame (f, event->xexpose.x, event->xexpose.y,
			  event->xexpose.width, event->xexpose.height);
	    x_clear_under_internal_border (f);
	  }

	if (!FRAME_GARBAGED_P (f))
	  show_back_buffer (f);
      }
    else
      {
	struct scroll_bar *bar;

	bar = x_window_to_scroll_bar (event->xexpose.display,
				      event->xexpose.window, 2);

	if (bar)
	  x_scroll_bar_expose (bar, event);
      }
#endif
      break;

  case GDK_MOTION_NOTIFY:         PGTK_TRACE("GDK_MOTION_NOTIFY"); break;
  case GDK_BUTTON_PRESS:          PGTK_TRACE("GDK_BUTTON_PRESS"); break;
  case GDK_2BUTTON_PRESS:         PGTK_TRACE("GDK_2BUTTON_PRESS"); break;
  case GDK_3BUTTON_PRESS:         PGTK_TRACE("GDK_3BUTTON_PRESS"); break;
  case GDK_BUTTON_RELEASE:        PGTK_TRACE("GDK_BUTTON_RELEASE"); break;
  case GDK_KEY_PRESS:             PGTK_TRACE("GDK_KEY_PRESS"); break;
  case GDK_KEY_RELEASE:           PGTK_TRACE("GDK_KEY_RELEASE"); break;
  case GDK_ENTER_NOTIFY:          PGTK_TRACE("GDK_ENTER_NOTIFY"); break;
  case GDK_LEAVE_NOTIFY:          PGTK_TRACE("GDK_LEAVE_NOTIFY"); break;
  case GDK_FOCUS_CHANGE:          PGTK_TRACE("GDK_FOCUS_CHANGE"); break;
  case GDK_CONFIGURE:
    PGTK_TRACE("GDK_CONFIGURE");
    f = pgtk_any_window_to_frame (event->configure.window);
    if (f) {
      pgtk_cr_destroy_surface (f);

      PGTK_TRACE("%dx%d", event->configure.width, event->configure.height);
      xg_frame_resized(f, event->configure.width, event->configure.height);
    }
    break;

  case GDK_MAP:
    PGTK_TRACE("GDK_MAP");
      f = pgtk_any_window_to_frame (event->any.window);
      if (f)
        {
	  bool iconified = FRAME_ICONIFIED_P (f);

#if 0
          /* Check if fullscreen was specified before we where mapped the
             first time, i.e. from the command line.  */
          if (!f->output_data.pgtk->has_been_visible)
	    {
	      x_check_fullscreen (f);
	    }
#endif

	  if (!iconified)
	    {
	      /* The `z-group' is reset every time a frame becomes
		 invisible.  Handle this here.  */
	      if (FRAME_Z_GROUP (f) == z_group_above)
		x_set_z_group (f, Qabove, Qnil);
	      else if (FRAME_Z_GROUP (f) == z_group_below)
		x_set_z_group (f, Qbelow, Qnil);
	    }

          SET_FRAME_VISIBLE (f, 1);
          SET_FRAME_ICONIFIED (f, false);
          f->output_data.pgtk->has_been_visible = true;

          if (iconified)
            {
              inev.ie.kind = DEICONIFY_EVENT;
              XSETFRAME (inev.ie.frame_or_window, f);
            }
          else if (! NILP (Vframe_list) && ! NILP (XCDR (Vframe_list)))
            /* Force a redisplay sooner or later to update the
	       frame titles in case this is the second frame.  */
            record_asynch_buffer_change ();
        }
    break;
  case GDK_UNMAP:                 PGTK_TRACE("GDK_UNMAP"); break;
  case GDK_PROPERTY_NOTIFY:       PGTK_TRACE("GDK_PROPERTY_NOTIFY"); break;
  case GDK_SELECTION_CLEAR:       PGTK_TRACE("GDK_SELECTION_CLEAR"); break;
  case GDK_SELECTION_REQUEST:     PGTK_TRACE("GDK_SELECTION_REQUEST"); break;
  case GDK_SELECTION_NOTIFY:      PGTK_TRACE("GDK_SELECTION_NOTIFY"); break;
  case GDK_PROXIMITY_IN:          PGTK_TRACE("GDK_PROXIMITY_IN"); break;
  case GDK_PROXIMITY_OUT:         PGTK_TRACE("GDK_PROXIMITY_OUT"); break;
  case GDK_DRAG_ENTER:            PGTK_TRACE("GDK_DRAG_ENTER"); break;
  case GDK_DRAG_LEAVE:            PGTK_TRACE("GDK_DRAG_LEAVE"); break;
  case GDK_DRAG_MOTION:           PGTK_TRACE("GDK_DRAG_MOTION"); break;
  case GDK_DRAG_STATUS:           PGTK_TRACE("GDK_DRAG_STATUS"); break;
  case GDK_DROP_START:            PGTK_TRACE("GDK_DROP_START"); break;
  case GDK_DROP_FINISHED:         PGTK_TRACE("GDK_DROP_FINISHED"); break;
  case GDK_CLIENT_EVENT:          PGTK_TRACE("GDK_CLIENT_EVENT"); break;
  case GDK_VISIBILITY_NOTIFY:     PGTK_TRACE("GDK_VISIBILITY_NOTIFY"); break;
  case GDK_SCROLL:                PGTK_TRACE("GDK_SCROLL"); break;
  case GDK_WINDOW_STATE:
    PGTK_TRACE("GDK_WINDOW_STATE");
    f = pgtk_any_window_to_frame (event->window_state.window);
    if (f && (event->window_state.changed_mask & GDK_WINDOW_STATE_ICONIFIED)) {
      if (FRAME_ICONIFIED_P (f))
	{
	  /* Gnome shell does not iconify us when C-z is pressed.
	     It hides the frame.  So if our state says we aren't
	     hidden anymore, treat it as deiconified.  */
	  SET_FRAME_VISIBLE (f, 1);
	  SET_FRAME_ICONIFIED (f, false);
	  f->output_data.pgtk->has_been_visible = true;
	  inev.ie.kind = DEICONIFY_EVENT;
	  XSETFRAME (inev.ie.frame_or_window, f);
	}
      else
	{
	  SET_FRAME_VISIBLE (f, 0);
	  SET_FRAME_ICONIFIED (f, true);
	  inev.ie.kind = ICONIFY_EVENT;
	  XSETFRAME (inev.ie.frame_or_window, f);
	}
    }
    break;
  case GDK_SETTING:               PGTK_TRACE("GDK_SETTING"); break;
  case GDK_OWNER_CHANGE:          PGTK_TRACE("GDK_OWNER_CHANGE"); break;
  case GDK_GRAB_BROKEN:           PGTK_TRACE("GDK_GRAB_BROKEN"); break;
  case GDK_DAMAGE:                PGTK_TRACE("GDK_DAMAGE"); break;
  case GDK_TOUCH_BEGIN:           PGTK_TRACE("GDK_TOUCH_BEGIN"); break;
  case GDK_TOUCH_UPDATE:          PGTK_TRACE("GDK_TOUCH_UPDATE"); break;
  case GDK_TOUCH_END:             PGTK_TRACE("GDK_TOUCH_END"); break;
  case GDK_TOUCH_CANCEL:          PGTK_TRACE("GDK_TOUCH_CANCEL"); break;
  case GDK_TOUCHPAD_SWIPE:        PGTK_TRACE("GDK_TOUCHPAD_SWIPE"); break;
  case GDK_TOUCHPAD_PINCH:        PGTK_TRACE("GDK_TOUCHPAD_PINCH"); break;
  case GDK_PAD_BUTTON_PRESS:      PGTK_TRACE("GDK_PAD_BUTTON_PRESS"); break;
  case GDK_PAD_BUTTON_RELEASE:    PGTK_TRACE("GDK_PAD_BUTTON_RELEASE"); break;
  case GDK_PAD_RING:              PGTK_TRACE("GDK_PAD_RING"); break;
  case GDK_PAD_STRIP:             PGTK_TRACE("GDK_PAD_STRIP"); break;
  case GDK_PAD_GROUP_MODE:        PGTK_TRACE("GDK_PAD_GROUP_MODE"); break;
  default:                        PGTK_TRACE("%d", event->type);
  }
  return FALSE;
}

static void
pgtk_fill_rectangle(struct frame *f, unsigned long color, int x, int y, int width, int height)
{
  PGTK_TRACE("pgtk_fill_rectangle");
  cairo_t *cr;
  cr = pgtk_begin_cr_clip (f, NULL);
  pgtk_set_cr_source_with_color (f, color);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);
  pgtk_end_cr_clip (f);
}

void
pgtk_clear_under_internal_border (struct frame *f)
{
  PGTK_TRACE("pgtk_clear_under_internal_border");
  if (FRAME_INTERNAL_BORDER_WIDTH (f) > 0)
    {
      int border = FRAME_INTERNAL_BORDER_WIDTH (f);
      int width = FRAME_PIXEL_WIDTH (f);
      int height = FRAME_PIXEL_HEIGHT (f);
      int margin = 0;
      struct face *face = FACE_FROM_ID_OR_NULL (f, INTERNAL_BORDER_FACE_ID);

      block_input ();

      if (face)
	{
	  unsigned long color = face->background;

	  pgtk_fill_rectangle (f, color, 0, margin, width, border);
	  pgtk_fill_rectangle (f, color, 0, 0, border, height);
	  pgtk_fill_rectangle (f, color, width - border, 0, border, height);
	  pgtk_fill_rectangle (f, color, 0, height - border, width, border);
	}
      else
	{
	  pgtk_clear_area (f, 0, 0, border, height);
	  pgtk_clear_area (f, 0, margin, width, border);
	  pgtk_clear_area (f, width - border, 0, border, height);
	  pgtk_clear_area (f, 0, height - border, width, border);
	}

      unblock_input ();
    }
}

static gboolean
pgtk_handle_draw(GtkWidget *widget, cairo_t *cr, gpointer *data)
{
  struct frame *f;

  PGTK_TRACE("pgtk_handle_draw");

  for (GtkWidget *w = widget; w != NULL; w = gtk_widget_get_parent(w)) {
    PGTK_TRACE("%p %s %d %d", w, G_OBJECT_TYPE_NAME(w), gtk_widget_get_mapped(w), gtk_widget_get_visible(w));
    gint wd, hi;
    gtk_widget_get_size_request(w, &wd, &hi);
    PGTK_TRACE(" %dx%d", wd, hi);
    GtkAllocation alloc;
    gtk_widget_get_allocation(w, &alloc);
    PGTK_TRACE(" %dx%d+%d+%d", alloc.width, alloc.height, alloc.x, alloc.y);
  }

#if 1
  {
#if 0
    PGTK_TRACE("widget: %s (window %s) (%p)", G_OBJECT_TYPE_NAME(widget), gtk_widget_get_has_window(widget) ? "yes" : "no", gtk_widget_get_window(widget));
    {
      GtkWidget *w = widget;
      while (w != NULL) {
	w = gtk_widget_get_parent(w);
	if (w != NULL)
	  PGTK_TRACE("widget: %s (window %s) (%p)", G_OBJECT_TYPE_NAME(w), gtk_widget_get_has_window(w) ? "yes" : "no", gtk_widget_get_window(w));
      }
    }
#endif

    GdkWindow *win = gtk_widget_get_window(widget);
#if 0
    {
      GdkWindow *w = win;
      PGTK_TRACE("window: %s %p %s", G_OBJECT_TYPE_NAME(w), w, gdk_window_has_native(w) ? "native" :"");
      while (w != NULL) {
	w = gdk_window_get_parent(w);
	if (w != NULL)
	  PGTK_TRACE("window: %s %p %s", G_OBJECT_TYPE_NAME(w), w, gdk_window_has_native(w) ? "native" :"");
      }
    }
#endif

    PGTK_TRACE("  win=%p", win);
    if (win != NULL) {
      f = pgtk_any_window_to_frame(win);
      PGTK_TRACE("  f=%p", f);
      PGTK_TRACE("  surface=%p", f ? FRAME_CR_SURFACE(f) : NULL);
      if (f != NULL && FRAME_CR_SURFACE(f) != NULL) {
	PGTK_TRACE("  resized_p=%d", f->resized_p);
	PGTK_TRACE("  garbaged=%d", f->garbaged);
	cairo_set_source_surface(cr, FRAME_CR_SURFACE(f), 0, 0);
	cairo_paint(cr);
      }
    }
    return TRUE;
  }
#endif


  GdkWindow *win = gtk_widget_get_window(widget);
  if (win == NULL) {
    PGTK_TRACE("win == NULL");
    return TRUE;
  }
  f = pgtk_any_window_to_frame(win);
  PGTK_TRACE(" f=%p", f);

  if (f)
    {
      PGTK_TRACE("f != NULL");
      PGTK_TRACE("  resized_p=%d\n", f->resized_p);
      PGTK_TRACE("  garbaged=%d\n", f->garbaged);
      if (!FRAME_VISIBLE_P (f))
	{
	  PGTK_TRACE("not visible");
	  block_input ();
	  SET_FRAME_VISIBLE (f, 1);
	  SET_FRAME_ICONIFIED (f, false);
#if 0
	  if (FRAME_X_DOUBLE_BUFFERED_P (f))
	    font_drop_xrender_surfaces (f);
#endif
	  f->output_data.pgtk->has_been_visible = true;
	  SET_FRAME_GARBAGED (f);
	  unblock_input ();
	}
      else if (FRAME_GARBAGED_P (f))
	{
	  PGTK_TRACE("garbaged.");
	  /* Go around the back buffer and manually clear the
	     window the first time we show it.  This way, we avoid
	     showing users the sanity-defying horror of whatever
	     GtkWindow is rendering beneath us.  We've garbaged
	     the frame, so we'll redraw the whole thing on next
	     redisplay anyway.  Yuck.  */
	  pgtk_clear_area(f, 0, 0, FRAME_PIXEL_WIDTH(f), FRAME_PIXEL_HEIGHT(f));
	  pgtk_clear_under_internal_border (f);
	}


      if (!FRAME_GARBAGED_P (f))
	{
	  PGTK_TRACE("not garbaged.");
	  /* This seems to be needed for GTK 2.6 and later, see
	     https://debbugs.gnu.org/cgi/bugreport.cgi?bug=15398.  */
	  PGTK_TRACE("%dx%d.", FRAME_PIXEL_WIDTH(f), FRAME_PIXEL_HEIGHT(f));
	  pgtk_clear_area(f, 0, 0, FRAME_PIXEL_WIDTH(f), FRAME_PIXEL_HEIGHT(f));
	  expose_frame (f, 0, 0, FRAME_PIXEL_WIDTH(f), FRAME_PIXEL_HEIGHT(f));
	  pgtk_clear_under_internal_border (f);
	}

#if 0
      if (!FRAME_GARBAGED_P (f))
	show_back_buffer (f);
#endif
    }
  else
    {
#if 0
      struct scroll_bar *bar;

      bar = x_window_to_scroll_bar (event->xexpose.display,
				    event->xexpose.window, 2);

      if (bar)
	x_scroll_bar_expose (bar, event);
#endif
    }

  return FALSE;
}

static void size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer *user_data)
{
  PGTK_TRACE("size-alloc: %dx%d+%d+%d.", alloc->width, alloc->height, alloc->x, alloc->y);

  struct frame *f = pgtk_any_window_to_frame (gtk_widget_get_window(widget));
  if (f) {
    pgtk_cr_destroy_surface (f);

    PGTK_TRACE("%dx%d", alloc->width, alloc->height);
    xg_frame_resized(f, alloc->width, alloc->height);
  }
}

int
pgtk_gtk_to_emacs_modifiers (int state)
{
  int mod_ctrl = ctrl_modifier;
  int mod_meta = meta_modifier;
  int mod_alt  = alt_modifier;
  int mod_hyper = hyper_modifier;
  int mod_super = super_modifier;
  Lisp_Object tem;

  tem = Fget (Vx_ctrl_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_ctrl = XINT (tem) & INT_MAX;
  tem = Fget (Vx_alt_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_alt = XINT (tem) & INT_MAX;
  tem = Fget (Vx_meta_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_meta = XINT (tem) & INT_MAX;
  tem = Fget (Vx_hyper_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_hyper = XINT (tem) & INT_MAX;
  tem = Fget (Vx_super_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_super = XINT (tem) & INT_MAX;

  return (  ((state & GDK_SHIFT_MASK)     ? shift_modifier : 0)
            | ((state & GDK_CONTROL_MASK) ? mod_ctrl	: 0)
            | ((state & GDK_META_MASK)	  ? mod_meta	: 0)
            | ((state & GDK_MOD1_MASK)	  ? mod_alt	: 0)
            | ((state & GDK_SUPER_MASK)	  ? mod_super	: 0)
            | ((state & GDK_HYPER_MASK)	  ? mod_hyper	: 0));
}

static int
pgtk_emacs_to_gtk_modifiers (EMACS_INT state)
{
  EMACS_INT mod_ctrl = ctrl_modifier;
  EMACS_INT mod_meta = meta_modifier;
  EMACS_INT mod_alt  = alt_modifier;
  EMACS_INT mod_hyper = hyper_modifier;
  EMACS_INT mod_super = super_modifier;

  Lisp_Object tem;

  tem = Fget (Vx_ctrl_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_ctrl = XINT (tem);
  tem = Fget (Vx_alt_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_alt = XINT (tem);
  tem = Fget (Vx_meta_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_meta = XINT (tem);
  tem = Fget (Vx_hyper_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_hyper = XINT (tem);
  tem = Fget (Vx_super_keysym, Qmodifier_value);
  if (INTEGERP (tem)) mod_super = XINT (tem);


  return (  ((state & mod_alt)		? GDK_MOD1_MASK    : 0)
            | ((state & mod_super)	? GDK_SUPER_MASK   : 0)
            | ((state & mod_hyper)	? GDK_HYPER_MASK   : 0)
            | ((state & shift_modifier)	? GDK_SHIFT_MASK   : 0)
            | ((state & mod_ctrl)	? GDK_CONTROL_MASK : 0)
            | ((state & mod_meta)	? GDK_META_MASK    : 0));
}

#define IsCursorKey(keysym)       (0xff50 <= (keysym) && (keysym) < 0xff60)
#define IsMiscFunctionKey(keysym) (0xff60 <= (keysym) && (keysym) < 0xff6c)
#define IsKeypadKey(keysym)       (0xff80 <= (keysym) && (keysym) < 0xffbe)
#define IsFunctionKey(keysym)     (0xffbe <= (keysym) && (keysym) < 0xffe1)

static gboolean key_press_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  struct coding_system coding;
  union buffered_input_event inev;
  ptrdiff_t nbytes = 0;
  Mouse_HLInfo *hlinfo;

  USE_SAFE_ALLOCA;

  PGTK_TRACE("key_press_event");

#if 0
  /* Dispatch KeyPress events when in menu.  */
  if (popup_activated ())
    goto done;
#endif

  struct frame *f = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  hlinfo = MOUSE_HL_INFO(f);

  /* If mouse-highlight is an integer, input clears out
     mouse highlighting.  */
  if (!hlinfo->mouse_face_hidden && INTEGERP (Vmouse_highlight))
    {
      clear_mouse_face (hlinfo);
      hlinfo->mouse_face_hidden = true;
    }

  if (f != 0)
    {
      guint keysym, orig_keysym;
      /* al%imercury@uunet.uu.net says that making this 81
	 instead of 80 fixed a bug whereby meta chars made
	 his Emacs hang.

	 It seems that some version of XmbLookupString has
	 a bug of not returning XBufferOverflow in
	 status_return even if the input is too long to
	 fit in 81 bytes.  So, we must prepare sufficient
	 bytes for copy_buffer.  513 bytes (256 chars for
	 two-byte character set) seems to be a fairly good
	 approximation.  -- 2000.8.10 handa@etl.go.jp  */
      unsigned char copy_buffer[513];
      unsigned char *copy_bufptr = copy_buffer;
      int copy_bufsiz = sizeof (copy_buffer);
      int modifiers;
      Lisp_Object coding_system = Qlatin_1;
      Lisp_Object c;
      guint state = event->key.state;

      /* Don't pass keys to GTK.  A Tab will shift focus to the
	 tool bar in GTK 2.4.  Keys will still go to menus and
	 dialogs because in that case popup_activated is nonzero
	 (see above).  */
      // *finish = X_EVENT_DROP;

      state |= pgtk_emacs_to_gtk_modifiers (extra_keyboard_modifiers);
      modifiers = state;

      /* This will have to go some day...  */

      /* make_lispy_event turns chars into control chars.
	 Don't do it here because XLookupString is too eager.  */
      state &= ~GDK_CONTROL_MASK;
      state &= ~(GDK_META_MASK
		 | GDK_SUPER_MASK
		 | GDK_HYPER_MASK
		 | GDK_MOD1_MASK);

      nbytes = event->key.length;
      if (nbytes > copy_bufsiz)
	nbytes = copy_bufsiz;
      memcpy(copy_bufptr, event->key.string, nbytes);

      keysym = event->key.keyval;
      orig_keysym = keysym;

      /* Common for all keysym input events.  */
      XSETFRAME (inev.ie.frame_or_window, f);
      inev.ie.modifiers = pgtk_gtk_to_emacs_modifiers (modifiers);
      inev.ie.timestamp = event->key.time;

      /* First deal with keysyms which have defined
	 translations to characters.  */
      if (keysym >= 32 && keysym < 128)
	/* Avoid explicitly decoding each ASCII character.  */
	{
	  inev.ie.kind = ASCII_KEYSTROKE_EVENT;
	  inev.ie.code = keysym;
	  goto done;
	}

      /* Keysyms directly mapped to Unicode characters.  */
      if (keysym >= 0x01000000 && keysym <= 0x0110FFFF)
	{
	  if (keysym < 0x01000080)
	    inev.ie.kind = ASCII_KEYSTROKE_EVENT;
	  else
	    inev.ie.kind = MULTIBYTE_CHAR_KEYSTROKE_EVENT;
	  inev.ie.code = keysym & 0xFFFFFF;
	  goto done;
	}

#if 0
      /* Now non-ASCII.  */
      if (HASH_TABLE_P (Vx_keysym_table)
	  && (c = Fgethash (make_number (keysym),
			    Vx_keysym_table,
			    Qnil),
	      NATNUMP (c)))
	{
	  inev.ie.kind = (SINGLE_BYTE_CHAR_P (XFASTINT (c))
			  ? ASCII_KEYSTROKE_EVENT
			  : MULTIBYTE_CHAR_KEYSTROKE_EVENT);
	  inev.ie.code = XFASTINT (c);
	  goto done;
	}
#endif

      /* Random non-modifier sorts of keysyms.  */
      if (((keysym >= GDK_KEY_BackSpace && keysym <= GDK_KEY_Escape)
	   || keysym == GDK_KEY_Delete
#ifdef GDK_KEY_ISO_Left_Tab
	   || (keysym >= GDK_KEY_ISO_Left_Tab
	       && keysym <= GDK_KEY_ISO_Enter)
#endif
	   || IsCursorKey (keysym) /* 0xff50 <= x < 0xff60 */
	   || IsMiscFunctionKey (keysym) /* 0xff60 <= x < VARIES */
#ifdef HPUX
	   /* This recognizes the "extended function
	      keys".  It seems there's no cleaner way.
	      Test IsModifierKey to avoid handling
	      mode_switch incorrectly.  */
	   || (GDK_KEY_Select <= keysym && keysym < GDK_KEY_KP_Space)
#endif
#ifdef GDK_KEY_dead_circumflex
	   || orig_keysym == GDK_KEY_dead_circumflex
#endif
#ifdef GDK_KEY_dead_grave
	   || orig_keysym == GDK_KEY_dead_grave
#endif
#ifdef GDK_KEY_dead_tilde
	   || orig_keysym == GDK_KEY_dead_tilde
#endif
#ifdef GDK_KEY_dead_diaeresis
	   || orig_keysym == GDK_KEY_dead_diaeresis
#endif
#ifdef GDK_KEY_dead_macron
	   || orig_keysym == GDK_KEY_dead_macron
#endif
#ifdef GDK_KEY_dead_degree
	   || orig_keysym == GDK_KEY_dead_degree
#endif
#ifdef GDK_KEY_dead_acute
	   || orig_keysym == GDK_KEY_dead_acute
#endif
#ifdef GDK_KEY_dead_cedilla
	   || orig_keysym == GDK_KEY_dead_cedilla
#endif
#ifdef GDK_KEY_dead_breve
	   || orig_keysym == GDK_KEY_dead_breve
#endif
#ifdef GDK_KEY_dead_ogonek
	   || orig_keysym == GDK_KEY_dead_ogonek
#endif
#ifdef GDK_KEY_dead_caron
	   || orig_keysym == GDK_KEY_dead_caron
#endif
#ifdef GDK_KEY_dead_doubleacute
	   || orig_keysym == GDK_KEY_dead_doubleacute
#endif
#ifdef GDK_KEY_dead_abovedot
	   || orig_keysym == GDK_KEY_dead_abovedot
#endif
	   || IsKeypadKey (keysym) /* 0xff80 <= x < 0xffbe */
	   || IsFunctionKey (keysym) /* 0xffbe <= x < 0xffe1 */
	   /* Any "vendor-specific" key is ok.  */
	   || (orig_keysym & (1 << 28))
	   || (keysym != GDK_KEY_VoidSymbol && nbytes == 0))
	  && ! (event->key.is_modifier
		/* The symbols from GDK_KEY_ISO_Lock
		   to GDK_KEY_ISO_Last_Group_Lock
		   don't have real modifiers but
		   should be treated similarly to
		   Mode_switch by Emacs. */
#if defined GDK_KEY_ISO_Lock && defined GDK_KEY_ISO_Last_Group_Lock
		|| (GDK_KEY_ISO_Lock <= orig_keysym
		    && orig_keysym <= GDK_KEY_ISO_Last_Group_Lock)
#endif
		))
	{
	  STORE_KEYSYM_FOR_DEBUG (keysym);
	  /* make_lispy_event will convert this to a symbolic
	     key.  */
	  inev.ie.kind = NON_ASCII_KEYSTROKE_EVENT;
	  inev.ie.code = keysym;
	  goto done;
	}

      {	/* Raw bytes, not keysym.  */
	ptrdiff_t i;
	int nchars, len;

	for (i = 0, nchars = 0; i < nbytes; i++)
	  {
	    if (ASCII_CHAR_P (copy_bufptr[i]))
	      nchars++;
	    STORE_KEYSYM_FOR_DEBUG (copy_bufptr[i]);
	  }

	if (nchars < nbytes)
	  {
	    /* Decode the input data.  */

	    /* The input should be decoded with `coding_system'
	       which depends on which X*LookupString function
	       we used just above and the locale.  */
	    setup_coding_system (coding_system, &coding);
	    coding.src_multibyte = false;
	    coding.dst_multibyte = true;
	    /* The input is converted to events, thus we can't
	       handle composition.  Anyway, there's no XIM that
	       gives us composition information.  */
	    coding.common_flags &= ~CODING_ANNOTATION_MASK;

	    SAFE_NALLOCA (coding.destination, MAX_MULTIBYTE_LENGTH,
			  nbytes);
	    coding.dst_bytes = MAX_MULTIBYTE_LENGTH * nbytes;
	    coding.mode |= CODING_MODE_LAST_BLOCK;
	    decode_coding_c_string (&coding, copy_bufptr, nbytes, Qnil);
	    nbytes = coding.produced;
	    nchars = coding.produced_char;
	    copy_bufptr = coding.destination;
	  }

	/* Convert the input data to a sequence of
	   character events.  */
	for (i = 0; i < nbytes; i += len)
	  {
	    int ch;
	    if (nchars == nbytes)
	      ch = copy_bufptr[i], len = 1;
	    else
	      ch = STRING_CHAR_AND_LENGTH (copy_bufptr + i, len);
	    inev.ie.kind = (SINGLE_BYTE_CHAR_P (ch)
			    ? ASCII_KEYSTROKE_EVENT
			    : MULTIBYTE_CHAR_KEYSTROKE_EVENT);
	    inev.ie.code = ch;
#if 0
	    kbd_buffer_store_buffered_event (&inev, hold_quit);
#else
	    kbd_buffer_store_buffered_event (&inev, NULL);
#endif
	  }

	// count += nchars;

	inev.ie.kind = NO_EVENT;  /* Already stored above.  */

	if (keysym == GDK_KEY_VoidSymbol)
	  goto done;
      }
    }

 done:
  if (inev.ie.kind != NO_EVENT)
    {
      kbd_buffer_store_buffered_event (&inev, NULL);
      XSETFRAME (inev.ie.frame_or_window, f);
      // count++;
    }

  SAFE_FREE();

  return TRUE;
}



static void
frame_highlight (struct frame *f)
{
  /* We used to only do this if Vx_no_window_manager was non-nil, but
     the ICCCM (section 4.1.6) says that the window's border pixmap
     and border pixel are window attributes which are "private to the
     client", so we can always change it to whatever we want.  */
  block_input ();
  /* I recently started to get errors in this XSetWindowBorder, depending on
     the window-manager in use, tho something more is at play since I've been
     using that same window-manager binary for ever.  Let's not crash just
     because of this (bug#9310).  */
#if 0
  x_catch_errors (FRAME_X_DISPLAY (f));
  XSetWindowBorder (FRAME_X_DISPLAY (f), FRAME_X_WINDOW (f),
		    f->output_data.x->border_pixel);
  x_uncatch_errors ();
#endif
  unblock_input ();
  x_update_cursor (f, true);
#if 0
  x_set_frame_alpha (f);
#endif
}

static void
frame_unhighlight (struct frame *f)
{
  /* We used to only do this if Vx_no_window_manager was non-nil, but
     the ICCCM (section 4.1.6) says that the window's border pixmap
     and border pixel are window attributes which are "private to the
     client", so we can always change it to whatever we want.  */
  block_input ();
  /* Same as above for XSetWindowBorder (bug#9310).  */
#if 0
  x_catch_errors (FRAME_X_DISPLAY (f));
  XSetWindowBorderPixmap (FRAME_X_DISPLAY (f), FRAME_X_WINDOW (f),
			  f->output_data.x->border_tile);
  x_uncatch_errors ();
#endif
  unblock_input ();
  x_update_cursor (f, true);
#if 0
  x_set_frame_alpha (f);
#endif
}


static void
x_frame_rehighlight (struct pgtk_display_info *dpyinfo)
{
  struct frame *old_highlight = dpyinfo->x_highlight_frame;

  if (dpyinfo->x_focus_frame)
    {
      dpyinfo->x_highlight_frame
	= ((FRAMEP (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame)))
	   ? XFRAME (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame))
	   : dpyinfo->x_focus_frame);
      if (! FRAME_LIVE_P (dpyinfo->x_highlight_frame))
	{
	  fset_focus_frame (dpyinfo->x_focus_frame, Qnil);
	  dpyinfo->x_highlight_frame = dpyinfo->x_focus_frame;
	}
    }
  else
    dpyinfo->x_highlight_frame = 0;

  if (dpyinfo->x_highlight_frame != old_highlight)
    {
      if (old_highlight)
	frame_unhighlight (old_highlight);
      if (dpyinfo->x_highlight_frame)
	frame_highlight (dpyinfo->x_highlight_frame);
    }
}

/* The focus has changed.  Update the frames as necessary to reflect
   the new situation.  Note that we can't change the selected frame
   here, because the Lisp code we are interrupting might become confused.
   Each event gets marked with the frame in which it occurred, so the
   Lisp code can tell when the switch took place by examining the events.  */

static void
x_new_focus_frame (struct pgtk_display_info *dpyinfo, struct frame *frame)
{
  struct frame *old_focus = dpyinfo->x_focus_frame;

  if (frame != dpyinfo->x_focus_frame)
    {
      /* Set this before calling other routines, so that they see
	 the correct value of x_focus_frame.  */
      dpyinfo->x_focus_frame = frame;

#if 0
      if (old_focus && old_focus->auto_lower)
	x_lower_frame (old_focus);
#endif

#if 0
      if (dpyinfo->x_focus_frame && dpyinfo->x_focus_frame->auto_raise)
	dpyinfo->x_pending_autoraise_frame = dpyinfo->x_focus_frame;
      else
	dpyinfo->x_pending_autoraise_frame = NULL;
#endif
    }

  x_frame_rehighlight (dpyinfo);
}

/* The focus may have changed.  Figure out if it is a real focus change,
   by checking both FocusIn/Out and Enter/LeaveNotify events.

   Returns FOCUS_IN_EVENT event in *BUFP. */

/* Handle FocusIn and FocusOut state changes for FRAME.
   If FRAME has focus and there exists more than one frame, puts
   a FOCUS_IN_EVENT into *BUFP.  */

static void
x_focus_changed (gboolean is_enter, int state, struct pgtk_display_info *dpyinfo, struct frame *frame, union buffered_input_event *bufp)
{
  if (is_enter)
    {
      if (dpyinfo->x_focus_event_frame != frame)
        {
          x_new_focus_frame (dpyinfo, frame);
          dpyinfo->x_focus_event_frame = frame;

          /* Don't stop displaying the initial startup message
             for a switch-frame event we don't need.  */
          /* When run as a daemon, Vterminal_frame is always NIL.  */
          bufp->ie.arg = (((NILP (Vterminal_frame)
                         || ! FRAME_X_P (XFRAME (Vterminal_frame))
                         || EQ (Fdaemonp (), Qt))
			&& CONSP (Vframe_list)
			&& !NILP (XCDR (Vframe_list)))
		       ? Qt : Qnil);
          bufp->ie.kind = FOCUS_IN_EVENT;
          XSETFRAME (bufp->ie.frame_or_window, frame);
        }

      frame->output_data.pgtk->focus_state |= state;

    }
  else
    {
      frame->output_data.pgtk->focus_state &= ~state;

      if (dpyinfo->x_focus_event_frame == frame)
        {
          dpyinfo->x_focus_event_frame = 0;
          x_new_focus_frame (dpyinfo, 0);

          bufp->ie.kind = FOCUS_OUT_EVENT;
          XSETFRAME (bufp->ie.frame_or_window, frame);
        }

#if 0
      if (frame->pointer_invisible)
        XTtoggle_invisible_pointer (frame, false);
#endif
    }
}

static gboolean
enter_notify_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("enter_notify_event");
  union buffered_input_event inev;
  struct frame *focus_frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  int focus_state
    = focus_frame ? focus_frame->output_data.pgtk->focus_state : 0;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  if (!(focus_state & FOCUS_EXPLICIT))
    x_focus_changed (TRUE,
		     FOCUS_IMPLICIT,
		     FRAME_DISPLAY_INFO(focus_frame), focus_frame, &inev);
  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

static gboolean
leave_notify_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("leave_notify_event");
  union buffered_input_event inev;
  struct frame *focus_frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  int focus_state
    = focus_frame ? focus_frame->output_data.pgtk->focus_state : 0;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  if (!(focus_state & FOCUS_EXPLICIT))
    x_focus_changed (FALSE,
		     FOCUS_IMPLICIT,
		     FRAME_DISPLAY_INFO(focus_frame), focus_frame, &inev);
  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

static gboolean
focus_in_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("focus_in_event");
  union buffered_input_event inev;
  struct frame *frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));

  if (frame == NULL)
    return TRUE;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  x_focus_changed (TRUE, FOCUS_IMPLICIT,
		   FRAME_DISPLAY_INFO(frame), frame, &inev);
  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

static gboolean
focus_out_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("focus_out_event");
  union buffered_input_event inev;
  struct frame *frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));

  if (frame == NULL)
    return TRUE;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  x_focus_changed (FALSE, FOCUS_IMPLICIT,
		   FRAME_DISPLAY_INFO(frame), frame, &inev);
  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

/* Function to report a mouse movement to the mainstream Emacs code.
   The input handler calls this.

   We have received a mouse movement event, which is given in *event.
   If the mouse is over a different glyph than it was last time, tell
   the mainstream emacs code by setting mouse_moved.  If not, ask for
   another motion event, so we can check again the next time it moves.  */

static bool
note_mouse_movement (struct frame *frame, const GdkEventMotion *event)
{
  XRectangle *r;
  struct pgtk_display_info *dpyinfo;

  if (!FRAME_X_OUTPUT (frame))
    return false;

  dpyinfo = FRAME_DISPLAY_INFO (frame);
  dpyinfo->last_mouse_movement_time = event->time;
  dpyinfo->last_mouse_motion_frame = frame;
  dpyinfo->last_mouse_motion_x = event->x;
  dpyinfo->last_mouse_motion_y = event->y;

  if (event->window != gtk_widget_get_window(FRAME_GTK_WIDGET (frame)))
    {
      frame->mouse_moved = true;
      dpyinfo->last_mouse_scroll_bar = NULL;
      note_mouse_highlight (frame, -1, -1);
      dpyinfo->last_mouse_glyph_frame = NULL;
      return true;
    }


  /* Has the mouse moved off the glyph it was on at the last sighting?  */
  r = &dpyinfo->last_mouse_glyph;
  if (frame != dpyinfo->last_mouse_glyph_frame
      || event->x < r->x || event->x >= r->x + r->width
      || event->y < r->y || event->y >= r->y + r->height)
    {
      frame->mouse_moved = true;
      dpyinfo->last_mouse_scroll_bar = NULL;
      note_mouse_highlight (frame, event->x, event->y);
      /* Remember which glyph we're now on.  */
      remember_mouse_glyph (frame, event->x, event->y, r);
      dpyinfo->last_mouse_glyph_frame = frame;
      return true;
    }

  return false;
}

static gboolean
motion_notify_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("motion_notify_event");
  union buffered_input_event inev;
  struct frame *f, *frame;
  struct pgtk_display_info *dpyinfo;
  Mouse_HLInfo *hlinfo;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  previous_help_echo_string = help_echo_string;
  help_echo_string = Qnil;

  frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  dpyinfo = FRAME_DISPLAY_INFO (frame);
  f = (x_mouse_grabbed (dpyinfo) ? dpyinfo->last_mouse_frame
       : pgtk_any_window_to_frame(gtk_widget_get_window(widget)));
  hlinfo = MOUSE_HL_INFO (f);

  if (hlinfo->mouse_face_hidden)
    {
      hlinfo->mouse_face_hidden = false;
      clear_mouse_face (hlinfo);
    }

#if 0
  if (f && xg_event_is_for_scrollbar (f, event))
    f = 0;
#endif
  if (f)
    {
      /* Maybe generate a SELECT_WINDOW_EVENT for
	 `mouse-autoselect-window' but don't let popup menus
	 interfere with this (Bug#1261).  */
      if (!NILP (Vmouse_autoselect_window)
#if 0
	  && !popup_activated ()
#endif
	  /* Don't switch if we're currently in the minibuffer.
	     This tries to work around problems where the
	     minibuffer gets unselected unexpectedly, and where
	     you then have to move your mouse all the way down to
	     the minibuffer to select it.  */
	  && !MINI_WINDOW_P (XWINDOW (selected_window))
	  /* With `focus-follows-mouse' non-nil create an event
	     also when the target window is on another frame.  */
	  && (f == XFRAME (selected_frame)
	      || !NILP (focus_follows_mouse)))
	{
	  static Lisp_Object last_mouse_window;
	  Lisp_Object window = window_from_coordinates
	    (f, event->motion.x, event->motion.y, 0, false);

	  /* A window will be autoselected only when it is not
	     selected now and the last mouse movement event was
	     not in it.  The remainder of the code is a bit vague
	     wrt what a "window" is.  For immediate autoselection,
	     the window is usually the entire window but for GTK
	     where the scroll bars don't count.  For delayed
	     autoselection the window is usually the window's text
	     area including the margins.  */
	  if (WINDOWP (window)
	      && !EQ (window, last_mouse_window)
	      && !EQ (window, selected_window))
	    {
	      inev.ie.kind = SELECT_WINDOW_EVENT;
	      inev.ie.frame_or_window = window;
	    }

	  /* Remember the last window where we saw the mouse.  */
	  last_mouse_window = window;
	}

      if (!note_mouse_movement (f, &event->motion))
	help_echo_string = previous_help_echo_string;
    }
  else
    {
      /* If we move outside the frame, then we're
	 certainly no longer on any text in the frame.  */
      clear_mouse_face (hlinfo);
    }

#if 0
  /* If the contents of the global variable help_echo_string
     has changed, generate a HELP_EVENT.  */
  if (!NILP (help_echo_string)
      || !NILP (previous_help_echo_string))
    do_help = 1;
#endif

  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

/* Mouse clicks and mouse movement.  Rah.

   Formerly, we used PointerMotionHintMask (in standard_event_mask)
   so that we would have to call XQueryPointer after each MotionNotify
   event to ask for another such event.  However, this made mouse tracking
   slow, and there was a bug that made it eventually stop.

   Simply asking for MotionNotify all the time seems to work better.

   In order to avoid asking for motion events and then throwing most
   of them away or busy-polling the server for mouse positions, we ask
   the server for pointer motion hints.  This means that we get only
   one event per group of mouse movements.  "Groups" are delimited by
   other kinds of events (focus changes and button clicks, for
   example), or by XQueryPointer calls; when one of these happens, we
   get another MotionNotify event the next time the mouse moves.  This
   is at least as efficient as getting motion events when mouse
   tracking is on, and I suspect only negligibly worse when tracking
   is off.  */

/* Prepare a mouse-event in *RESULT for placement in the input queue.

   If the event is a button press, then note that we have grabbed
   the mouse.  */

static Lisp_Object
construct_mouse_click (struct input_event *result,
		       const GdkEventButton *event,
		       struct frame *f)
{
  /* Make the event type NO_EVENT; we'll change that when we decide
     otherwise.  */
  result->kind = MOUSE_CLICK_EVENT;
  result->code = event->button - 1;
  result->timestamp = event->time;
  result->modifiers = (pgtk_gtk_to_emacs_modifiers (event->state)
		       | (event->type == GDK_BUTTON_RELEASE
			  ? up_modifier
			  : down_modifier));

  XSETINT (result->x, event->x);
  XSETINT (result->y, event->y);
  XSETFRAME (result->frame_or_window, f);
  result->arg = Qnil;
  return Qnil;
}

static gboolean
button_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("button_event: type=%d, button=%u.", event->button.type, event->button.button);
  union buffered_input_event inev;
  struct frame *f, *frame;
  struct pgtk_display_info *dpyinfo;

  /* If we decide we want to generate an event to be seen
     by the rest of Emacs, we put it here.  */
  bool tool_bar_p = false;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  /* ignore double click and triple click. */
  if (event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE)
    return TRUE;

  frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  dpyinfo = FRAME_DISPLAY_INFO (frame);

#if 0
  memset (&compose_status, 0, sizeof (compose_status));
#endif
  dpyinfo->last_mouse_glyph_frame = NULL;
#if 0
  x_display_set_last_user_time (dpyinfo, event->button.time);
#endif

  if (x_mouse_grabbed (dpyinfo))
    f = dpyinfo->last_mouse_frame;
  else
    {
      f = pgtk_any_window_to_frame(gtk_widget_get_window(widget));

      if (f && event->button.type == GDK_BUTTON_PRESS
#if 0
	  && !popup_activated ()
#endif
#if 0
	  && !x_window_to_scroll_bar (event->button.display,
				      event->button.window, 2)
#endif
	  && !FRAME_NO_ACCEPT_FOCUS (f))
	{
	  /* When clicking into a child frame or when clicking
	     into a parent frame with the child frame selected and
	     `no-accept-focus' is not set, select the clicked
	     frame.  */
	  struct frame *hf = dpyinfo->x_highlight_frame;

	  if (FRAME_PARENT_FRAME (f) || (hf && frame_ancestor_p (f, hf)))
	    {
	      block_input ();
#if 0
	      XSetInputFocus (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f),
			      RevertToParent, CurrentTime);
	      if (FRAME_PARENT_FRAME (f))
		XRaiseWindow (FRAME_X_DISPLAY (f), FRAME_OUTER_WINDOW (f));
#endif
	      unblock_input ();
	    }
	}
    }

#if 0
  if (f && xg_event_is_for_scrollbar (f, event))
    f = 0;
#endif
  if (f)
    {
      if (!tool_bar_p)
#if 0
	if (! popup_activated ())
#endif
	  {
	    if (ignore_next_mouse_click_timeout)
	      {
		if (event->type == GDK_BUTTON_PRESS
		    && event->button.time > ignore_next_mouse_click_timeout)
		  {
		    ignore_next_mouse_click_timeout = 0;
		    construct_mouse_click (&inev.ie, &event->button, f);
		  }
		if (event->type == GDK_BUTTON_RELEASE)
		  ignore_next_mouse_click_timeout = 0;
	      }
	    else
	      construct_mouse_click (&inev.ie, &event->button, f);
	  }
#if 0
      if (FRAME_X_EMBEDDED_P (f))
	xembed_send_message (f, event->button.time,
			     XEMBED_REQUEST_FOCUS, 0, 0, 0);
#endif
    }
  else
    {
#if 0
      struct scroll_bar *bar
	= x_window_to_scroll_bar (event->button.display,
				  event->button.window, 2);

      if (bar)
	x_scroll_bar_handle_click (bar, event, &inev.ie);
#endif
    }

  if (event->type == GDK_BUTTON_PRESS)
    {
      dpyinfo->grabbed |= (1 << event->button.button);
      dpyinfo->last_mouse_frame = f;
    }
  else
    dpyinfo->grabbed &= ~(1 << event->button.button);

  /* Ignore any mouse motion that happened before this event;
     any subsequent mouse-movement Emacs events should reflect
     only motion after the ButtonPress/Release.  */
  if (f != 0)
    f->mouse_moved = false;

#if 0
  f = x_menubar_window_to_frame (dpyinfo, event);
  /* For a down-event in the menu bar,
     don't pass it to Xt right now.
     Instead, save it away
     and we will pass it to Xt from kbd_buffer_get_event.
     That way, we can run some Lisp code first.  */
  if (! popup_activated ()
      /* Gtk+ menus only react to the first three buttons. */
      && event->button.button < 3
      && f && event->type == GDK_BUTTON_PRESS
      /* Verify the event is really within the menu bar
	 and not just sent to it due to grabbing.  */
      && event->button.x >= 0
      && event->button.x < FRAME_PIXEL_WIDTH (f)
      && event->button.y >= 0
      && event->button.y < FRAME_MENUBAR_HEIGHT (f)
      && event->button.same_screen)
    {
      if (!f->output_data.pgtk->saved_menu_event)
	f->output_data.pgtk->saved_menu_event = xmalloc (sizeof *event);
      *f->output_data.pgtk->saved_menu_event = *event;
      inev.ie.kind = MENU_BAR_ACTIVATE_EVENT;
      XSETFRAME (inev.ie.frame_or_window, f);
      *finish = X_EVENT_DROP;
    }
#endif

  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

static gboolean
scroll_event(GtkWidget *widget, GdkEvent *event, gpointer *user_data)
{
  PGTK_TRACE("scroll_event");
  union buffered_input_event inev;
  struct frame *f, *frame;
  struct pgtk_display_info *dpyinfo;

  EVENT_INIT (inev.ie);
  inev.ie.kind = NO_EVENT;
  inev.ie.arg = Qnil;

  frame = pgtk_any_window_to_frame(gtk_widget_get_window(widget));
  dpyinfo = FRAME_DISPLAY_INFO (frame);

  if (x_mouse_grabbed (dpyinfo))
    f = dpyinfo->last_mouse_frame;
  else
    f = pgtk_any_window_to_frame(gtk_widget_get_window(widget));

  inev.ie.kind = WHEEL_EVENT;
  inev.ie.timestamp = event->scroll.time;
  inev.ie.modifiers = pgtk_gtk_to_emacs_modifiers (event->scroll.state);
  XSETINT (inev.ie.x, event->scroll.x);
  XSETINT (inev.ie.y, event->scroll.y);
  XSETFRAME (inev.ie.frame_or_window, f);
  inev.ie.arg = Qnil;

  switch (event->scroll.direction) {
  case GDK_SCROLL_UP:
    inev.ie.kind = WHEEL_EVENT;
    inev.ie.modifiers |= up_modifier;
    break;
  case GDK_SCROLL_DOWN:
    inev.ie.kind = WHEEL_EVENT;
    inev.ie.modifiers |= down_modifier;
    break;
  case GDK_SCROLL_LEFT:
    inev.ie.kind = HORIZ_WHEEL_EVENT;
    inev.ie.modifiers |= up_modifier;
    break;
  case GDK_SCROLL_RIGHT:
    inev.ie.kind = HORIZ_WHEEL_EVENT;
    inev.ie.modifiers |= down_modifier;
    break;
  case GDK_SCROLL_SMOOTH:
    if (event->scroll.delta_y >= 0.5) {
      inev.ie.kind = WHEEL_EVENT;
      inev.ie.modifiers |= down_modifier;
    } else if (event->scroll.delta_y <= -0.5) {
      inev.ie.kind = WHEEL_EVENT;
      inev.ie.modifiers |= up_modifier;
    } else if (event->scroll.delta_x >= 0.5) {
      inev.ie.kind = HORIZ_WHEEL_EVENT;
      inev.ie.modifiers |= down_modifier;
    } else if (event->scroll.delta_x <= -0.5) {
      inev.ie.kind = HORIZ_WHEEL_EVENT;
      inev.ie.modifiers |= up_modifier;
    } else
      return TRUE;
    break;
  default:
    return TRUE;
  }

  if (inev.ie.kind != NO_EVENT)
    kbd_buffer_store_buffered_event (&inev, NULL);
  return TRUE;
}

void
pgtk_set_event_handler(struct frame *f)
{
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "size-allocate", G_CALLBACK(size_allocate), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "key-press-event", G_CALLBACK(key_press_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "focus-in-event", G_CALLBACK(focus_in_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "focus-out-event", G_CALLBACK(focus_out_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "enter-notify-event", G_CALLBACK(enter_notify_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "leave-notify-event", G_CALLBACK(leave_notify_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "motion-notify-event", G_CALLBACK(motion_notify_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "button-press-event", G_CALLBACK(button_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "button-release-event", G_CALLBACK(button_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "scroll-event", G_CALLBACK(scroll_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "event", G_CALLBACK(pgtk_handle_event), NULL);
  g_signal_connect(G_OBJECT(FRAME_GTK_WIDGET(f)), "draw", G_CALLBACK(pgtk_handle_draw), NULL);
}

static void
my_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *msg, gpointer user_data)
{
  if (!strstr (msg, "g_set_prgname"))
      fprintf (stderr, "%s-WARNING **: %s", log_domain, msg);
}


/* Open a connection to X display DISPLAY_NAME, and return
   the structure that describes the open display.
   If we cannot contact the display, return null.  */

struct pgtk_display_info *
pgtk_term_init (Lisp_Object display_name, char *resource_name)
{
  Display *dpy;
  struct terminal *terminal;
  struct pgtk_display_info *dpyinfo;
  static int x_initialized = 0;

  block_input ();

  if (!x_initialized)
    {
      /* Try to not use interrupt input; start polling.  */
      Fset_input_interrupt_mode (Qnil);
      x_cr_init_fringe (&pgtk_redisplay_interface);
      ++x_initialized;
    }

#if 0
  if (! x_display_ok (SSDATA (display_name)))
    error ("Display %s can't be opened", SSDATA (display_name));
#endif

  {
#define NUM_ARGV 10
    int argc;
    char *argv[NUM_ARGV];
    char **argv2 = argv;
    guint id;

    if (x_initialized++ > 1)
      {
	PGTK_TRACE("...1");
        xg_display_open (SSDATA (display_name), &dpy);
      }
    else
      {
        static char display_opt[] = "--display";
        static char name_opt[] = "--name";

        for (argc = 0; argc < NUM_ARGV; ++argc)
          argv[argc] = 0;

        argc = 0;
        argv[argc++] = initial_argv[0];

        if (! NILP (display_name))
          {
            argv[argc++] = display_opt;
            argv[argc++] = SSDATA (display_name);
          }

        argv[argc++] = name_opt;
        argv[argc++] = resource_name;

#if 0
        XSetLocaleModifiers ("");
#endif

        /* Work around GLib bug that outputs a faulty warning. See
           https://bugzilla.gnome.org/show_bug.cgi?id=563627.  */
        id = g_log_set_handler ("GLib", G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL
                                  | G_LOG_FLAG_RECURSION, my_log_handler, NULL);

#if 0
        /* NULL window -> events for all windows go to our function.
           Call before gtk_init so Gtk+ event filters comes after our.  */
        gdk_window_add_filter (NULL, event_handler_gdk, NULL);
#endif

        /* gtk_init does set_locale.  Fix locale before and after.  */
        fixup_locale ();
        unrequest_sigio (); /* See comment in x_display_ok.  */
        gtk_init (&argc, &argv2);
        request_sigio ();
        fixup_locale ();

        g_log_remove_handler ("GLib", id);

        xg_initialize ();

        dpy = DEFAULT_GDK_DISPLAY ();

#if ! GTK_CHECK_VERSION (2, 90, 0)
        /* Load our own gtkrc if it exists.  */
        {
          const char *file = "~/.emacs.d/gtkrc";
          Lisp_Object s, abs_file;

          s = build_string (file);
          abs_file = Fexpand_file_name (s, Qnil);

          if (! NILP (abs_file) && !NILP (Ffile_readable_p (abs_file)))
            gtk_rc_parse (SSDATA (abs_file));
        }
#endif

#if 0
        XSetErrorHandler (x_error_handler);
        XSetIOErrorHandler (x_io_error_quitter);
#endif
      }
  }

  /* Detect failure.  */
  if (dpy == 0)
    {
      unblock_input ();
      return 0;
    }

  /* We have definitely succeeded.  Record the new connection.  */

  dpyinfo = xzalloc (sizeof *dpyinfo);
  pgtk_initialize_display_info (dpyinfo);
  terminal = pgtk_create_terminal (dpyinfo);

  {
#if 0
    struct pgtk_display_info *share;

    for (share = x_display_list; share; share = share->next)
      if (same_x_server (SSDATA (XCAR (share->name_list_element)),
			 SSDATA (display_name)))
	break;
    if (share)
      terminal->kboard = share->terminal->kboard;
    else
#endif
      {
	terminal->kboard = allocate_kboard (Qpgtk);

#if 0
	if (!EQ (XSYMBOL (Qvendor_specific_keysyms)->u.s.function, Qunbound))
	  {
	    char *vendor = ServerVendor (dpy);

	    /* Temporarily hide the partially initialized terminal.  */
	    terminal_list = terminal->next_terminal;
	    unblock_input ();
	    kset_system_key_alist
	      (terminal->kboard,
	       call1 (Qvendor_specific_keysyms,
		      vendor ? build_string (vendor) : empty_unibyte_string));
	    block_input ();
	    terminal->next_terminal = terminal_list;
	    terminal_list = terminal;
	  }
#endif

	/* Don't let the initial kboard remain current longer than necessary.
	   That would cause problems if a file loaded on startup tries to
	   prompt in the mini-buffer.  */
	if (current_kboard == initial_kboard)
	  current_kboard = terminal->kboard;
      }
    terminal->kboard->reference_count++;
  }

  /* Put this display on the chain.  */
  dpyinfo->next = x_display_list;
  x_display_list = dpyinfo;

  dpyinfo->name_list_element = Fcons (display_name, Qnil);
#if 0
  dpyinfo->display = dpy;
  dpyinfo->connection = ConnectionNumber (dpyinfo->display);
#endif

  /* https://lists.gnu.org/r/emacs-devel/2015-11/msg00194.html  */
  dpyinfo->smallest_font_height = 1;
  dpyinfo->smallest_char_width = 1;

  /* Set the name of the terminal. */
  terminal->name = xlispstrdup (display_name);

  Lisp_Object system_name = Fsystem_name ();
  ptrdiff_t nbytes;
  if (INT_ADD_WRAPV (SBYTES (Vinvocation_name), SBYTES (system_name) + 2,
		     &nbytes))
    memory_full (SIZE_MAX);
#if 0
  dpyinfo->x_id = ++x_display_id;
#endif
  dpyinfo->x_id_name = xmalloc (nbytes);
  char *nametail = lispstpcpy (dpyinfo->x_id_name, Vinvocation_name);
  *nametail++ = '@';
  lispstpcpy (nametail, system_name);

#if 0
  /* Figure out which modifier bits mean what.  */
  x_find_modifier_meanings (dpyinfo);
#endif

  /* Get the scroll bar cursor.  */
  /* We must create a GTK cursor, it is required for GTK widgets.  */
#if 0
  dpyinfo->xg_cursor = xg_create_default_cursor (dpyinfo->display);
#else
  dpyinfo->xg_cursor = xg_create_default_cursor (NULL);
#endif

#if 0
  dpyinfo->vertical_scroll_bar_cursor
    = XCreateFontCursor (dpyinfo->display, XC_sb_v_double_arrow);

  dpyinfo->horizontal_scroll_bar_cursor
    = XCreateFontCursor (dpyinfo->display, XC_sb_h_double_arrow);
#endif

#if 0
  xrdb = x_load_resources (dpyinfo->display, xrm_option,
			   resource_name, EMACS_CLASS);
  dpyinfo->display->db = xrdb;
  /* Put the rdb where we can find it in a way that works on
     all versions.  */
  dpyinfo->xrdb = xrdb;
#endif

#if 0
  dpyinfo->screen = ScreenOfDisplay (dpyinfo->display,
				     DefaultScreen (dpyinfo->display));
  select_visual (dpyinfo);
  dpyinfo->cmap = DefaultColormapOfScreen (dpyinfo->screen);
  dpyinfo->root_window = RootWindowOfScreen (dpyinfo->screen);
  dpyinfo->icon_bitmap_id = -1;
  dpyinfo->wm_type = X_WMTYPE_UNKNOWN;
#endif

  reset_mouse_highlight (&dpyinfo->mouse_highlight);

#if 0
  /* See if we can construct pixel values from RGB values.  */
  if (dpyinfo->visual->class == TrueColor)
    {
      get_bits_and_offset (dpyinfo->visual->red_mask,
                           &dpyinfo->red_bits, &dpyinfo->red_offset);
      get_bits_and_offset (dpyinfo->visual->blue_mask,
                           &dpyinfo->blue_bits, &dpyinfo->blue_offset);
      get_bits_and_offset (dpyinfo->visual->green_mask,
                           &dpyinfo->green_bits, &dpyinfo->green_offset);
    }
#endif

#if 0
  /* See if a private colormap is requested.  */
  if (dpyinfo->visual == DefaultVisualOfScreen (dpyinfo->screen))
    {
      if (dpyinfo->visual->class == PseudoColor)
	{
	  AUTO_STRING (privateColormap, "privateColormap");
	  AUTO_STRING (PrivateColormap, "PrivateColormap");
	  Lisp_Object value
	    = display_x_get_resource (dpyinfo, privateColormap,
				      PrivateColormap, Qnil, Qnil);
	  if (STRINGP (value)
	      && (!strcmp (SSDATA (value), "true")
		  || !strcmp (SSDATA (value), "on")))
	    dpyinfo->cmap = XCopyColormapAndFree (dpyinfo->display, dpyinfo->cmap);
	}
    }
  else
    dpyinfo->cmap = XCreateColormap (dpyinfo->display, dpyinfo->root_window,
                                     dpyinfo->visual, AllocNone);
#endif

  // fixme: get dpi from somewhere.
  dpyinfo->resx = 96;
  dpyinfo->resy = 96;

#if 0
  dpyinfo->x_dnd_atoms_size = 8;
  dpyinfo->x_dnd_atoms = xmalloc (sizeof *dpyinfo->x_dnd_atoms
                                  * dpyinfo->x_dnd_atoms_size);
#endif
#if 0
  dpyinfo->gray
    = XCreatePixmapFromBitmapData (dpyinfo->display, dpyinfo->root_window,
				   gray_bits, gray_width, gray_height,
				   1, 0, 1);
#endif

#if 0
  x_setup_pointer_blanking (dpyinfo);
#endif

  xsettings_initialize (dpyinfo);

#if 0
  /* This is only needed for distinguishing keyboard and process input.  */
  if (dpyinfo->connection != 0)
    add_keyboard_wait_descriptor (dpyinfo->connection);

#ifdef F_SETOWN
  fcntl (dpyinfo->connection, F_SETOWN, getpid ());
#endif /* ! defined (F_SETOWN) */

  if (interrupt_input)
    init_sigio (dpyinfo->connection);
#endif

  unblock_input ();

  return dpyinfo;
}

char *
pgtk_xlfd_to_fontname (const char *xlfd)
/* --------------------------------------------------------------------------
    Convert an X font name (XLFD) to an NS font name.
    Only family is used.
    The string returned is temporarily allocated.
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("pgtk_xlfd_to_fontname");
  char *name = xmalloc (180);

  if (!strncmp (xlfd, "--", 2)) {
    if (sscanf (xlfd, "--%179[^-]-", name) != 1)
      name[0] = '\0';
  } else {
    if (sscanf (xlfd, "-%*[^-]-%179[^-]-", name) != 1)
      name[0] = '\0';
  }

  /* stopgap for malformed XLFD input */
  if (strlen (name) == 0)
    strcpy (name, "Monospace");

  PGTK_TRACE("converted '%s' to '%s'", xlfd, name);
  return name;
}

bool
pgtk_defined_color (struct frame *f,
                  const char *name,
                  XColor *color_def,
                  bool alloc,
                  bool makeIndex)
/* --------------------------------------------------------------------------
         Return true if named color found, and set color_def rgb accordingly.
         If makeIndex and alloc are nonzero put the color in the color_table,
         and set color_def pixel to the resulting index.
         If makeIndex is zero, set color_def pixel to ARGB.
         Return false if not found
   -------------------------------------------------------------------------- */
{
  // PGTK_TRACE("pgtk_defined_color(%s)", name);
  int r;

  block_input ();
  r = pgtk_parse_color (name, color_def);
  unblock_input ();
  return r;
}

/* On frame F, translate the color name to RGB values.  Use cached
   information, if possible.

   Note that there is currently no way to clean old entries out of the
   cache.  However, it is limited to names in the server's database,
   and names we've actually looked up; list-colors-display is probably
   the most color-intensive case we're likely to hit.  */

int pgtk_parse_color (const char *color_name, XColor *color)
{
  // PGTK_TRACE("pgtk_parse_color: %s", color_name);

  GdkRGBA rgba;
  if (gdk_rgba_parse(&rgba, color_name)) {
    color->red = rgba.red * 65535;
    color->green = rgba.green * 65535;
    color->blue = rgba.blue * 65535;
    color->pixel =
      (unsigned long) 0xff << 24 |
      (color->red >> 8) << 16 |
      (color->green >> 8) << 8 |
      (color->blue >> 8) << 0;
    return 1;
  }
  return 0;
}

int
pgtk_lisp_to_color (Lisp_Object color, XColor *col)
/* --------------------------------------------------------------------------
     Convert a Lisp string object to a NS color
   -------------------------------------------------------------------------- */
{
  PGTK_TRACE("pgtk_lisp_to_color");
  if (STRINGP (color))
    return !pgtk_parse_color (SSDATA (color), col);
  else if (SYMBOLP (color))
    return !pgtk_parse_color (SSDATA (SYMBOL_NAME (color)), col);
  return 1;
}

/* On frame F, translate pixel colors to RGB values for the NCOLORS
   colors in COLORS.  On W32, we no longer try to map colors to
   a palette.  */
void
pgtk_query_colors (struct frame *f, XColor *colors, int ncolors)
{
  PGTK_TRACE("pgtk_query_colors");
  int i;

  for (i = 0; i < ncolors; i++)
    {
      unsigned long pixel = colors[i].pixel;
      /* Convert to a 16 bit value in range 0 - 0xffff. */
#define GetRValue(p) (((p) >> 16) & 0xff)
#define GetGValue(p) (((p) >> 8) & 0xff)
#define GetBValue(p) (((p) >> 0) & 0xff)
      colors[i].red = GetRValue (pixel) * 257;
      colors[i].green = GetGValue (pixel) * 257;
      colors[i].blue = GetBValue (pixel) * 257;
    }
}

void
pgtk_query_color (struct frame *f, XColor *color)
{
  PGTK_TRACE("pgtk_query_color");
  pgtk_query_colors (f, color, 1);
}

void
pgtk_clear_area (struct frame *f, int x, int y, int width, int height)
{
  PGTK_TRACE("pgtk_clear_area: %dx%d+%d+%d.", width, height, x, y);
  cairo_t *cr;

  eassert (width > 0 && height > 0);

  cr = pgtk_begin_cr_clip (f, NULL);
  PGTK_TRACE("back color %08lx.", (unsigned long) f->output_data.pgtk->background_color);
  pgtk_set_cr_source_with_color (f, f->output_data.pgtk->background_color);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);
  pgtk_end_cr_clip (f);
}


void
syms_of_pgtkterm (void)
{
  /* from 23+ we need to tell emacs what modifiers there are.. */
  DEFSYM (Qmodifier_value, "modifier-value");
  DEFSYM (Qalt, "alt");
  DEFSYM (Qhyper, "hyper");
  DEFSYM (Qmeta, "meta");
  DEFSYM (Qsuper, "super");
  DEFSYM (Qcontrol, "control");
  DEFSYM (QUTF8_STRING, "UTF8_STRING");

  DEFSYM (Qfile, "file");
  DEFSYM (Qurl, "url");

  DEFSYM (Qlatin_1, "latin-1");

  Fput (Qalt, Qmodifier_value, make_number (alt_modifier));
  Fput (Qhyper, Qmodifier_value, make_number (hyper_modifier));
  Fput (Qmeta, Qmodifier_value, make_number (meta_modifier));
  Fput (Qsuper, Qmodifier_value, make_number (super_modifier));
  Fput (Qcontrol, Qmodifier_value, make_number (ctrl_modifier));

  DEFVAR_LISP ("x-ctrl-keysym", Vx_ctrl_keysym,
    doc: /* Which keys Emacs uses for the ctrl modifier.
This should be one of the symbols `ctrl', `alt', `hyper', `meta',
`super'.  For example, `ctrl' means use the Ctrl_L and Ctrl_R keysyms.
The default is nil, which is the same as `ctrl'.  */);
  Vx_ctrl_keysym = Qnil;

  DEFVAR_LISP ("x-alt-keysym", Vx_alt_keysym,
    doc: /* Which keys Emacs uses for the alt modifier.
This should be one of the symbols `ctrl', `alt', `hyper', `meta',
`super'.  For example, `alt' means use the Alt_L and Alt_R keysyms.
The default is nil, which is the same as `alt'.  */);
  Vx_alt_keysym = Qnil;

  DEFVAR_LISP ("x-hyper-keysym", Vx_hyper_keysym,
    doc: /* Which keys Emacs uses for the hyper modifier.
This should be one of the symbols `ctrl', `alt', `hyper', `meta',
`super'.  For example, `hyper' means use the Hyper_L and Hyper_R
keysyms.  The default is nil, which is the same as `hyper'.  */);
  Vx_hyper_keysym = Qnil;

  DEFVAR_LISP ("x-meta-keysym", Vx_meta_keysym,
    doc: /* Which keys Emacs uses for the meta modifier.
This should be one of the symbols `ctrl', `alt', `hyper', `meta',
`super'.  For example, `meta' means use the Meta_L and Meta_R keysyms.
The default is nil, which is the same as `meta'.  */);
  Vx_meta_keysym = Qnil;

  DEFVAR_LISP ("x-super-keysym", Vx_super_keysym,
    doc: /* Which keys Emacs uses for the super modifier.
This should be one of the symbols `ctrl', `alt', `hyper', `meta',
`super'.  For example, `super' means use the Super_L and Super_R
keysyms.  The default is nil, which is the same as `super'.  */);
  Vx_super_keysym = Qnil;

  /* TODO: move to common code */
  DEFVAR_LISP ("x-toolkit-scroll-bars", Vx_toolkit_scroll_bars,
	       doc: /* Which toolkit scroll bars Emacs uses, if any.
A value of nil means Emacs doesn't use toolkit scroll bars.
With the X Window system, the value is a symbol describing the
X toolkit.  Possible values are: gtk, motif, xaw, or xaw3d.
With MS Windows or Nextstep, the value is t.  */);
  Vx_toolkit_scroll_bars = Qt;

  DEFVAR_BOOL ("x-use-underline-position-properties",
	       x_use_underline_position_properties,
     doc: /*Non-nil means make use of UNDERLINE_POSITION font properties.
A value of nil means ignore them.  If you encounter fonts with bogus
UNDERLINE_POSITION font properties, for example 7x13 on XFree prior
to 4.1, set this to nil. */);
  x_use_underline_position_properties = 0;

  DEFVAR_BOOL ("x-underline-at-descent-line",
	       x_underline_at_descent_line,
     doc: /* Non-nil means to draw the underline at the same place as the descent line.
A value of nil means to draw the underline according to the value of the
variable `x-use-underline-position-properties', which is usually at the
baseline level.  The default value is nil.  */);
  x_underline_at_descent_line = 0;

  DEFVAR_BOOL ("x-gtk-use-window-move", x_gtk_use_window_move,
    doc: /* Non-nil means rely on gtk_window_move to set frame positions.
If this variable is t (the default), the GTK build uses the function
gtk_window_move to set or store frame positions and disables some time
consuming frame position adjustments.  In newer versions of GTK, Emacs
always uses gtk_window_move and ignores the value of this variable.  */);
  x_gtk_use_window_move = true;

  /* Tell Emacs about this window system.  */
  Fprovide (Qpgtk, Qnil);

}

cairo_t *
pgtk_begin_cr_clip (struct frame *f, XGCValues *gc)
{
  cairo_t *cr = FRAME_CR_CONTEXT (f);

  PGTK_TRACE("pgtk_begin_cr_clip");
  if (! FRAME_CR_SURFACE (f))
    {
      FRAME_CR_SURFACE(f) = gdk_window_create_similar_surface(gtk_widget_get_window (FRAME_GTK_WIDGET (f)),
							      CAIRO_CONTENT_COLOR_ALPHA,
							      FRAME_PIXEL_WIDTH (f),
							      FRAME_PIXEL_HEIGHT (f));
    }

  if (!cr)
    {
      cr = cairo_create (FRAME_CR_SURFACE (f));
      FRAME_CR_CONTEXT (f) = cr;
    }

  cairo_save (cr);

#if 0
  if (gc)
    {
      struct x_gc_ext_data *gc_ext = x_gc_get_ext_data (f, gc, 0);

      if (gc_ext && gc_ext->n_clip_rects)
	{
	  int i;

	  for (i = 0; i < gc_ext->n_clip_rects; i++)
	    cairo_rectangle (cr, gc_ext->clip_rects[i].x,
			     gc_ext->clip_rects[i].y,
			     gc_ext->clip_rects[i].width,
			     gc_ext->clip_rects[i].height);
	  cairo_clip (cr);
	}
    }
#endif

  return cr;
}

void
pgtk_end_cr_clip (struct frame *f)
{
  PGTK_TRACE("pgtk_end_cr_clip");
  cairo_restore (FRAME_CR_CONTEXT (f));

  GtkWidget *widget = FRAME_GTK_WIDGET(f);
  gtk_widget_queue_draw(widget);
}

void
pgtk_set_cr_source_with_gc_foreground (struct frame *f, XGCValues *gc)
{
  PGTK_TRACE("pgtk_set_cr_source_with_gc_foreground: %08lx", gc->foreground);
  pgtk_set_cr_source_with_color(f, gc->foreground);
}

void
pgtk_set_cr_source_with_gc_background (struct frame *f, XGCValues *gc)
{
  PGTK_TRACE("pgtk_set_cr_source_with_gc_background: %08lx", gc->background);
  pgtk_set_cr_source_with_color(f, gc->background);
}

void
pgtk_set_cr_source_with_color (struct frame *f, unsigned long color)
{
  PGTK_TRACE("pgtk_set_cr_source_with_color: %08lx.", color);
  XColor col;
  col.pixel = color;
  pgtk_query_color(f, &col);
  cairo_set_source_rgb (FRAME_CR_CONTEXT (f), col.red / 65535.0,
			col.green / 65535.0, col.blue / 65535.0);
}

void
pgtk_cr_draw_frame (cairo_t *cr, struct frame *f)
{
  PGTK_TRACE("pgtk_cr_draw_frame");
#if 0
  int width, height;

  width = FRAME_PIXEL_WIDTH (f);
  height = FRAME_PIXEL_HEIGHT (f);

#if 0
  x_free_cr_resources (f);
#endif
  FRAME_CR_CONTEXT (f) = cr;
  pgtk_clear_area (f, 0, 0, width, height);
  expose_frame (f, 0, 0, width, height);
  FRAME_CR_CONTEXT (f) = NULL;
#endif
}

static void
pgtk_cr_destroy_surface(struct frame *f)
{
  PGTK_TRACE("pgtk_cr_destroy_surface");
  if (FRAME_CR_CONTEXT(f) != NULL) {
    cairo_destroy(FRAME_CR_CONTEXT(f));
    FRAME_CR_CONTEXT(f) = NULL;
  }
  if (FRAME_CR_SURFACE(f) != NULL) {
    cairo_surface_destroy(FRAME_CR_SURFACE(f));
    FRAME_CR_SURFACE(f) = NULL;
  }
}

void
init_pgtkterm (void)
{
  /* FD for select() is unknown on pure gtk, so I can't setup
     SIGIO for it. I use polling, not interrupts.
     However, alarm does not occur when timerfd is available.
     I can't poll without alarm, so disable timerfd. */
  xputenv ("EMACS_IGNORE_TIMERFD=1");
}

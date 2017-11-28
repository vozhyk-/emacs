/* Functions for the Gtk+-3 with wayland.

Copyright (C) 1989, 1992-1994, 2005-2006, 2008-2017 Free Software
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

#include <math.h>
#include <c-strcase.h>

#include "lisp.h"
#include "blockinput.h"
#include "gtkutil.h"
#include "window.h"
#include "character.h"
#include "buffer.h"
#include "keyboard.h"
#include "termhooks.h"
#include "fontset.h"
#include "font.h"


#ifdef HAVE_GTK3WL

#define wx gtk3wl

//static EmacsTooltip *gtk3wl_tooltip = nil;

/* Static variables to handle applescript execution.  */
static Lisp_Object as_script, *as_result;
static int as_status;

static ptrdiff_t image_cache_refcount;

static struct gtk3wl_display_info *gtk3wl_display_info_for_name (Lisp_Object);
static void gtk3wl_set_name_as_filename (struct frame *);

static const char *gtk3wl_app_name = "gtk3wl_app_name:Emacs";

/* ==========================================================================

    Internal utility functions

   ========================================================================== */


/* Let the user specify a Nextstep display with a Lisp object.
   OBJECT may be nil, a frame or a terminal object.
   nil stands for the selected frame--or, if that is not a Nextstep frame,
   the first Nextstep display on the list.  */

static struct gtk3wl_display_info *
check_gtk3wl_display_info (Lisp_Object object)
{
  struct gtk3wl_display_info *dpyinfo = NULL;

  if (NILP (object))
    {
      struct frame *sf = XFRAME (selected_frame);

      if (FRAME_GTK3WL_P (sf) && FRAME_LIVE_P (sf))
	dpyinfo = FRAME_DISPLAY_INFO (sf);
      else if (x_display_list != 0)
	dpyinfo = x_display_list;
      else
        error ("Nextstep windows are not in use or not initialized");
    }
  else if (TERMINALP (object))
    {
      struct terminal *t = decode_live_terminal (object);

      if (t->type != output_ns)
        error ("Terminal %d is not a Nextstep display", t->id);

      dpyinfo = t->display_info.gtk3wl;
    }
  else if (STRINGP (object))
    dpyinfo = gtk3wl_display_info_for_name (object);
  else
    {
      struct frame *f = decode_window_system_frame (object);
      dpyinfo = FRAME_DISPLAY_INFO (f);
    }

  return dpyinfo;
}

#if 0

static id
gtk3wl_get_window (Lisp_Object maybeFrame)
{
  id view =nil, window =nil;

  if (!FRAMEP (maybeFrame) || !FRAME_GTK3WL_P (XFRAME (maybeFrame)))
    maybeFrame = selected_frame;/*wrong_type_argument (Qframep, maybeFrame); */

  if (!NILP (maybeFrame))
    view = FRAME_GTK3WL_VIEW (XFRAME (maybeFrame));
  if (view) window =[view window];

  return window;
}

#endif

/* Return the X display structure for the display named NAME.
   Open a new connection if necessary.  */
static struct gtk3wl_display_info *
gtk3wl_display_info_for_name (Lisp_Object name)
{
  struct gtk3wl_display_info *dpyinfo;

  CHECK_STRING (name);

  for (dpyinfo = x_display_list; dpyinfo; dpyinfo = dpyinfo->next)
    if (!NILP (Fstring_equal (XCAR (dpyinfo->name_list_element), name)))
      return dpyinfo;

  error ("Emacs for Nextstep does not yet support multi-display");

  Fx_open_connection (name, Qnil, Qnil);
  dpyinfo = x_display_list;

  if (dpyinfo == 0)
    error ("Display on %s not responding.\n", SDATA (name));

  return dpyinfo;
}

#if 0

static NSString *
gtk3wl_filename_from_panel (NSSavePanel *panel)
{
#ifdef GTK3WL_IMPL_COCOA
  NSURL *url = [panel URL];
  NSString *str = [url path];
  return str;
#else
  return [panel filename];
#endif
}

static NSString *
gtk3wl_directory_from_panel (NSSavePanel *panel)
{
#ifdef GTK3WL_IMPL_COCOA
  NSURL *url = [panel directoryURL];
  NSString *str = [url path];
  return str;
#else
  return [panel directory];
#endif
}

#ifndef GTK3WL_IMPL_COCOA
static Lisp_Object
interpret_services_menu (NSMenu *menu, Lisp_Object prefix, Lisp_Object old)
/* --------------------------------------------------------------------------
   Turn the input menu (an NSMenu) into a lisp list for tracking on lisp side
   -------------------------------------------------------------------------- */
{
  int i, count;
  NSMenuItem *item;
  const char *name;
  Lisp_Object nameStr;
  unsigned short key;
  NSString *keys;
  Lisp_Object res;

  count = [menu numberOfItems];
  for (i = 0; i<count; i++)
    {
      item = [menu itemAtIndex: i];
      name = [[item title] UTF8String];
      if (!name) continue;

      nameStr = build_string (name);

      if ([item hasSubmenu])
        {
          old = interpret_services_menu ([item submenu],
                                        Fcons (nameStr, prefix), old);
        }
      else
        {
          keys = [item keyEquivalent];
          if (keys && [keys length] )
            {
              key = [keys characterAtIndex: 0];
              res = make_number (key|super_modifier);
            }
          else
            {
              res = Qundefined;
            }
          old = Fcons (Fcons (res,
                            Freverse (Fcons (nameStr,
                                           prefix))),
                    old);
        }
    }
  return old;
}
#endif

#endif

/* ==========================================================================

    Frame parameter setters

   ========================================================================== */


static void
x_set_foreground_color (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
  XColor col;

  /* Must block_input, because gtk3wl_lisp_to_color does block/unblock_input
     which means that col may be deallocated in its unblock_input if there
     is user input, unless we also block_input.  */
  block_input ();
  if (gtk3wl_lisp_to_color (arg, &col))
    {
      store_frame_param (f, Qforeground_color, oldval);
      unblock_input ();
      error ("Unknown color");
    }

  f->output_data.gtk3wl->foreground_color = col.pixel;

  FRAME_FOREGROUND_PIXEL (f) = col.pixel;

  if (FRAME_GTK_WIDGET (f))
    {
      update_face_from_frame_parameter (f, Qforeground_color, arg);
      /*recompute_basic_faces (f); */
      if (FRAME_VISIBLE_P (f))
        SET_FRAME_GARBAGED (f);
    }
  unblock_input ();
}


static void
x_set_background_color (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
  XColor col;
  struct face *face;

  block_input ();
  if (gtk3wl_lisp_to_color (arg, &col))
    {
      store_frame_param (f, Qbackground_color, oldval);
      unblock_input ();
      error ("Unknown color");
    }

  /* clear the frame; in some instances the NS-internal GC appears not to
     update, or it does update and cannot clear old text properly */
  if (FRAME_VISIBLE_P (f))
    gtk3wl_clear_frame (f);

  GTK3WL_TRACE("x_set_background_color: col.pixel=%08lx.", col.pixel);
  f->output_data.gtk3wl->background_color = col.pixel;

  xg_set_background_color(f, col.pixel);
  update_face_from_frame_parameter (f, Qbackground_color, arg);

  GTK3WL_TRACE("visible_p=%d.", FRAME_VISIBLE_P(f));
  if (FRAME_VISIBLE_P (f))
    SET_FRAME_GARBAGED (f);

  unblock_input ();
}


static void
x_set_cursor_color (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
  XColor col;

  block_input ();
  if (gtk3wl_lisp_to_color (arg, &col))
    {
      store_frame_param (f, Qcursor_color, oldval);
      unblock_input ();
      error ("Unknown color");
    }

  FRAME_CURSOR_COLOR (f) = col.pixel;

  if (FRAME_VISIBLE_P (f))
    {
      x_update_cursor (f, 0);
      x_update_cursor (f, 1);
    }
  update_face_from_frame_parameter (f, Qcursor_color, arg);
  unblock_input ();
}


static void
x_set_icon_name (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  NSView *view = FRAME_GTK3WL_VIEW (f);
  NSTRACE ("x_set_icon_name");

  /* see if it's changed */
  if (STRINGP (arg))
    {
      if (STRINGP (oldval) && EQ (Fstring_equal (oldval, arg), Qt))
        return;
    }
  else if (!STRINGP (oldval) && EQ (oldval, Qnil) == EQ (arg, Qnil))
    return;

  fset_icon_name (f, arg);

  if (NILP (arg))
    {
      if (!NILP (f->title))
        arg = f->title;
      else
        /* Explicit name and no icon-name -> explicit_name.  */
        if (f->explicit_name)
          arg = f->name;
        else
          {
            /* No explicit name and no icon-name ->
               name has to be rebuild from icon_title_format.  */
            windows_or_buffers_changed = 62;
            return;
          }
    }

  /* Don't change the name if it's already NAME.  */
  if ([[view window] miniwindowTitle]
      && ([[[view window] miniwindowTitle]
             isEqualToString: [NSString stringWithUTF8String:
					  SSDATA (arg)]]))
    return;

  [[view window] setMiniwindowTitle:
        [NSString stringWithUTF8String: SSDATA (arg)]];
#endif
}

static void
gtk3wl_set_name_internal (struct frame *f, Lisp_Object name)
{
#if 0
  Lisp_Object encoded_name, encoded_icon_name;
  NSString *str;
  NSView *view = FRAME_GTK3WL_VIEW (f);


  encoded_name = ENCODE_UTF_8 (name);

  str = [NSString stringWithUTF8String: SSDATA (encoded_name)];


  /* Don't change the name if it's already NAME.  */
  if (! [[[view window] title] isEqualToString: str])
    [[view window] setTitle: str];

  if (!STRINGP (f->icon_name))
    encoded_icon_name = encoded_name;
  else
    encoded_icon_name = ENCODE_UTF_8 (f->icon_name);

  str = [NSString stringWithUTF8String: SSDATA (encoded_icon_name)];

  if ([[view window] miniwindowTitle]
      && ! [[[view window] miniwindowTitle] isEqualToString: str])
    [[view window] setMiniwindowTitle: str];

#endif
}

static void
gtk3wl_set_name (struct frame *f, Lisp_Object name, int explicit)
{
#if 0
  NSTRACE ("gtk3wl_set_name");

  /* Make sure that requests from lisp code override requests from
     Emacs redisplay code.  */
  if (explicit)
    {
      /* If we're switching from explicit to implicit, we had better
         update the mode lines and thereby update the title.  */
      if (f->explicit_name && NILP (name))
        update_mode_lines = 21;

      f->explicit_name = ! NILP (name);
    }
  else if (f->explicit_name)
    return;

  if (NILP (name))
    name = build_string (gtk3wl_app_name);
  else
    CHECK_STRING (name);

  /* Don't change the name if it's already NAME.  */
  if (! NILP (Fstring_equal (name, f->name)))
    return;

  fset_name (f, name);

  /* Title overrides explicit name.  */
  if (! NILP (f->title))
    name = f->title;

  gtk3wl_set_name_internal (f, name);
#endif
}


/* This function should be called when the user's lisp code has
   specified a name for the frame; the name will override any set by the
   redisplay code.  */
static void
x_explicitly_set_name (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  NSTRACE ("x_explicitly_set_name");
  gtk3wl_set_name (f, arg, 1);
#endif
}


/* This function should be called by Emacs redisplay code to set the
   name; names set this way will never override names set by the user's
   lisp code.  */
void
x_implicitly_set_name (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  NSTRACE ("x_implicitly_set_name");

  Lisp_Object frame_title = buffer_local_value
    (Qframe_title_format, XWINDOW (f->selected_window)->contents);
  Lisp_Object icon_title = buffer_local_value
    (Qicon_title_format, XWINDOW (f->selected_window)->contents);

  /* Deal with NS specific format t.  */
  if (FRAME_GTK3WL_P (f) && ((FRAME_ICONIFIED_P (f) && EQ (icon_title, Qt))
                         || EQ (frame_title, Qt)))
    gtk3wl_set_name_as_filename (f);
  else
    gtk3wl_set_name (f, arg, 0);
#endif
}


/* Change the title of frame F to NAME.
   If NAME is nil, use the frame name as the title.  */

static void
x_set_title (struct frame *f, Lisp_Object name, Lisp_Object old_name)
{
#if 0
  NSTRACE ("x_set_title");
  /* Don't change the title if it's already NAME.  */
  if (EQ (name, f->title))
    return;

  update_mode_lines = 22;

  fset_title (f, name);

  if (NILP (name))
    name = f->name;
  else
    CHECK_STRING (name);

  gtk3wl_set_name_internal (f, name);
#endif
}


static void
gtk3wl_set_name_as_filename (struct frame *f)
{
#if 0
  NSView *view;
  Lisp_Object name, filename;
  Lisp_Object buf = XWINDOW (f->selected_window)->contents;
  const char *title;
  NSAutoreleasePool *pool;
  Lisp_Object encoded_name, encoded_filename;
  NSString *str;
  NSTRACE ("gtk3wl_set_name_as_filename");

  if (f->explicit_name || ! NILP (f->title))
    return;

  block_input ();
  pool = [[NSAutoreleasePool alloc] init];
  filename = BVAR (XBUFFER (buf), filename);
  name = BVAR (XBUFFER (buf), name);

  if (NILP (name))
    {
      if (! NILP (filename))
        name = Ffile_name_nondirectory (filename);
      else
        name = build_string (gtk3wl_app_name);
    }

  encoded_name = ENCODE_UTF_8 (name);

  view = FRAME_GTK3WL_VIEW (f);

  title = FRAME_ICONIFIED_P (f) ? [[[view window] miniwindowTitle] UTF8String]
                                : [[[view window] title] UTF8String];

  if (title && (! strcmp (title, SSDATA (encoded_name))))
    {
      [pool release];
      unblock_input ();
      return;
    }

  str = [NSString stringWithUTF8String: SSDATA (encoded_name)];
  if (str == nil) str = @"Bad coding";

  if (FRAME_ICONIFIED_P (f))
    [[view window] setMiniwindowTitle: str];
  else
    {
      NSString *fstr;

      if (! NILP (filename))
        {
          encoded_filename = ENCODE_UTF_8 (filename);

          fstr = [NSString stringWithUTF8String: SSDATA (encoded_filename)];
          if (fstr == nil) fstr = @"";
        }
      else
        fstr = @"";

      gtk3wl_set_represented_filename (fstr, f);
      [[view window] setTitle: str];
      fset_name (f, name);
    }

  [pool release];
  unblock_input ();
#endif
}


void
gtk3wl_set_doc_edited (void)
{
#if 0
  NSAutoreleasePool *pool;
  Lisp_Object tail, frame;
  block_input ();
  pool = [[NSAutoreleasePool alloc] init];
  FOR_EACH_FRAME (tail, frame)
    {
      BOOL edited = NO;
      struct frame *f = XFRAME (frame);
      struct window *w;
      NSView *view;

      if (! FRAME_GTK3WL_P (f)) continue;
      w = XWINDOW (FRAME_SELECTED_WINDOW (f));
      view = FRAME_GTK3WL_VIEW (f);
      if (!MINI_WINDOW_P (w))
        edited = ! NILP (Fbuffer_modified_p (w->contents)) &&
          ! NILP (Fbuffer_file_name (w->contents));
      [[view window] setDocumentEdited: edited];
    }

  [pool release];
  unblock_input ();
#endif
}


static void
x_set_menu_bar_lines (struct frame *f, Lisp_Object value, Lisp_Object oldval)
{
#if 0
  int nlines;
  if (FRAME_MINIBUF_ONLY_P (f))
    return;

  if (TYPE_RANGED_INTEGERP (int, value))
    nlines = XINT (value);
  else
    nlines = 0;

  FRAME_MENU_BAR_LINES (f) = 0;
  if (nlines)
    {
      FRAME_EXTERNAL_MENU_BAR (f) = 1;
      /* does for all frames, whereas we just want for one frame
	 [NSMenu setMenuBarVisible: YES]; */
    }
  else
    {
      if (FRAME_EXTERNAL_MENU_BAR (f) == 1)
        free_frame_menubar (f);
      /*      [NSMenu setMenuBarVisible: NO]; */
      FRAME_EXTERNAL_MENU_BAR (f) = 0;
    }
#endif
}


/* toolbar support */
static void
x_set_tool_bar_lines (struct frame *f, Lisp_Object value, Lisp_Object oldval)
{
#if 0
  /* Currently, when the tool bar change state, the frame is resized.

     TODO: It would be better if this didn't occur when 1) the frame
     is full height or maximized or 2) when specified by
     `frame-inhibit-implied-resize'. */
  int nlines;

  NSTRACE ("x_set_tool_bar_lines");

  if (FRAME_MINIBUF_ONLY_P (f))
    return;

  if (RANGED_INTEGERP (0, value, INT_MAX))
    nlines = XFASTINT (value);
  else
    nlines = 0;

  if (nlines)
    {
      FRAME_EXTERNAL_TOOL_BAR (f) = 1;
      update_frame_tool_bar (f);
    }
  else
    {
      if (FRAME_EXTERNAL_TOOL_BAR (f))
        {
          free_frame_tool_bar (f);
          FRAME_EXTERNAL_TOOL_BAR (f) = 0;

          {
            EmacsView *view = FRAME_GTK3WL_VIEW (f);
            int fs_state = [view fullscreenState];

            if (fs_state == FULLSCREEN_MAXIMIZED)
              {
                [view setFSValue:FULLSCREEN_WIDTH];
              }
            else if (fs_state == FULLSCREEN_HEIGHT)
              {
                [view setFSValue:FULLSCREEN_NONE];
              }
          }
       }
    }

  {
    int inhibit
      = ((f->after_make_frame
	  && !f->tool_bar_resized
	  && (EQ (frame_inhibit_implied_resize, Qt)
	      || (CONSP (frame_inhibit_implied_resize)
		  && !NILP (Fmemq (Qtool_bar_lines,
				   frame_inhibit_implied_resize))))
	  && NILP (get_frame_param (f, Qfullscreen)))
	 ? 0
	 : 2);

    NSTRACE_MSG ("inhibit:%d", inhibit);

    frame_size_history_add (f, Qupdate_frame_tool_bar, 0, 0, Qnil);
    adjust_frame_size (f, -1, -1, inhibit, 0, Qtool_bar_lines);
  }
#endif
}


static void
x_set_internal_border_width (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  int old_width = FRAME_INTERNAL_BORDER_WIDTH (f);

  CHECK_TYPE_RANGED_INTEGER (int, arg);
  f->internal_border_width = XINT (arg);
  if (FRAME_INTERNAL_BORDER_WIDTH (f) < 0)
    f->internal_border_width = 0;

  if (FRAME_INTERNAL_BORDER_WIDTH (f) == old_width)
    return;

  if (FRAME_X_WINDOW (f) != 0)
    adjust_frame_size (f, -1, -1, 3, 0, Qinternal_border_width);

  SET_FRAME_GARBAGED (f);
#endif
}


static void
gtk3wl_implicitly_set_icon_type (struct frame *f)
{
#if 0
  Lisp_Object tem;
  EmacsView *view = FRAME_GTK3WL_VIEW (f);
  id image = nil;
  Lisp_Object chain, elt;
  NSAutoreleasePool *pool;
  BOOL setMini = YES;

  NSTRACE ("gtk3wl_implicitly_set_icon_type");

  block_input ();
  pool = [[NSAutoreleasePool alloc] init];
  if (f->output_data.gtk3wl->miniimage
      && [[NSString stringWithUTF8String: SSDATA (f->name)]
               isEqualToString: [(NSImage *)f->output_data.gtk3wl->miniimage name]])
    {
      [pool release];
      unblock_input ();
      return;
    }

  tem = assq_no_quit (Qicon_type, f->param_alist);
  if (CONSP (tem) && ! NILP (XCDR (tem)))
    {
      [pool release];
      unblock_input ();
      return;
    }

  for (chain = Vgtk3wl_icon_type_alist;
       image == nil && CONSP (chain);
       chain = XCDR (chain))
    {
      elt = XCAR (chain);
      /* special case: t means go by file type */
      if (SYMBOLP (elt) && EQ (elt, Qt) && SSDATA (f->name)[0] == '/')
        {
          NSString *str
	     = [NSString stringWithUTF8String: SSDATA (f->name)];
          if ([[NSFileManager defaultManager] fileExistsAtPath: str])
            image = [[[NSWorkspace sharedWorkspace] iconForFile: str] retain];
        }
      else if (CONSP (elt) &&
               STRINGP (XCAR (elt)) &&
               STRINGP (XCDR (elt)) &&
               fast_string_match (XCAR (elt), f->name) >= 0)
        {
          image = [EmacsImage allocInitFromFile: XCDR (elt)];
          if (image == nil)
            image = [[NSImage imageNamed:
                               [NSString stringWithUTF8String:
					    SSDATA (XCDR (elt))]] retain];
        }
    }

  if (image == nil)
    {
      image = [[[NSWorkspace sharedWorkspace] iconForFileType: @"text"] retain];
      setMini = NO;
    }

  [f->output_data.gtk3wl->miniimage release];
  f->output_data.gtk3wl->miniimage = image;
  [view setMiniwindowImage: setMini];
  [pool release];
  unblock_input ();
#endif
}


static void
x_set_icon_type (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  EmacsView *view = FRAME_GTK3WL_VIEW (f);
  id image = nil;
  BOOL setMini = YES;

  NSTRACE ("x_set_icon_type");

  if (!NILP (arg) && SYMBOLP (arg))
    {
      arg =build_string (SSDATA (SYMBOL_NAME (arg)));
      store_frame_param (f, Qicon_type, arg);
    }

  /* do it the implicit way */
  if (NILP (arg))
    {
      gtk3wl_implicitly_set_icon_type (f);
      return;
    }

  CHECK_STRING (arg);

  image = [EmacsImage allocInitFromFile: arg];
  if (image == nil)
    image =[NSImage imageNamed: [NSString stringWithUTF8String:
                                            SSDATA (arg)]];

  if (image == nil)
    {
      image = [NSImage imageNamed: @"text"];
      setMini = NO;
    }

  f->output_data.gtk3wl->miniimage = image;
  [view setMiniwindowImage: setMini];
#endif
}

/* This is the same as the xfns.c definition.  */
static void
x_set_cursor_type (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
  set_frame_cursor_types (f, arg);
}

/* called to set mouse pointer color, but all other terms use it to
   initialize pointer types (and don't set the color ;) */
static void
x_set_mouse_color (struct frame *f, Lisp_Object arg, Lisp_Object oldval)
{
#if 0
  /* don't think we can do this on Nextstep */
#endif
}


#define Str(x) #x
#define Xstr(x) Str(x)

static Lisp_Object
gtk3wl_appkit_version_str (void)
{
#if 0
  char tmp[256];

#ifdef GTK3WL_IMPL_GNUSTEP
  sprintf(tmp, "gnustep-gui-%s", Xstr(GNUSTEP_GUI_VERSION));
#elif defined (GTK3WL_IMPL_COCOA)
  NSString *osversion
    = [[NSProcessInfo processInfo] operatingSystemVersionString];
  sprintf(tmp, "appkit-%.2f %s",
          NSAppKitVersionNumber,
          [osversion UTF8String]);
#else
  tmp = "ns-unknown";
#endif
  return build_string (tmp);
#else
  return Qnil;
#endif
}


/* This is for use by x-server-version and collapses all version info we
   have into a single int.  For a better picture of the implementation
   running, use gtk3wl_appkit_version_str.*/
static int
gtk3wl_appkit_version_int (void)
{
#if 0
#ifdef GTK3WL_IMPL_GNUSTEP
  return GNUSTEP_GUI_MAJOR_VERSION * 100 + GNUSTEP_GUI_MINOR_VERSION;
#elif defined (GTK3WL_IMPL_COCOA)
  return (int)NSAppKitVersionNumber;
#endif
#endif
  return 0;
}


static void
x_icon (struct frame *f, Lisp_Object parms)
/* --------------------------------------------------------------------------
   Strangely-named function to set icon position parameters in frame.
   This is irrelevant under macOS, but might be needed under GNUstep,
   depending on the window manager used.  Note, this is not a standard
   frame parameter-setter; it is called directly from x-create-frame.
   -------------------------------------------------------------------------- */
{
#if 0
  Lisp_Object icon_x, icon_y;
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (Qnil);

  f->output_data.gtk3wl->icon_top = -1;
  f->output_data.gtk3wl->icon_left = -1;

  /* Set the position of the icon.  */
  icon_x = x_get_arg (dpyinfo, parms, Qicon_left, 0, 0, RES_TYPE_NUMBER);
  icon_y = x_get_arg (dpyinfo, parms, Qicon_top, 0, 0,  RES_TYPE_NUMBER);
  if (!EQ (icon_x, Qunbound) && !EQ (icon_y, Qunbound))
    {
      CHECK_NUMBER (icon_x);
      CHECK_NUMBER (icon_y);
      f->output_data.gtk3wl->icon_top = XINT (icon_y);
      f->output_data.gtk3wl->icon_left = XINT (icon_x);
    }
  else if (!EQ (icon_x, Qunbound) || !EQ (icon_y, Qunbound))
    error ("Both left and top icon corners of icon must be specified");
#endif
}


/* Note: see frame.c for template, also where generic functions are impl */
frame_parm_handler gtk3wl_frame_parm_handlers[] =
{
  x_set_autoraise, /* generic OK */
  x_set_autolower, /* generic OK */
  x_set_background_color,
  0, /* x_set_border_color,  may be impossible under Nextstep */
  0, /* x_set_border_width,  may be impossible under Nextstep */
  x_set_cursor_color,
  x_set_cursor_type,
  x_set_font, /* generic OK */
  x_set_foreground_color,
  x_set_icon_name,
  x_set_icon_type,
  x_set_internal_border_width, /* generic OK */
  x_set_right_divider_width,
  x_set_bottom_divider_width,
  x_set_menu_bar_lines,
  x_set_mouse_color,
  x_explicitly_set_name,
  x_set_scroll_bar_width, /* generic OK */
  x_set_scroll_bar_height, /* generic OK */
  x_set_title,
  x_set_unsplittable, /* generic OK */
  x_set_vertical_scroll_bars, /* generic OK */
  x_set_horizontal_scroll_bars, /* generic OK */
  x_set_visibility, /* generic OK */
  x_set_tool_bar_lines,
  0, /* x_set_scroll_bar_foreground, will ignore (not possible on NS) */
  0, /* x_set_scroll_bar_background,  will ignore (not possible on NS) */
  x_set_screen_gamma, /* generic OK */
  x_set_line_spacing, /* generic OK, sets f->extra_line_spacing to int */
  x_set_left_fringe, /* generic OK */
  x_set_right_fringe, /* generic OK */
  0, /* x_set_wait_for_wm, will ignore */
  x_set_fullscreen, /* generic OK */
  x_set_font_backend, /* generic OK */
  x_set_alpha,
  0, /* x_set_sticky */
  0, /* x_set_tool_bar_position */
  0, /* x_set_inhibit_double_buffering */
#ifdef GTK3WL_IMPL_COCOA
  x_set_undecorated,
#else
  0, /*x_set_undecorated */
#endif
  x_set_parent_frame,
  0, /* x_set_skip_taskbar */
  x_set_no_focus_on_map,
  x_set_no_accept_focus,
  x_set_z_group, /* x_set_z_group */
  0, /* x_set_override_redirect */
  x_set_no_special_glyphs,
#ifdef GTK3WL_IMPL_COCOA
  gtk3wl_set_appearance,
  gtk3wl_set_transparent_titlebar,
#endif
};


/* Handler for signals raised during x_create_frame.
   FRAME is the frame which is partially constructed.  */

static void
unwind_create_frame (Lisp_Object frame)
{
#if 0
  struct frame *f = XFRAME (frame);

  /* If frame is already dead, nothing to do.  This can happen if the
     display is disconnected after the frame has become official, but
     before x_create_frame removes the unwind protect.  */
  if (!FRAME_LIVE_P (f))
    return;

  /* If frame is ``official'', nothing to do.  */
  if (NILP (Fmemq (frame, Vframe_list)))
    {
#if defined GLYPH_DEBUG && defined ENABLE_CHECKING
      struct gtk3wl_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);
#endif

      /* If the frame's image cache refcount is still the same as our
	 private shadow variable, it means we are unwinding a frame
	 for which we didn't yet call init_frame_faces, where the
	 refcount is incremented.  Therefore, we increment it here, so
	 that free_frame_faces, called in x_free_frame_resources
	 below, will not mistakenly decrement the counter that was not
	 incremented yet to account for this new frame.  */
      if (FRAME_IMAGE_CACHE (f) != NULL
	  && FRAME_IMAGE_CACHE (f)->refcount == image_cache_refcount)
	FRAME_IMAGE_CACHE (f)->refcount++;

      x_free_frame_resources (f);
      free_glyphs (f);

#if defined GLYPH_DEBUG && defined ENABLE_CHECKING
      /* Check that reference counts are indeed correct.  */
      eassert (dpyinfo->terminal->image_cache->refcount == image_cache_refcount);
#endif
    }
#endif
}

/*
 * Read geometry related parameters from preferences if not in PARMS.
 * Returns the union of parms and any preferences read.
 */

static Lisp_Object
get_geometry_from_preferences (struct gtk3wl_display_info *dpyinfo,
                               Lisp_Object parms)
{
#if 0
  struct {
    const char *val;
    const char *cls;
    Lisp_Object tem;
  } r[] = {
    { "width",  "Width", Qwidth },
    { "height", "Height", Qheight },
    { "left", "Left", Qleft },
    { "top", "Top", Qtop },
  };

  int i;
  for (i = 0; i < ARRAYELTS (r); ++i)
    {
      if (NILP (Fassq (r[i].tem, parms)))
        {
          Lisp_Object value
            = x_get_arg (dpyinfo, parms, r[i].tem, r[i].val, r[i].cls,
                         RES_TYPE_NUMBER);
          if (! EQ (value, Qunbound))
            parms = Fcons (Fcons (r[i].tem, value), parms);
        }
    }

  return parms;
#else
  return Qnil;
#endif
}

/* ==========================================================================

    Lisp definitions

   ========================================================================== */

DEFUN ("x-create-frame", Fx_create_frame, Sx_create_frame,
       1, 1, 0,
       doc: /* Make a new Nextstep window, called a "frame" in Emacs terms.
Return an Emacs frame object.
PARMS is an alist of frame parameters.
If the parameters specify that the frame should not have a minibuffer,
and do not specify a specific minibuffer window to use,
then `default-minibuffer-frame' must be a frame whose minibuffer can
be shared by the new frame.

This function is an internal primitive--use `make-frame' instead.  */)
     (Lisp_Object parms)
{
  struct frame *f;
  Lisp_Object frame, tem;
  Lisp_Object name;
  int minibuffer_only = 0;
  long window_prompting = 0;
  ptrdiff_t count = specpdl_ptr - specpdl;
  Lisp_Object display;
  struct gtk3wl_display_info *dpyinfo = NULL;
  Lisp_Object parent, parent_frame;
  struct kboard *kb;
  static int desc_ctr = 1;
  int x_width = 0, x_height = 0;

  /* x_get_arg modifies parms.  */
  parms = Fcopy_alist (parms);

  /* Use this general default value to start with
     until we know if this frame has a specified name.  */
  Vx_resource_name = Vinvocation_name;

  display = x_get_arg (dpyinfo, parms, Qterminal, 0, 0, RES_TYPE_STRING);
  if (EQ (display, Qunbound))
    display = Qnil;
  dpyinfo = check_gtk3wl_display_info (display);
  kb = dpyinfo->terminal->kboard;

  if (!dpyinfo->terminal->name)
    error ("Terminal is not live, can't create new frames on it");

  name = x_get_arg (dpyinfo, parms, Qname, 0, 0, RES_TYPE_STRING);
  if (!STRINGP (name)
      && ! EQ (name, Qunbound)
      && ! NILP (name))
    error ("Invalid frame name--not a string or nil");

  if (STRINGP (name))
    Vx_resource_name = name;

  parent = x_get_arg (dpyinfo, parms, Qparent_id, 0, 0, RES_TYPE_NUMBER);
  if (EQ (parent, Qunbound))
    parent = Qnil;
  if (! NILP (parent))
    CHECK_NUMBER (parent);

  /* make_frame_without_minibuffer can run Lisp code and garbage collect.  */
  /* No need to protect DISPLAY because that's not used after passing
     it to make_frame_without_minibuffer.  */
  frame = Qnil;
  tem = x_get_arg (dpyinfo, parms, Qminibuffer, "minibuffer", "Minibuffer",
                  RES_TYPE_SYMBOL);
  if (EQ (tem, Qnone) || NILP (tem))
      f = make_frame_without_minibuffer (Qnil, kb, display);
  else if (EQ (tem, Qonly))
    {
      f = make_minibuffer_frame ();
      minibuffer_only = 1;
    }
  else if (WINDOWP (tem))
      f = make_frame_without_minibuffer (tem, kb, display);
  else
      f = make_frame (1);

  XSETFRAME (frame, f);

  f->terminal = dpyinfo->terminal;

  f->output_method = output_gtk3wl;
  f->output_data.gtk3wl = xzalloc (sizeof *f->output_data.gtk3wl);

  FRAME_FONTSET (f) = -1;

  fset_icon_name (f, x_get_arg (dpyinfo, parms, Qicon_name,
				"iconName", "Title",
				RES_TYPE_STRING));
  if (! STRINGP (f->icon_name))
    fset_icon_name (f, Qnil);

  FRAME_DISPLAY_INFO (f) = dpyinfo;

  /* With FRAME_DISPLAY_INFO set up, this unwind-protect is safe.  */
  record_unwind_protect (unwind_create_frame, frame);

  f->output_data.gtk3wl->window_desc = desc_ctr++;
  if (TYPE_RANGED_INTEGERP (Window, parent))
    {
      f->output_data.gtk3wl->parent_desc = XFASTINT (parent);
      f->output_data.gtk3wl->explicit_parent = 1;
    }
  else
    {
      f->output_data.gtk3wl->parent_desc = FRAME_DISPLAY_INFO (f)->root_window;
      f->output_data.gtk3wl->explicit_parent = 0;
    }

  /* Set the name; the functions to which we pass f expect the name to
     be set.  */
  if (EQ (name, Qunbound) || NILP (name) || ! STRINGP (name))
    {
      fset_name (f, build_string (gtk3wl_app_name));
      f->explicit_name = 0;
    }
  else
    {
      fset_name (f, name);
      f->explicit_name = 1;
      specbind (Qx_resource_name, name);
    }

  block_input ();

  register_font_driver (&ftcrfont_driver, f);

  image_cache_refcount =
    FRAME_IMAGE_CACHE (f) ? FRAME_IMAGE_CACHE (f)->refcount : 0;

  x_default_parameter (f, parms, Qfont_backend, Qnil,
			"fontBackend", "FontBackend", RES_TYPE_STRING);

#if 0
  {
    /* use for default font name */
    id font = [NSFont userFixedPitchFontOfSize: -1.0]; /* default */
    x_default_parameter (f, parms, Qfontsize,
                                    make_number (0 /*(int)[font pointSize]*/),
                                    "fontSize", "FontSize", RES_TYPE_NUMBER);
    // Remove ' Regular', not handled by backends.
    char *fontname = xstrdup ([[font displayName] UTF8String]);
    int len = strlen (fontname);
    if (len > 8 && strcmp (fontname + len - 8, " Regular") == 0)
      fontname[len-8] = '\0';
    x_default_parameter (f, parms, Qfont,
                                 build_string (fontname),
                                 "font", "Font", RES_TYPE_STRING);
    xfree (fontname);
  }
#else
  x_default_parameter (f, parms, Qfont,
                               build_string ("Monospace"),
                               "font", "Font", RES_TYPE_STRING);
#endif
  unblock_input ();

  x_default_parameter (f, parms, Qborder_width, make_number (0),
		       "borderwidth", "BorderWidth", RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qinternal_border_width, make_number (2),
                      "internalBorderWidth", "InternalBorderWidth",
                      RES_TYPE_NUMBER);

  /* default vertical scrollbars on right on Mac */
  {
      Lisp_Object spos
#ifdef GTK3WL_IMPL_GNUSTEP
          = Qt;
#else
          = Qright;
#endif
      x_default_parameter (f, parms, Qvertical_scroll_bars, spos,
			   "verticalScrollBars", "VerticalScrollBars",
			   RES_TYPE_SYMBOL);
  }
  x_default_parameter (f, parms, Qhorizontal_scroll_bars, Qnil,
		       "horizontalScrollBars", "HorizontalScrollBars",
		       RES_TYPE_SYMBOL);
  x_default_parameter (f, parms, Qforeground_color, build_string ("Black"),
                      "foreground", "Foreground", RES_TYPE_STRING);
  x_default_parameter (f, parms, Qbackground_color, build_string ("White"),
                      "background", "Background", RES_TYPE_STRING);
  /* FIXME: not supported yet in Nextstep */
  x_default_parameter (f, parms, Qline_spacing, Qnil,
		       "lineSpacing", "LineSpacing", RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qleft_fringe, Qnil,
		       "leftFringe", "LeftFringe", RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qright_fringe, Qnil,
		       "rightFringe", "RightFringe", RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qno_special_glyphs, Qnil,
		       NULL, NULL, RES_TYPE_BOOLEAN);

  init_frame_faces (f);

  /* Read comment about this code in corresponding place in xfns.c.  */
  tem = x_get_arg (dpyinfo, parms, Qmin_width, NULL, NULL, RES_TYPE_NUMBER);
  if (NUMBERP (tem))
    store_frame_param (f, Qmin_width, tem);
  tem = x_get_arg (dpyinfo, parms, Qmin_height, NULL, NULL, RES_TYPE_NUMBER);
  if (NUMBERP (tem))
    store_frame_param (f, Qmin_height, tem);
  adjust_frame_size (f, FRAME_COLS (f) * FRAME_COLUMN_WIDTH (f),
		     FRAME_LINES (f) * FRAME_LINE_HEIGHT (f), 5, 1,
		     Qx_create_frame_1);

  tem = x_get_arg (dpyinfo, parms, Qundecorated, NULL, NULL, RES_TYPE_BOOLEAN);
  FRAME_UNDECORATED (f) = !NILP (tem) && !EQ (tem, Qunbound);
  store_frame_param (f, Qundecorated, FRAME_UNDECORATED (f) ? Qt : Qnil);

#ifdef GTK3WL_IMPL_COCOA
  tem = x_get_arg (dpyinfo, parms, Qgtk3wl_appearance, NULL, NULL, RES_TYPE_SYMBOL);
  FRAME_GTK3WL_APPEARANCE (f) = EQ (tem, Qdark)
    ? gtk3wl_appearance_vibrant_dark : gtk3wl_appearance_aqua;
  store_frame_param (f, Qgtk3wl_appearance, tem);

  tem = x_get_arg (dpyinfo, parms, Qgtk3wl_transparent_titlebar,
                   NULL, NULL, RES_TYPE_BOOLEAN);
  FRAME_GTK3WL_TRANSPARENT_TITLEBAR (f) = !NILP (tem) && !EQ (tem, Qunbound);
  store_frame_param (f, Qgtk3wl_transparent_titlebar, tem);
#endif

  parent_frame = x_get_arg (dpyinfo, parms, Qparent_frame, NULL, NULL,
			    RES_TYPE_SYMBOL);
  /* Accept parent-frame iff parent-id was not specified.  */
  if (!NILP (parent)
      || EQ (parent_frame, Qunbound)
      || NILP (parent_frame)
      || !FRAMEP (parent_frame)
      || !FRAME_LIVE_P (XFRAME (parent_frame)))
    parent_frame = Qnil;

  fset_parent_frame (f, parent_frame);
  store_frame_param (f, Qparent_frame, parent_frame);

  x_default_parameter (f, parms, Qz_group, Qnil, NULL, NULL, RES_TYPE_SYMBOL);
  x_default_parameter (f, parms, Qno_focus_on_map, Qnil,
		       NULL, NULL, RES_TYPE_BOOLEAN);
  x_default_parameter (f, parms, Qno_accept_focus, Qnil,
                       NULL, NULL, RES_TYPE_BOOLEAN);

  /* The resources controlling the menu-bar and tool-bar are
     processed specially at startup, and reflected in the mode
     variables; ignore them here.  */
  x_default_parameter (f, parms, Qmenu_bar_lines,
		       NILP (Vmenu_bar_mode)
		       ? make_number (0) : make_number (1),
		       NULL, NULL, RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qtool_bar_lines,
		       NILP (Vtool_bar_mode)
		       ? make_number (0) : make_number (1),
		       NULL, NULL, RES_TYPE_NUMBER);

  x_default_parameter (f, parms, Qbuffer_predicate, Qnil, "bufferPredicate",
                       "BufferPredicate", RES_TYPE_SYMBOL);
  x_default_parameter (f, parms, Qtitle, Qnil, "title", "Title",
                       RES_TYPE_STRING);

  parms = get_geometry_from_preferences (dpyinfo, parms);
  window_prompting = x_figure_window_size (f, parms, true, &x_width, &x_height);

  tem = x_get_arg (dpyinfo, parms, Qunsplittable, 0, 0, RES_TYPE_BOOLEAN);
  f->no_split = minibuffer_only || (!EQ (tem, Qunbound) && !EQ (tem, Qnil));

#if 0
  /* NOTE: on other terms, this is done in set_mouse_color, however this
     was not getting called under Nextstep */
  f->output_data.gtk3wl->text_cursor = [NSCursor IBeamCursor];
  f->output_data.gtk3wl->nontext_cursor = [NSCursor arrowCursor];
  f->output_data.gtk3wl->modeline_cursor = [NSCursor pointingHandCursor];
  f->output_data.gtk3wl->hand_cursor = [NSCursor pointingHandCursor];
  f->output_data.gtk3wl->hourglass_cursor = [NSCursor disappearingItemCursor];
  f->output_data.gtk3wl->horizontal_drag_cursor = [NSCursor resizeLeftRightCursor];
  f->output_data.gtk3wl->vertical_drag_cursor = [NSCursor resizeUpDownCursor];
  f->output_data.gtk3wl->left_edge_cursor = [NSCursor resizeLeftRightCursor];
  f->output_data.gtk3wl->top_left_corner_cursor = [NSCursor arrowCursor];
  f->output_data.gtk3wl->top_edge_cursor = [NSCursor resizeUpDownCursor];
  f->output_data.gtk3wl->top_right_corner_cursor = [NSCursor arrowCursor];
  f->output_data.gtk3wl->right_edge_cursor = [NSCursor resizeLeftRightCursor];
  f->output_data.gtk3wl->bottom_right_corner_cursor = [NSCursor arrowCursor];
  f->output_data.gtk3wl->bottom_edge_cursor = [NSCursor resizeUpDownCursor];
  f->output_data.gtk3wl->bottom_left_corner_cursor = [NSCursor arrowCursor];

  FRAME_DISPLAY_INFO (f)->vertical_scroll_bar_cursor
     = [NSCursor arrowCursor];
  FRAME_DISPLAY_INFO (f)->horizontal_scroll_bar_cursor
     = [NSCursor arrowCursor];
#endif
  f->output_data.gtk3wl->current_pointer = f->output_data.gtk3wl->text_cursor;

  // f->output_data.gtk3wl->in_animation = NO;

  // [[EmacsView alloc] initFrameFromEmacs: f];
  xg_create_frame_widgets(f);
  gtk3wl_set_event_handler(f);

  x_icon (f, parms);

  /* gtk3wl_display_info does not have a reference_count.  */
  f->terminal->reference_count++;

  /* It is now ok to make the frame official even if we get an error below.
     The frame needs to be on Vframe_list or making it visible won't work. */
  Vframe_list = Fcons (frame, Vframe_list);

  x_default_parameter (f, parms, Qicon_type, Qnil,
                       "bitmapIcon", "BitmapIcon", RES_TYPE_SYMBOL);

  x_default_parameter (f, parms, Qauto_raise, Qnil,
                       "autoRaise", "AutoRaiseLower", RES_TYPE_BOOLEAN);
  x_default_parameter (f, parms, Qauto_lower, Qnil,
                       "autoLower", "AutoLower", RES_TYPE_BOOLEAN);
  x_default_parameter (f, parms, Qcursor_type, Qbox,
                       "cursorType", "CursorType", RES_TYPE_SYMBOL);
  x_default_parameter (f, parms, Qscroll_bar_width, Qnil,
                       "scrollBarWidth", "ScrollBarWidth",
                       RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qscroll_bar_height, Qnil,
                       "scrollBarHeight", "ScrollBarHeight",
                       RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qalpha, Qnil,
                       "alpha", "Alpha", RES_TYPE_NUMBER);
  x_default_parameter (f, parms, Qfullscreen, Qnil,
                       "fullscreen", "Fullscreen", RES_TYPE_SYMBOL);

  /* Allow x_set_window_size, now.  */
  f->can_x_set_window_size = true;

  if (x_width > 0)
    SET_FRAME_WIDTH (f, x_width);
  if (x_height > 0)
    SET_FRAME_HEIGHT (f, x_height);

  adjust_frame_size (f, FRAME_TEXT_WIDTH (f), FRAME_TEXT_HEIGHT (f), 0, 1,
		     Qx_create_frame_2);

  if (! f->output_data.gtk3wl->explicit_parent)
    {
      Lisp_Object visibility;

      visibility = x_get_arg (dpyinfo, parms, Qvisibility, 0, 0,
                              RES_TYPE_SYMBOL);
      if (EQ (visibility, Qunbound))
	visibility = Qt;

      if (EQ (visibility, Qicon))
	x_iconify_frame (f);
      else if (! NILP (visibility))
	{
	  x_make_frame_visible (f);
	  // [[FRAME_GTK3WL_VIEW (f) window] makeKeyWindow];
	}
      else
        {
	  /* Must have been Qnil.  */
        }
    }

  if (FRAME_HAS_MINIBUF_P (f)
      && (!FRAMEP (KVAR (kb, Vdefault_minibuffer_frame))
          || !FRAME_LIVE_P (XFRAME (KVAR (kb, Vdefault_minibuffer_frame)))))
    kset_default_minibuffer_frame (kb, frame);

  /* All remaining specified parameters, which have not been "used"
     by x_get_arg and friends, now go in the misc. alist of the frame.  */
  for (tem = parms; CONSP (tem); tem = XCDR (tem))
    if (CONSP (XCAR (tem)) && !NILP (XCAR (XCAR (tem))))
      fset_param_alist (f, Fcons (XCAR (tem), f->param_alist));

  if (window_prompting & USPosition)
    x_set_offset (f, f->left_pos, f->top_pos, 1);

  /* Make sure windows on this frame appear in calls to next-window
     and similar functions.  */
  Vwindow_list = Qnil;

  return unbind_to (count, frame);
}

void
x_focus_frame (struct frame *f, bool noactivate)
{
  struct gtk3wl_display_info *dpyinfo = FRAME_DISPLAY_INFO (f);

#if 0
  if (dpyinfo->x_focus_frame != f)
    {
      EmacsView *view = FRAME_GTK3WL_VIEW (f);
      block_input ();
      [NSApp activateIgnoringOtherApps: YES];
      [[view window] makeKeyAndOrderFront: view];
      unblock_input ();
    }
#endif
}

#if 0
static int
gtk3wl_window_is_ancestor (GTK3WLWindow *win, GTK3WLWindow *candidate)
/* Test whether CANDIDATE is an ancestor window of WIN. */
{
  if (candidate == NULL)
    return 0;
  else if (win == candidate)
    return 1;
  else
    return gtk3wl_window_is_ancestor(win, [candidate parentWindow]);
}
#endif

DEFUN ("gtk3wl-frame-list-z-order", Fgtk3wl_frame_list_z_order,
       Sgtk3wl_frame_list_z_order, 0, 1, 0,
       doc: /* Return list of Emacs' frames, in Z (stacking) order.
If TERMINAL is non-nil and specifies a live frame, return the child
frames of that frame in Z (stacking) order.

Frames are listed from topmost (first) to bottommost (last).  */)
  (Lisp_Object terminal)
{
  Lisp_Object frames = Qnil;
#if 0
  GTK3WLWindow *parent = nil;

  if (FRAMEP (terminal) && FRAME_LIVE_P (XFRAME (terminal)))
    parent = [FRAME_GTK3WL_VIEW (XFRAME (terminal)) window];

  for (GTK3WLWindow *win in [[NSApp orderedWindows] reverseObjectEnumerator])
    {
      Lisp_Object frame;

      /* Check against [win parentWindow] so that it doesn't match itself. */
      if (parent == nil || gtk3wl_window_is_ancestor (parent, [win parentWindow]))
        {
          XSETFRAME (frame, ((EmacsView *)[win delegate])->emacsframe);
          frames = Fcons(frame, frames);
        }
    }
#endif

  return frames;
}

DEFUN ("gtk3wl-frame-restack", Fgtk3wl_frame_restack, Sgtk3wl_frame_restack, 2, 3, 0,
       doc: /* Restack FRAME1 below FRAME2.
This means that if both frames are visible and the display areas of
these frames overlap, FRAME2 (partially) obscures FRAME1.  If optional
third argument ABOVE is non-nil, restack FRAME1 above FRAME2.  This
means that if both frames are visible and the display areas of these
frames overlap, FRAME1 (partially) obscures FRAME2.

Some window managers may refuse to restack windows.  */)
     (Lisp_Object frame1, Lisp_Object frame2, Lisp_Object above)
{
  struct frame *f1 = decode_live_frame (frame1);
  struct frame *f2 = decode_live_frame (frame2);

  if (FRAME_GTK3WL_VIEW (f1) && FRAME_GTK3WL_VIEW (f2))
    {
#if 0
      GTK3WLWindow *window = [FRAME_GTK3WL_VIEW (f1) window];
      NSInteger window2 = [[FRAME_GTK3WL_VIEW (f2) window] windowNumber];
      GTK3WLWindowOrderingMode flag = NILP (above) ? GTK3WLWindowBelow : GTK3WLWindowAbove;

      [window orderWindow: flag
               relativeTo: window2];

#endif
      return Qt;
    }
  else
    {
      error ("Cannot restack frames");
      return Qnil;
    }
}

#if 0
DEFUN ("gtk3wl-popup-font-panel", Fgtk3wl_popup_font_panel, Sgtk3wl_popup_font_panel,
       0, 1, "",
       doc: /* Pop up the font panel. */)
     (Lisp_Object frame)
{
  struct frame *f = decode_window_system_frame (frame);
  id fm = [NSFontManager sharedFontManager];
  struct font *font = f->output_data.gtk3wl->font;
  NSFont *nsfont;
#ifdef GTK3WL_IMPL_GNUSTEP
  nsfont = ((struct nsfont_info *)font)->nsfont;
#endif
#ifdef GTK3WL_IMPL_COCOA
  nsfont = (NSFont *) macfont_get_nsctfont (font);
#endif
  [fm setSelectedFont: nsfont isMultiple: NO];
  [fm orderFrontFontPanel: NSApp];
  return Qnil;
}
#endif


#if 0
DEFUN ("gtk3wl-popup-color-panel", Fgtk3wl_popup_color_panel, Sgtk3wl_popup_color_panel,
       0, 1, "",
       doc: /* Pop up the color panel.  */)
     (Lisp_Object frame)
{
  check_window_system (NULL);
  [NSApp orderFrontColorPanel: NSApp];
  return Qnil;
}
#endif

#if 0
static struct
{
  id panel;
  BOOL ret;
#ifdef GTK3WL_IMPL_GNUSTEP
  NSString *dirS, *initS;
  BOOL no_types;
#endif
} gtk3wl_fd_data;
#endif

void
gtk3wl_run_file_dialog (void)
{
#if 0
  if (gtk3wl_fd_data.panel == nil) return;
#ifdef GTK3WL_IMPL_COCOA
  gtk3wl_fd_data.ret = [gtk3wl_fd_data.panel runModal];
#else
  if (gtk3wl_fd_data.no_types)
    {
      gtk3wl_fd_data.ret = [gtk3wl_fd_data.panel
                           runModalForDirectory: gtk3wl_fd_data.dirS
                           file: gtk3wl_fd_data.initS];
    }
  else
    {
      gtk3wl_fd_data.ret = [gtk3wl_fd_data.panel
                           runModalForDirectory: gtk3wl_fd_data.dirS
                           file: gtk3wl_fd_data.initS
                           types: nil];
    }
#endif
  gtk3wl_fd_data.panel = nil;
#endif
}

#if 0

#ifdef GTK3WL_IMPL_COCOA
#if MAC_OS_X_VERSION_MAX_ALLOWED > 1090
#define MODAL_OK_RESPONSE NSModalResponseOK
#endif
#endif
#ifndef MODAL_OK_RESPONSE
#define MODAL_OK_RESPONSE NSOKButton
#endif

DEFUN ("gtk3wl-read-file-name", Fgtk3wl_read_file_name, Sgtk3wl_read_file_name, 1, 5, 0,
       doc: /* Use a graphical panel to read a file name, using prompt PROMPT.
Optional arg DIR, if non-nil, supplies a default directory.
Optional arg MUSTMATCH, if non-nil, means the returned file or
directory must exist.
Optional arg INIT, if non-nil, provides a default file name to use.
Optional arg DIR_ONLY_P, if non-nil, means choose only directories.  */)
  (Lisp_Object prompt, Lisp_Object dir, Lisp_Object mustmatch,
   Lisp_Object init, Lisp_Object dir_only_p)
{
  static id fileDelegate = nil;
  BOOL isSave = NILP (mustmatch) && NILP (dir_only_p);
  id panel;
  Lisp_Object fname = Qnil;

  NSString *promptS = NILP (prompt) || !STRINGP (prompt) ? nil :
    [NSString stringWithUTF8String: SSDATA (prompt)];
  NSString *dirS = NILP (dir) || !STRINGP (dir) ?
    [NSString stringWithUTF8String: SSDATA (BVAR (current_buffer, directory))] :
    [NSString stringWithUTF8String: SSDATA (dir)];
  NSString *initS = NILP (init) || !STRINGP (init) ? nil :
    [NSString stringWithUTF8String: SSDATA (init)];
  NSEvent *nxev;

  check_window_system (NULL);

  if (fileDelegate == nil)
    fileDelegate = [EmacsFileDelegate new];

  [NSCursor setHiddenUntilMouseMoves: NO];

  if ([dirS characterAtIndex: 0] == '~')
    dirS = [dirS stringByExpandingTildeInPath];

  panel = isSave ?
    (id)[EmacsSavePanel savePanel] : (id)[EmacsOpenPanel openPanel];

  [panel setTitle: promptS];

  [panel setAllowsOtherFileTypes: YES];
  [panel setTreatsFilePackagesAsDirectories: YES];
  [panel setDelegate: fileDelegate];

  if (! NILP (dir_only_p))
    {
      [panel setCanChooseDirectories: YES];
      [panel setCanChooseFiles: NO];
    }
  else if (! isSave)
    {
      /* This is not quite what the documentation says, but it is compatible
         with the Gtk+ code.  Also, the menu entry says "Open File...".  */
      [panel setCanChooseDirectories: NO];
      [panel setCanChooseFiles: YES];
    }

  block_input ();
  gtk3wl_fd_data.panel = panel;
  gtk3wl_fd_data.ret = NO;
#ifdef GTK3WL_IMPL_COCOA
  if (! NILP (mustmatch) || ! NILP (dir_only_p))
    [panel setAllowedFileTypes: nil];
  if (dirS) [panel setDirectoryURL: [NSURL fileURLWithPath: dirS]];
  if (initS && NILP (Ffile_directory_p (init)))
    [panel setNameFieldStringValue: [initS lastPathComponent]];
  else
    [panel setNameFieldStringValue: @""];

#else
  gtk3wl_fd_data.no_types = NILP (mustmatch) && NILP (dir_only_p);
  gtk3wl_fd_data.dirS = dirS;
  gtk3wl_fd_data.initS = initS;
#endif

  /* runModalForDirectory/runModal restarts the main event loop when done,
     so we must start an event loop and then pop up the file dialog.
     The file dialog may pop up a confirm dialog after Ok has been pressed,
     so we can not simply pop down on the Ok/Cancel press.
   */
  nxev = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                            location: NSMakePoint (0, 0)
                       modifierFlags: 0
                           timestamp: 0
                        windowNumber: [[NSApp mainWindow] windowNumber]
                             context: [NSApp context]
                             subtype: 0
                               data1: 0
                               data2: NSAPP_DATA2_RUNFILEDIALOG];

  [NSApp postEvent: nxev atStart: NO];
  while (gtk3wl_fd_data.panel != nil)
    [NSApp run];

  if (gtk3wl_fd_data.ret == MODAL_OK_RESPONSE)
    {
      NSString *str = gtk3wl_filename_from_panel (panel);
      if (! str) str = gtk3wl_directory_from_panel (panel);
      if (str) fname = build_string ([str UTF8String]);
    }

  [[FRAME_GTK3WL_VIEW (SELECTED_FRAME ()) window] makeKeyWindow];
  unblock_input ();

  return fname;
}

#endif

const char *
gtk3wl_get_defaults_value (const char *key)
{
#if 0
  NSObject *obj = [[NSUserDefaults standardUserDefaults]
                    objectForKey: [NSString stringWithUTF8String: key]];

  if (!obj) return NULL;

  return [[NSString stringWithFormat: @"%@", obj] UTF8String];
#endif
  return NULL;
}


DEFUN ("gtk3wl-get-resource", Fgtk3wl_get_resource, Sgtk3wl_get_resource, 2, 2, 0,
       doc: /* Return the value of the property NAME of OWNER from the defaults database.
If OWNER is nil, Emacs is assumed.  */)
     (Lisp_Object owner, Lisp_Object name)
{
  const char *value;

  check_window_system (NULL);
  if (NILP (owner))
    owner = build_string(gtk3wl_app_name);
  CHECK_STRING (name);

  value = gtk3wl_get_defaults_value (SSDATA (name));

  if (value)
    return build_string (value);
  return Qnil;
}


DEFUN ("gtk3wl-set-resource", Fgtk3wl_set_resource, Sgtk3wl_set_resource, 3, 3, 0,
       doc: /* Set property NAME of OWNER to VALUE, from the defaults database.
If OWNER is nil, Emacs is assumed.
If VALUE is nil, the default is removed.  */)
     (Lisp_Object owner, Lisp_Object name, Lisp_Object value)
{
  check_window_system (NULL);
  if (NILP (owner))
    owner = build_string (gtk3wl_app_name);
  CHECK_STRING (name);
#if 0
  if (NILP (value))
    {
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:
                         [NSString stringWithUTF8String: SSDATA (name)]];
    }
  else
    {
      CHECK_STRING (value);
      [[NSUserDefaults standardUserDefaults] setObject:
                [NSString stringWithUTF8String: SSDATA (value)]
                                        forKey: [NSString stringWithUTF8String:
                                                         SSDATA (name)]];
    }
#endif

  return Qnil;
}


DEFUN ("x-server-max-request-size", Fx_server_max_request_size,
       Sx_server_max_request_size,
       0, 1, 0,
       doc: /* This function is a no-op.  It is only present for completeness.  */)
     (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  /* This function has no real equivalent under NeXTstep.  Return nil to
     indicate this. */
  return Qnil;
}


DEFUN ("x-server-vendor", Fx_server_vendor, Sx_server_vendor, 0, 1, 0,
       doc: /* Return the "vendor ID" string of Nextstep display server TERMINAL.
\(Labeling every distributor as a "vendor" embodies the false assumption
that operating systems cannot be developed and distributed noncommercially.)
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
#ifdef GTK3WL_IMPL_GNUSTEP
  return build_string ("GNU");
#else
  return build_string ("Apple");
#endif
}


DEFUN ("x-server-version", Fx_server_version, Sx_server_version, 0, 1, 0,
       doc: /* Return the version numbers of the server of display TERMINAL.
The value is a list of three integers: the major and minor
version numbers of the X Protocol in use, and the distributor-specific release
number.  See also the function `x-server-vendor'.

The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  /*NOTE: it is unclear what would best correspond with "protocol";
          we return 10.3, meaning Panther, since this is roughly the
          level that GNUstep's APIs correspond to.
          The last number is where we distinguish between the Apple
          and GNUstep implementations ("distributor-specific release
          number") and give int'ized versions of major.minor. */
  return list3i (10, 3, gtk3wl_appkit_version_int ());
}


DEFUN ("x-display-screens", Fx_display_screens, Sx_display_screens, 0, 1, 0,
       doc: /* Return the number of screens on Nextstep display server TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

Note: "screen" here is not in Nextstep terminology but in X11's.  For
the number of physical monitors, use `(length
\(display-monitor-attributes-list TERMINAL))' instead.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  return make_number (1);
}


DEFUN ("x-display-mm-height", Fx_display_mm_height, Sx_display_mm_height, 0, 1, 0,
       doc: /* Return the height in millimeters of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

On \"multi-monitor\" setups this refers to the height in millimeters for
all physical monitors associated with TERMINAL.  To get information
for each physical monitor, use `display-monitor-attributes-list'.  */)
  (Lisp_Object terminal)
{
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (terminal);

  return make_number (x_display_pixel_height (dpyinfo) / (92.0/25.4));
}


DEFUN ("x-display-mm-width", Fx_display_mm_width, Sx_display_mm_width, 0, 1, 0,
       doc: /* Return the width in millimeters of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

On \"multi-monitor\" setups this refers to the width in millimeters for
all physical monitors associated with TERMINAL.  To get information
for each physical monitor, use `display-monitor-attributes-list'.  */)
  (Lisp_Object terminal)
{
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (terminal);

  return make_number (x_display_pixel_width (dpyinfo) / (92.0/25.4));
}


DEFUN ("x-display-backing-store", Fx_display_backing_store,
       Sx_display_backing_store, 0, 1, 0,
       doc: /* Return an indication of whether the Nextstep display TERMINAL does backing store.
The value may be `buffered', `retained', or `non-retained'.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
#if 0
  switch ([gtk3wl_get_window (terminal) backingType])
    {
    case NSBackingStoreBuffered:
      return intern ("buffered");
    case NSBackingStoreRetained:
      return intern ("retained");
    case NSBackingStoreNonretained:
      return intern ("non-retained");
    default:
      error ("Strange value for backingType parameter of frame");
    }
#endif
  return Qnil;  /* not reached, shut compiler up */
}


DEFUN ("x-display-visual-class", Fx_display_visual_class,
       Sx_display_visual_class, 0, 1, 0,
       doc: /* Return the visual class of the Nextstep display TERMINAL.
The value is one of the symbols `static-gray', `gray-scale',
`static-color', `pseudo-color', `true-color', or `direct-color'.

The optional argument TERMINAL specifies which display to ask about.
TERMINAL should a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
#if 0
  GTK3WLWindowDepth depth;

  check_gtk3wl_display_info (terminal);
  depth = [[[NSScreen screens] objectAtIndex:0] depth];

  if ( depth == NSBestDepth (NSCalibratedWhiteColorSpace, 2, 2, YES, NULL))
    return intern ("static-gray");
  else if (depth == NSBestDepth (NSCalibratedWhiteColorSpace, 8, 8, YES, NULL))
    return intern ("gray-scale");
  else if ( depth == NSBestDepth (NSCalibratedRGBColorSpace, 8, 8, YES, NULL))
    return intern ("pseudo-color");
  else if ( depth == NSBestDepth (NSCalibratedRGBColorSpace, 4, 12, NO, NULL))
    return intern ("true-color");
  else if ( depth == NSBestDepth (NSCalibratedRGBColorSpace, 8, 24, NO, NULL))
    return intern ("direct-color");
  else
    /* color mgmt as far as we do it is really handled by Nextstep itself anyway */
    return intern ("direct-color");
#endif
  return intern ("direct-color");
}


DEFUN ("x-display-save-under", Fx_display_save_under,
       Sx_display_save_under, 0, 1, 0,
       doc: /* Return t if TERMINAL supports the save-under feature.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
#if 0
  switch ([gtk3wl_get_window (terminal) backingType])
    {
    case NSBackingStoreBuffered:
      return Qt;

    case NSBackingStoreRetained:
    case NSBackingStoreNonretained:
      return Qnil;

    default:
      error ("Strange value for backingType parameter of frame");
    }
#endif
  return Qnil;  /* not reached, shut compiler up */
}


DEFUN ("x-open-connection", Fx_open_connection, Sx_open_connection,
       1, 3, 0,
       doc: /* Open a connection to a display server.
DISPLAY is the name of the display to connect to.
Optional second arg XRM-STRING is a string of resources in xrdb format.
If the optional third arg MUST-SUCCEED is non-nil,
terminate Emacs if we can't open the connection.
\(In the Nextstep version, the last two arguments are currently ignored.)  */)
     (Lisp_Object display, Lisp_Object resource_string, Lisp_Object must_succeed)
{
  struct gtk3wl_display_info *dpyinfo;

  CHECK_STRING (display);

  // nxatoms_of_gtk3wlselect ();
  dpyinfo = gtk3wl_term_init (display);
  if (dpyinfo == 0)
    {
      if (!NILP (must_succeed))
        fatal ("Display on %s not responding.\n",
               SSDATA (display));
      else
        error ("Display on %s not responding.\n",
               SSDATA (display));
    }

  return Qnil;
}


DEFUN ("x-close-connection", Fx_close_connection, Sx_close_connection,
       1, 1, 0,
       doc: /* Close the connection to TERMINAL's Nextstep display server.
For TERMINAL, specify a terminal object, a frame or a display name (a
string).  If TERMINAL is nil, that stands for the selected frame's
terminal.  */)
     (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  // [NSApp terminate: NSApp];
  return Qnil;
}


DEFUN ("x-display-list", Fx_display_list, Sx_display_list, 0, 0, 0,
       doc: /* Return the list of display names that Emacs has connections to.  */)
     (void)
{
  Lisp_Object result = Qnil;
  struct gtk3wl_display_info *ndi;

  for (ndi = x_display_list; ndi; ndi = ndi->next)
    result = Fcons (XCAR (ndi->name_list_element), result);

  return result;
}


DEFUN ("gtk3wl-hide-others", Fgtk3wl_hide_others, Sgtk3wl_hide_others,
       0, 0, 0,
       doc: /* Hides all applications other than Emacs.  */)
     (void)
{
  check_window_system (NULL);
  // [NSApp hideOtherApplications: NSApp];
  return Qnil;
}

DEFUN ("gtk3wl-hide-emacs", Fgtk3wl_hide_emacs, Sgtk3wl_hide_emacs,
       1, 1, 0,
       doc: /* If ON is non-nil, the entire Emacs application is hidden.
Otherwise if Emacs is hidden, it is unhidden.
If ON is equal to `activate', Emacs is unhidden and becomes
the active application.  */)
     (Lisp_Object on)
{
  check_window_system (NULL);
#if 0
  if (EQ (on, intern ("activate")))
    {
      [NSApp unhide: NSApp];
      [NSApp activateIgnoringOtherApps: YES];
    }
  else if (NILP (on))
    [NSApp unhide: NSApp];
  else
    [NSApp hide: NSApp];
#endif
  return Qnil;
}


DEFUN ("gtk3wl-emacs-info-panel", Fgtk3wl_emacs_info_panel, Sgtk3wl_emacs_info_panel,
       0, 0, 0,
       doc: /* Shows the 'Info' or 'About' panel for Emacs.  */)
     (void)
{
  check_window_system (NULL);
#if 0
  [NSApp orderFrontStandardAboutPanel: nil];
#endif
  return Qnil;
}


DEFUN ("gtk3wl-font-name", Fgtk3wl_font_name, Sgtk3wl_font_name, 1, 1, 0,
       doc: /* Determine font PostScript or family name for font NAME.
NAME should be a string containing either the font name or an XLFD
font descriptor.  If string contains `fontset' and not
`fontset-startup', it is left alone. */)
     (Lisp_Object name)
{
  char *nm;
  CHECK_STRING (name);
  nm = SSDATA (name);

  if (nm[0] != '-')
    return name;
  if (strstr (nm, "fontset") && !strstr (nm, "fontset-startup"))
    return name;

  char *str = gtk3wl_xlfd_to_fontname (SSDATA (name));
  name = build_string (str);
  xfree(str);
  return name;
}


DEFUN ("gtk3wl-list-services", Fgtk3wl_list_services, Sgtk3wl_list_services, 0, 0, 0,
       doc: /* List available Nextstep services by querying NSApp.  */)
     (void)
{
#ifdef GTK3WL_IMPL_COCOA
  /* You can't get services like this in 10.6+.  */
  return Qnil;
#else
  Lisp_Object ret = Qnil;
#if 0
  NSMenu *svcs;

  check_window_system (NULL);
  svcs = [[NSMenu alloc] initWithTitle: @"Services"];
  [NSApp setServicesMenu: svcs];
  [NSApp registerServicesMenuSendTypes: gtk3wl_send_types
                           returnTypes: gtk3wl_return_types];

  [svcs setAutoenablesItems: NO];

  ret = interpret_services_menu (svcs, Qnil, ret);
#endif
  return ret;
#endif
}


DEFUN ("gtk3wl-perform-service", Fgtk3wl_perform_service, Sgtk3wl_perform_service,
       2, 2, 0,
       doc: /* Perform Nextstep SERVICE on SEND.
SEND should be either a string or nil.
The return value is the result of the service, as string, or nil if
there was no result.  */)
     (Lisp_Object service, Lisp_Object send)
{
#if 0
  id pb;
  NSString *svcName;
  char *utfStr;

  CHECK_STRING (service);
  check_window_system (NULL);

  utfStr = SSDATA (service);
  svcName = [NSString stringWithUTF8String: utfStr];

  pb =[NSPasteboard pasteboardWithUniqueName];
  gtk3wl_string_to_pasteboard (pb, send);

  if (NSPerformService (svcName, pb) == NO)
    Fsignal (Qquit, list1 (build_string ("service not available")));

  if ([[pb types] count] == 0)
    return build_string ("");
  return gtk3wl_string_from_pasteboard (pb);
#endif
  return build_string ("");
}


/* ==========================================================================

    Miscellaneous functions not called through hooks

   ========================================================================== */

/* called from frame.c */
struct gtk3wl_display_info *
check_x_display_info (Lisp_Object frame)
{
  return check_gtk3wl_display_info (frame);
}


void
x_set_scroll_bar_default_width (struct frame *f)
{
#if 0
  int wid = FRAME_COLUMN_WIDTH (f);
  FRAME_CONFIG_SCROLL_BAR_WIDTH (f) = GTK3WL_SCROLL_BAR_WIDTH_DEFAULT;
  FRAME_CONFIG_SCROLL_BAR_COLS (f) = (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) +
                                      wid - 1) / wid;
#endif
}

void
x_set_scroll_bar_default_height (struct frame *f)
{
#if 0
  int height = FRAME_LINE_HEIGHT (f);
  FRAME_CONFIG_SCROLL_BAR_HEIGHT (f) = GTK3WL_SCROLL_BAR_WIDTH_DEFAULT;
  FRAME_CONFIG_SCROLL_BAR_LINES (f) = (FRAME_CONFIG_SCROLL_BAR_HEIGHT (f) +
				       height - 1) / height;
#endif
}

/* terms impl this instead of x-get-resource directly */
char *
x_get_string_resource (XrmDatabase rdb, const char *name, const char *class)
{
  /* remove appname prefix; TODO: allow for !="Emacs" */
  const char *res, *toCheck = class + (!strncmp (class, "Emacs.", 6) ? 6 : 0);

  check_window_system (NULL);

  if (inhibit_x_resources)
    /* --quick was passed, so this is a no-op.  */
    return NULL;

  res = gtk3wl_get_defaults_value (toCheck);
  return (char *) (!res ? NULL
		   : !c_strncasecmp (res, "YES", 3) ? "true"
		   : !c_strncasecmp (res, "NO", 2) ? "false"
		   : res);
}


Lisp_Object
x_get_focus_frame (struct frame *frame)
{
  struct gtk3wl_display_info *dpyinfo = FRAME_DISPLAY_INFO (frame);
  Lisp_Object nsfocus;

  if (!dpyinfo->x_focus_frame)
    return Qnil;

  XSETFRAME (nsfocus, dpyinfo->x_focus_frame);
  return nsfocus;
}

/* ==========================================================================

    Lisp definitions that, for whatever reason, we can't alias as 'ns-XXX'.

   ========================================================================== */


DEFUN ("xw-color-defined-p", Fxw_color_defined_p, Sxw_color_defined_p, 1, 2, 0,
       doc: /* Internal function called by `color-defined-p', which see.
\(Note that the Nextstep version of this function ignores FRAME.)  */)
     (Lisp_Object color, Lisp_Object frame)
{
  XColor col;
  return gtk3wl_lisp_to_color (color, &col) ? Qnil : Qt;
}


DEFUN ("xw-color-values", Fxw_color_values, Sxw_color_values, 1, 2, 0,
       doc: /* Internal function called by `color-values', which see.  */)
     (Lisp_Object color, Lisp_Object frame)
{
  XColor col;

  CHECK_STRING (color);

  block_input ();

  if (gtk3wl_lisp_to_color (color, &col))
    {
      unblock_input ();
      return Qnil;
    }

  unblock_input ();

  return list3i (col.red, col.green, col.blue);
}


DEFUN ("xw-display-color-p", Fxw_display_color_p, Sxw_display_color_p, 0, 1, 0,
       doc: /* Internal function called by `display-color-p', which see.  */)
     (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  return Qt;
}


DEFUN ("x-display-grayscale-p", Fx_display_grayscale_p, Sx_display_grayscale_p,
       0, 1, 0,
       doc: /* Return t if the Nextstep display supports shades of gray.
Note that color displays do support shades of gray.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  return Qnil;
}


DEFUN ("x-display-pixel-width", Fx_display_pixel_width, Sx_display_pixel_width,
       0, 1, 0,
       doc: /* Return the width in pixels of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

On \"multi-monitor\" setups this refers to the pixel width for all
physical monitors associated with TERMINAL.  To get information for
each physical monitor, use `display-monitor-attributes-list'.  */)
  (Lisp_Object terminal)
{
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (terminal);

  return make_number (x_display_pixel_width (dpyinfo));
}


DEFUN ("x-display-pixel-height", Fx_display_pixel_height,
       Sx_display_pixel_height, 0, 1, 0,
       doc: /* Return the height in pixels of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

On \"multi-monitor\" setups this refers to the pixel height for all
physical monitors associated with TERMINAL.  To get information for
each physical monitor, use `display-monitor-attributes-list'.  */)
  (Lisp_Object terminal)
{
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (terminal);

  return make_number (x_display_pixel_height (dpyinfo));
}

static Lisp_Object
gtk3wl_make_monitor_attribute_list (struct MonitorInfo *monitors,
                                int n_monitors,
                                int primary_monitor,
                                const char *source)
{
#if 0
  Lisp_Object monitor_frames = Fmake_vector (make_number (n_monitors), Qnil);
  Lisp_Object frame, rest;
  NSArray *screens = [NSScreen screens];
  int i;

  FOR_EACH_FRAME (rest, frame)
    {
      struct frame *f = XFRAME (frame);

      if (FRAME_GTK3WL_P (f))
	{
          NSView *view = FRAME_GTK3WL_VIEW (f);
          NSScreen *screen = [[view window] screen];
          NSUInteger k;

          i = -1;
          for (k = 0; i == -1 && k < [screens count]; ++k)
            {
              if ([screens objectAtIndex: k] == screen)
                i = (int)k;
            }

          if (i > -1)
            ASET (monitor_frames, i, Fcons (frame, AREF (monitor_frames, i)));
	}
    }

  return make_monitor_attribute_list (monitors, n_monitors, primary_monitor,
                                      monitor_frames, source);
#endif
  return Qnil;
}

DEFUN ("gtk3wl-display-monitor-attributes-list",
       Fgtk3wl_display_monitor_attributes_list,
       Sgtk3wl_display_monitor_attributes_list,
       0, 1, 0,
       doc: /* Return a list of physical monitor attributes on the X display TERMINAL.

The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.

In addition to the standard attribute keys listed in
`display-monitor-attributes-list', the following keys are contained in
the attributes:

 source -- String describing the source from which multi-monitor
	   information is obtained, \"NS\" is always the source."

Internal use only, use `display-monitor-attributes-list' instead.  */)
  (Lisp_Object terminal)
{
#if 0
  struct terminal *term = decode_live_terminal (terminal);
  NSArray *screens;
  NSUInteger i, n_monitors;
  struct MonitorInfo *monitors;
  Lisp_Object attributes_list = Qnil;
  CGFloat primary_display_height = 0;

  if (term->type != output_ns)
    return Qnil;

  screens = [NSScreen screens];
  n_monitors = [screens count];
  if (n_monitors == 0)
    return Qnil;

  monitors = xzalloc (n_monitors * sizeof *monitors);

  for (i = 0; i < [screens count]; ++i)
    {
      NSScreen *s = [screens objectAtIndex:i];
      struct MonitorInfo *m = &monitors[i];
      NSRect fr = [s frame];
      NSRect vfr = [s visibleFrame];
      short y, vy;

#ifdef GTK3WL_IMPL_COCOA
      NSDictionary *dict = [s deviceDescription];
      NSNumber *nid = [dict objectForKey:@"NSScreenNumber"];
      CGDirectDisplayID did = [nid unsignedIntValue];
#endif
      if (i == 0)
        {
          primary_display_height = fr.size.height;
          y = (short) fr.origin.y;
          vy = (short) vfr.origin.y;
        }
      else
        {
          // Flip y coordinate as NS has y starting from the bottom.
          y = (short) (primary_display_height - fr.size.height - fr.origin.y);
          vy = (short) (primary_display_height -
                        vfr.size.height - vfr.origin.y);
        }

      m->geom.x = (short) fr.origin.x;
      m->geom.y = y;
      m->geom.width = (unsigned short) fr.size.width;
      m->geom.height = (unsigned short) fr.size.height;

      m->work.x = (short) vfr.origin.x;
      // y is flipped on NS, so vy - y are pixels missing at the bottom,
      // and fr.size.height - vfr.size.height are pixels missing in total.
      // Pixels missing at top are
      // fr.size.height - vfr.size.height - vy + y.
      // work.y is then pixels missing at top + y.
      m->work.y = (short) (fr.size.height - vfr.size.height) - vy + y + y;
      m->work.width = (unsigned short) vfr.size.width;
      m->work.height = (unsigned short) vfr.size.height;

#ifdef GTK3WL_IMPL_COCOA
      m->name = gtk3wl_screen_name (did);

      {
        CGSize mms = CGDisplayScreenSize (did);
        m->mm_width = (int) mms.width;
        m->mm_height = (int) mms.height;
      }

#else
      // Assume 92 dpi as x-display-mm-height/x-display-mm-width does.
      m->mm_width = (int) (25.4 * fr.size.width / 92.0);
      m->mm_height = (int) (25.4 * fr.size.height / 92.0);
#endif
    }

  // Primary monitor is always first for NS.
  attributes_list = gtk3wl_make_monitor_attribute_list (monitors, n_monitors,
                                                    0, "NS");

  free_monitors (monitors, n_monitors);
  return attributes_list;
#endif
  return Qnil;
}


DEFUN ("x-display-planes", Fx_display_planes, Sx_display_planes,
       0, 1, 0,
       doc: /* Return the number of bitplanes of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  check_gtk3wl_display_info (terminal);
  return make_number(32);
}


DEFUN ("x-display-color-cells", Fx_display_color_cells, Sx_display_color_cells,
       0, 1, 0,
       doc: /* Returns the number of color cells of the Nextstep display TERMINAL.
The optional argument TERMINAL specifies which display to ask about.
TERMINAL should be a terminal object, a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display.  */)
  (Lisp_Object terminal)
{
  struct gtk3wl_display_info *dpyinfo = check_gtk3wl_display_info (terminal);
  /* We force 24+ bit depths to 24-bit to prevent an overflow.  */
  return make_number (1 << min (dpyinfo->n_planes, 24));
}


/* Unused dummy def needed for compatibility. */
Lisp_Object tip_frame;

#if 0
/* TODO: move to xdisp or similar */
static void
compute_tip_xy (struct frame *f,
                Lisp_Object parms,
                Lisp_Object dx,
                Lisp_Object dy,
                int width,
                int height,
                int *root_x,
                int *root_y)
{
  Lisp_Object left, top, right, bottom;
  NSPoint pt;
  NSScreen *screen;

  /* Start with user-specified or mouse position.  */
  left = Fcdr (Fassq (Qleft, parms));
  top = Fcdr (Fassq (Qtop, parms));
  right = Fcdr (Fassq (Qright, parms));
  bottom = Fcdr (Fassq (Qbottom, parms));

  if ((!INTEGERP (left) && !INTEGERP (right))
      || (!INTEGERP (top) && !INTEGERP (bottom)))
    pt = [NSEvent mouseLocation];
  else
    {
      /* Absolute coordinates.  */
      pt.x = INTEGERP (left) ? XINT (left) : XINT (right);
      pt.y = (x_display_pixel_height (FRAME_DISPLAY_INFO (f))
	      - (INTEGERP (top) ? XINT (top) : XINT (bottom))
	      - height);
    }

  /* Find the screen that pt is on. */
  for (screen in [NSScreen screens])
    if (pt.x >= screen.frame.origin.x
        && pt.x < screen.frame.origin.x + screen.frame.size.width
        && pt.y >= screen.frame.origin.y
        && pt.y < screen.frame.origin.y + screen.frame.size.height)
      break;

  /* We could use this instead of the if above:

         if (CGRectContainsPoint ([screen frame], pt))

     which would be neater, but it causes problems building on old
     versions of macOS and in GNUstep. */

  /* Ensure in bounds.  (Note, screen origin = lower left.) */
  if (INTEGERP (left) || INTEGERP (right))
    *root_x = pt.x;
  else if (pt.x + XINT (dx) <= screen.frame.origin.x)
    *root_x = screen.frame.origin.x; /* Can happen for negative dx */
  else if (pt.x + XINT (dx) + width
	   <= screen.frame.origin.x + screen.frame.size.width)
    /* It fits to the right of the pointer.  */
    *root_x = pt.x + XINT (dx);
  else if (width + XINT (dx) <= pt.x)
    /* It fits to the left of the pointer.  */
    *root_x = pt.x - width - XINT (dx);
  else
    /* Put it left justified on the screen -- it ought to fit that way.  */
    *root_x = screen.frame.origin.x;

  if (INTEGERP (top) || INTEGERP (bottom))
    *root_y = pt.y;
  else if (pt.y - XINT (dy) - height >= screen.frame.origin.y)
    /* It fits below the pointer.  */
    *root_y = pt.y - height - XINT (dy);
  else if (pt.y + XINT (dy) + height
	   <= screen.frame.origin.y + screen.frame.size.height)
    /* It fits above the pointer */
      *root_y = pt.y + XINT (dy);
  else
    /* Put it on the top.  */
    *root_y = screen.frame.origin.y + screen.frame.size.height - height;
}
#endif


DEFUN ("x-show-tip", Fx_show_tip, Sx_show_tip, 1, 6, 0,
       doc: /* Show STRING in a \"tooltip\" window on frame FRAME.
A tooltip window is a small window displaying a string.

This is an internal function; Lisp code should call `tooltip-show'.

FRAME nil or omitted means use the selected frame.

PARMS is an optional list of frame parameters which can be used to
change the tooltip's appearance.

Automatically hide the tooltip after TIMEOUT seconds.  TIMEOUT nil
means use the default timeout of 5 seconds.

If the list of frame parameters PARMS contains a `left' parameter,
display the tooltip at that x-position.  If the list of frame parameters
PARMS contains no `left' but a `right' parameter, display the tooltip
right-adjusted at that x-position. Otherwise display it at the
x-position of the mouse, with offset DX added (default is 5 if DX isn't
specified).

Likewise for the y-position: If a `top' frame parameter is specified, it
determines the position of the upper edge of the tooltip window.  If a
`bottom' parameter but no `top' frame parameter is specified, it
determines the position of the lower edge of the tooltip window.
Otherwise display the tooltip window at the y-position of the mouse,
with offset DY added (default is -10).

A tooltip's maximum size is specified by `x-max-tooltip-size'.
Text larger than the specified size is clipped.  */)
     (Lisp_Object string, Lisp_Object frame, Lisp_Object parms, Lisp_Object timeout, Lisp_Object dx, Lisp_Object dy)
{
#if 0
  int root_x, root_y;
  ptrdiff_t count = SPECPDL_INDEX ();
  struct frame *f;
  char *str;
  NSSize size;

  specbind (Qinhibit_redisplay, Qt);

  CHECK_STRING (string);
  str = SSDATA (string);
  f = decode_window_system_frame (frame);
  if (NILP (timeout))
    timeout = make_number (5);
  else
    CHECK_NATNUM (timeout);

  if (NILP (dx))
    dx = make_number (5);
  else
    CHECK_NUMBER (dx);

  if (NILP (dy))
    dy = make_number (-10);
  else
    CHECK_NUMBER (dy);

  block_input ();
  if (gtk3wl_tooltip == nil)
    gtk3wl_tooltip = [[EmacsTooltip alloc] init];
  else
    Fx_hide_tip ();

  [gtk3wl_tooltip setText: str];
  size = [gtk3wl_tooltip frame].size;

  /* Move the tooltip window where the mouse pointer is.  Resize and
     show it.  */
  compute_tip_xy (f, parms, dx, dy, (int)size.width, (int)size.height,
		  &root_x, &root_y);

  [gtk3wl_tooltip showAtX: root_x Y: root_y for: XINT (timeout)];
  unblock_input ();

  return unbind_to (count, Qnil);
#endif
  return Qnil;
}


DEFUN ("x-hide-tip", Fx_hide_tip, Sx_hide_tip, 0, 0, 0,
       doc: /* Hide the current tooltip window, if there is any.
Value is t if tooltip was open, nil otherwise.  */)
     (void)
{
#if 0
  if (gtk3wl_tooltip == nil || ![gtk3wl_tooltip isActive])
    return Qnil;
  [gtk3wl_tooltip hide];
  return Qt;
#endif
  return Qnil;
}

/* Return geometric attributes of FRAME.  According to the value of
   ATTRIBUTES return the outer edges of FRAME (Qouter_edges), the inner
   edges of FRAME, the root window edges of frame (Qroot_edges).  Any
   other value means to return the geometry as returned by
   Fx_frame_geometry.  */
static Lisp_Object
frame_geometry (Lisp_Object frame, Lisp_Object attribute)
{
  struct frame *f = decode_live_frame (frame);
  Lisp_Object fullscreen_symbol = Fframe_parameter (frame, Qfullscreen);
  bool fullscreen = (EQ (fullscreen_symbol, Qfullboth)
		     || EQ (fullscreen_symbol, Qfullscreen));
  int border = fullscreen ? 0 : f->border_width;
#if 0
  int title_height = fullscreen ? 0 : FRAME_GTK3WL_TITLEBAR_HEIGHT (f);
#else
  int title_height = 0;
#endif
  int native_width = FRAME_PIXEL_WIDTH (f);
  int native_height = FRAME_PIXEL_HEIGHT (f);
  int outer_width = native_width + 2 * border;
  int outer_height = native_height + 2 * border + title_height;
  int native_left = f->left_pos + border;
  int native_top = f->top_pos + border + title_height;
  int native_right = f->left_pos + outer_width - border;
  int native_bottom = f->top_pos + outer_height - border;
  int internal_border_width = FRAME_INTERNAL_BORDER_WIDTH (f);
#if 0
  int tool_bar_height = FRAME_TOOLBAR_HEIGHT (f);
#else
  int tool_bar_height = 0;
#endif
  int tool_bar_width = (tool_bar_height
			? outer_width - 2 * internal_border_width
			: 0);

  /* Construct list.  */
  if (EQ (attribute, Qouter_edges))
    return list4 (make_number (f->left_pos), make_number (f->top_pos),
		  make_number (f->left_pos + outer_width),
		  make_number (f->top_pos + outer_height));
  else if (EQ (attribute, Qnative_edges))
    return list4 (make_number (native_left), make_number (native_top),
		  make_number (native_right), make_number (native_bottom));
  else if (EQ (attribute, Qinner_edges))
    return list4 (make_number (native_left + internal_border_width),
		  make_number (native_top
			       + tool_bar_height
			       + internal_border_width),
		  make_number (native_right - internal_border_width),
		  make_number (native_bottom - internal_border_width));
  else
    return
      listn (CONSTYPE_HEAP, 10,
	     Fcons (Qouter_position,
		    Fcons (make_number (f->left_pos),
			   make_number (f->top_pos))),
	     Fcons (Qouter_size,
		    Fcons (make_number (outer_width),
			   make_number (outer_height))),
	     Fcons (Qexternal_border_size,
		    (fullscreen
		     ? Fcons (make_number (0), make_number (0))
		     : Fcons (make_number (border), make_number (border)))),
	     Fcons (Qtitle_bar_size,
		    Fcons (make_number (0), make_number (title_height))),
	     Fcons (Qmenu_bar_external, Qnil),
	     Fcons (Qmenu_bar_size, Fcons (make_number (0), make_number (0))),
	     Fcons (Qtool_bar_external,
		    FRAME_EXTERNAL_TOOL_BAR (f) ? Qt : Qnil),
	     Fcons (Qtool_bar_position, FRAME_TOOL_BAR_POSITION (f)),
	     Fcons (Qtool_bar_size,
		    Fcons (make_number (tool_bar_width),
			   make_number (tool_bar_height))),
	     Fcons (Qinternal_border_width,
		    make_number (internal_border_width)));
}

DEFUN ("gtk3wl-frame-geometry", Fgtk3wl_frame_geometry, Sgtk3wl_frame_geometry, 0, 1, 0,
       doc: /* Return geometric attributes of FRAME.
FRAME must be a live frame and defaults to the selected one.  The return
value is an association list of the attributes listed below.  All height
and width values are in pixels.

`outer-position' is a cons of the outer left and top edges of FRAME
  relative to the origin - the position (0, 0) - of FRAME's display.

`outer-size' is a cons of the outer width and height of FRAME.  The
  outer size includes the title bar and the external borders as well as
  any menu and/or tool bar of frame.

`external-border-size' is a cons of the horizontal and vertical width of
  FRAME's external borders as supplied by the window manager.

`title-bar-size' is a cons of the width and height of the title bar of
  FRAME as supplied by the window manager.  If both of them are zero,
  FRAME has no title bar.  If only the width is zero, Emacs was not
  able to retrieve the width information.

`menu-bar-external', if non-nil, means the menu bar is external (never
  included in the inner edges of FRAME).

`menu-bar-size' is a cons of the width and height of the menu bar of
  FRAME.

`tool-bar-external', if non-nil, means the tool bar is external (never
  included in the inner edges of FRAME).

`tool-bar-position' tells on which side the tool bar on FRAME is and can
  be one of `left', `top', `right' or `bottom'.  If this is nil, FRAME
  has no tool bar.

`tool-bar-size' is a cons of the width and height of the tool bar of
  FRAME.

`internal-border-width' is the width of the internal border of
  FRAME.  */)
  (Lisp_Object frame)
{
  return frame_geometry (frame, Qnil);
}

DEFUN ("gtk3wl-frame-edges", Fgtk3wl_frame_edges, Sgtk3wl_frame_edges, 0, 2, 0,
       doc: /* Return edge coordinates of FRAME.
FRAME must be a live frame and defaults to the selected one.  The return
value is a list of the form (LEFT, TOP, RIGHT, BOTTOM).  All values are
in pixels relative to the origin - the position (0, 0) - of FRAME's
display.

If optional argument TYPE is the symbol `outer-edges', return the outer
edges of FRAME.  The outer edges comprise the decorations of the window
manager (like the title bar or external borders) as well as any external
menu or tool bar of FRAME.  If optional argument TYPE is the symbol
`native-edges' or nil, return the native edges of FRAME.  The native
edges exclude the decorations of the window manager and any external
menu or tool bar of FRAME.  If TYPE is the symbol `inner-edges', return
the inner edges of FRAME.  These edges exclude title bar, any borders,
menu bar or tool bar of FRAME.  */)
  (Lisp_Object frame, Lisp_Object type)
{
  return frame_geometry (frame, ((EQ (type, Qouter_edges)
				  || EQ (type, Qinner_edges))
				 ? type
				 : Qnative_edges));
}

DEFUN ("gtk3wl-set-mouse-absolute-pixel-position",
       Fgtk3wl_set_mouse_absolute_pixel_position,
       Sgtk3wl_set_mouse_absolute_pixel_position, 2, 2, 0,
       doc: /* Move mouse pointer to absolute pixel position (X, Y).
The coordinates X and Y are interpreted in pixels relative to a position
\(0, 0) of the selected frame's display.  */)
       (Lisp_Object x, Lisp_Object y)
{
#ifdef GTK3WL_IMPL_COCOA
  /* GNUstep doesn't support CGWarpMouseCursorPosition, so none of
     this will work. */
  struct frame *f = SELECTED_FRAME ();
  EmacsView *view = FRAME_GTK3WL_VIEW (f);
  NSScreen *screen = [[view window] screen];
  NSRect screen_frame = [screen frame];
  int mouse_x, mouse_y;

  NSScreen *primary_screen = [[NSScreen screens] objectAtIndex:0];
  NSRect primary_screen_frame = [primary_screen frame];
  CGFloat primary_screen_height = primary_screen_frame.size.height;

  if (FRAME_INITIAL_P (f) || !FRAME_GTK3WL_P (f))
    return Qnil;

  CHECK_TYPE_RANGED_INTEGER (int, x);
  CHECK_TYPE_RANGED_INTEGER (int, y);

  mouse_x = screen_frame.origin.x + XINT (x);

  if (screen == primary_screen)
    mouse_y = screen_frame.origin.y + XINT (y);
  else
    mouse_y = (primary_screen_height - screen_frame.size.height
               - screen_frame.origin.y) + XINT (y);

  CGPoint mouse_pos = CGPointMake(mouse_x, mouse_y);
  CGWarpMouseCursorPosition (mouse_pos);
#endif /* GTK3WL_IMPL_COCOA */

  return Qnil;
}

DEFUN ("gtk3wl-mouse-absolute-pixel-position",
       Fgtk3wl_mouse_absolute_pixel_position,
       Sgtk3wl_mouse_absolute_pixel_position, 0, 0, 0,
       doc: /* Return absolute position of mouse cursor in pixels.
The position is returned as a cons cell (X . Y) of the
coordinates of the mouse cursor position in pixels relative to a
position (0, 0) of the selected frame's terminal. */)
     (void)
{
  struct frame *f = SELECTED_FRAME ();
#if 0
  EmacsView *view = FRAME_GTK3WL_VIEW (f);
  NSScreen *screen = [[view window] screen];
  NSPoint pt = [NSEvent mouseLocation];

  return Fcons(make_number(pt.x - screen.frame.origin.x),
               make_number(screen.frame.size.height -
                           (pt.y - screen.frame.origin.y)));
#endif
  return Fcons(make_number(0), make_number(0));
}

/* ==========================================================================

    Class implementations

   ========================================================================== */

#if 0
/*
  Handle arrow/function/control keys and copy/paste/cut in file dialogs.
  Return YES if handled, NO if not.
 */
static BOOL
handlePanelKeys (NSSavePanel *panel, NSEvent *theEvent)
{
  NSString *s;
  int i;
  BOOL ret = NO;

  if ([theEvent type] != NSEventTypeKeyDown) return NO;
  s = [theEvent characters];

  for (i = 0; i < [s length]; ++i)
    {
      int ch = (int) [s characterAtIndex: i];
      switch (ch)
        {
        case NSHomeFunctionKey:
        case NSDownArrowFunctionKey:
        case NSUpArrowFunctionKey:
        case NSLeftArrowFunctionKey:
        case NSRightArrowFunctionKey:
        case NSPageUpFunctionKey:
        case NSPageDownFunctionKey:
        case NSEndFunctionKey:
          /* Don't send command modified keys, as those are handled in the
             performKeyEquivalent method of the super class.
          */
          if (! ([theEvent modifierFlags] & NSEventModifierFlagCommand))
            {
              [panel sendEvent: theEvent];
              ret = YES;
            }
          break;
          /* As we don't have the standard key commands for
             copy/paste/cut/select-all in our edit menu, we must handle
             them here.  TODO: handle Emacs key bindings for copy/cut/select-all
             here, paste works, because we have that in our Edit menu.
             I.e. refactor out code in nsterm.m, keyDown: to figure out the
             correct modifier.
          */
        case 'x': // Cut
        case 'c': // Copy
        case 'v': // Paste
        case 'a': // Select all
          if ([theEvent modifierFlags] & NSEventModifierFlagCommand)
            {
              [NSApp sendAction:
                       (ch == 'x'
                        ? @selector(cut:)
                        : (ch == 'c'
                           ? @selector(copy:)
                           : (ch == 'v'
                              ? @selector(paste:)
                              : @selector(selectAll:))))
                             to:nil from:panel];
              ret = YES;
            }
        default:
          // Send all control keys, as the text field supports C-a, C-f, C-e
          // C-b and more.
          if ([theEvent modifierFlags] & NSEventModifierFlagControl)
            {
              [panel sendEvent: theEvent];
              ret = YES;
            }
          break;
        }
    }


  return ret;
}

@implementation EmacsSavePanel
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
  BOOL ret = handlePanelKeys (self, theEvent);
  if (! ret)
    ret = [super performKeyEquivalent:theEvent];
  return ret;
}
@end


@implementation EmacsOpenPanel
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
  // NSOpenPanel inherits NSSavePanel, so passing self is OK.
  BOOL ret = handlePanelKeys (self, theEvent);
  if (! ret)
    ret = [super performKeyEquivalent:theEvent];
  return ret;
}
@end


@implementation EmacsFileDelegate
/* --------------------------------------------------------------------------
   Delegate methods for Open/Save panels
   -------------------------------------------------------------------------- */
- (BOOL)panel: (id)sender isValidFilename: (NSString *)filename
{
  return YES;
}
- (BOOL)panel: (id)sender shouldShowFilename: (NSString *)filename
{
  return YES;
}
- (NSString *)panel: (id)sender userEnteredFilename: (NSString *)filename
          confirmed: (BOOL)okFlag
{
  return filename;
}
@end
#endif

#endif


/* ==========================================================================

    Lisp interface declaration

   ========================================================================== */


void
syms_of_gtk3wlfns (void)
{
  DEFSYM (Qfontsize, "fontsize");
  DEFSYM (Qframe_title_format, "frame-title-format");
  DEFSYM (Qicon_title_format, "icon-title-format");
  DEFSYM (Qdark, "dark");

  DEFVAR_LISP ("gtk3wl-icon-type-alist", Vgtk3wl_icon_type_alist,
               doc: /* Alist of elements (REGEXP . IMAGE) for images of icons associated to frames.
If the title of a frame matches REGEXP, then IMAGE.tiff is
selected as the image of the icon representing the frame when it's
miniaturized.  If an element is t, then Emacs tries to select an icon
based on the filetype of the visited file.

The images have to be installed in a folder called English.lproj in the
Emacs folder.  You have to restart Emacs after installing new icons.

Example: Install an icon Gnus.tiff and execute the following code

  (setq ns-icon-type-alist
        (append ns-icon-type-alist
                \\='((\"^\\\\*\\\\(Group\\\\*$\\\\|Summary \\\\|Article\\\\*$\\\\)\"
                   . \"Gnus\"))))

When you miniaturize a Group, Summary or Article frame, Gnus.tiff will
be used as the image of the icon representing the frame.  */);
  Vgtk3wl_icon_type_alist = list1 (Qt);

  DEFVAR_LISP ("gtk3wl-version-string", Vgtk3wl_version_string,
               doc: /* Toolkit version for NS Windowing.  */);
  Vgtk3wl_version_string = gtk3wl_appkit_version_str ();

#if 0
  defsubr (&Sgtk3wl_read_file_name);
#endif
  defsubr (&Sgtk3wl_get_resource);
  defsubr (&Sgtk3wl_set_resource);
  defsubr (&Sxw_display_color_p); /* this and next called directly by C code */
  defsubr (&Sx_display_grayscale_p);
  defsubr (&Sgtk3wl_font_name);
#ifdef GTK3WL_IMPL_COCOA
  defsubr (&Sgtk3wl_do_applescript);
#endif
  defsubr (&Sxw_color_defined_p);
  defsubr (&Sxw_color_values);
  defsubr (&Sx_server_max_request_size);
  defsubr (&Sx_server_vendor);
  defsubr (&Sx_server_version);
  defsubr (&Sx_display_pixel_width);
  defsubr (&Sx_display_pixel_height);
  defsubr (&Sgtk3wl_display_monitor_attributes_list);
  defsubr (&Sgtk3wl_frame_geometry);
  defsubr (&Sgtk3wl_frame_edges);
  defsubr (&Sgtk3wl_frame_list_z_order);
  defsubr (&Sgtk3wl_frame_restack);
  defsubr (&Sgtk3wl_set_mouse_absolute_pixel_position);
  defsubr (&Sgtk3wl_mouse_absolute_pixel_position);
  defsubr (&Sx_display_mm_width);
  defsubr (&Sx_display_mm_height);
  defsubr (&Sx_display_screens);
  defsubr (&Sx_display_planes);
  defsubr (&Sx_display_color_cells);
  defsubr (&Sx_display_visual_class);
  defsubr (&Sx_display_backing_store);
  defsubr (&Sx_display_save_under);
  defsubr (&Sx_create_frame);
  defsubr (&Sx_open_connection);
  defsubr (&Sx_close_connection);
  defsubr (&Sx_display_list);

  defsubr (&Sgtk3wl_hide_others);
  defsubr (&Sgtk3wl_hide_emacs);
  defsubr (&Sgtk3wl_emacs_info_panel);
  defsubr (&Sgtk3wl_list_services);
  defsubr (&Sgtk3wl_perform_service);
#if 0
  defsubr (&Sgtk3wl_popup_font_panel);
  defsubr (&Sgtk3wl_popup_color_panel);
#endif

  defsubr (&Sx_show_tip);
  defsubr (&Sx_hide_tip);

  as_status = 0;
  as_script = Qnil;
  as_result = 0;


/* This is not ifdef:ed, so other builds than GTK can customize it.  */
  DEFVAR_BOOL ("x-gtk-use-old-file-dialog", x_gtk_use_old_file_dialog,
    doc: /* Non-nil means prompt with the old GTK file selection dialog.
If nil or if the file selection dialog is not available, the new GTK file
chooser is used instead.  To turn off all file dialogs set the
variable `use-file-dialog'.  */);
  x_gtk_use_old_file_dialog = false;

  DEFVAR_BOOL ("x-gtk-show-hidden-files", x_gtk_show_hidden_files,
    doc: /* If non-nil, the GTK file chooser will by default show hidden files.
Note that this is just the default, there is a toggle button on the file
chooser to show or not show hidden files on a case by case basis.  */);
  x_gtk_show_hidden_files = false;

  DEFVAR_BOOL ("x-gtk-file-dialog-help-text", x_gtk_file_dialog_help_text,
    doc: /* If non-nil, the GTK file chooser will show additional help text.
If more space for files in the file chooser dialog is wanted, set this to nil
to turn the additional text off.  */);
  x_gtk_file_dialog_help_text = true;

  DEFVAR_BOOL ("x-gtk-use-system-tooltips", x_gtk_use_system_tooltips,
    doc: /* If non-nil with a Gtk+ built Emacs, the Gtk+ tooltip is used.
Otherwise use Emacs own tooltip implementation.
When using Gtk+ tooltips, the tooltip face is not used.  */);
  x_gtk_use_system_tooltips = true;


  DEFSYM (Qmono, "mono");

#ifdef USE_CAIRO
  DEFSYM (Qpdf, "pdf");

  DEFSYM (Qorientation, "orientation");
  DEFSYM (Qtop_margin, "top-margin");
  DEFSYM (Qbottom_margin, "bottom-margin");
  DEFSYM (Qportrait, "portrait");
  DEFSYM (Qlandscape, "landscape");
  DEFSYM (Qreverse_portrait, "reverse-portrait");
  DEFSYM (Qreverse_landscape, "reverse-landscape");
#endif
}

#include <stdarg.h>
#include <time.h>
void gtk3wl_log(const char *file, int lineno, const char *fmt, ...)
{
  struct timespec ts;
  struct tm tm;
  char timestr[32];
  va_list ap;

  clock_gettime(CLOCK_REALTIME, &ts);

  localtime_r(&ts.tv_sec, &tm);
  strftime(timestr, sizeof timestr, "%H:%M:%S", &tm);

  fprintf(stderr, "%s %.10s:%04d ", timestr, file, lineno);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

void gtk3wl_backtrace(const char *file, int lineno)
{
  Lisp_Object bt = make_uninit_vector(10);
  for (int i = 0; i < 10; i++)
    ASET(bt, i, Qnil);

  struct timespec ts;
  struct tm tm;
  char timestr[32];

  clock_gettime(CLOCK_REALTIME, &ts);

  localtime_r(&ts.tv_sec, &tm);
  strftime(timestr, sizeof timestr, "%H:%M:%S", &tm);

  fprintf(stderr, "%s %.10s:%04d ********\n", timestr, file, lineno);

  get_backtrace(bt);
  for (int i = 0; i < 10; i++) {
    Lisp_Object stk = AREF(bt, i);
    if (!NILP(stk)) {
      Lisp_Object args[2] = { build_string("%S"), stk };
      Lisp_Object str = Fformat(2, args);
      fprintf(stderr, "%s %.10s:%04d %s\n", timestr, file, lineno, SSDATA(str));
    }
  }

  fprintf(stderr, "%s %.10s:%04d ********\n", timestr, file, lineno);
}

/* Definitions and headers for communication with Gtk+3 with wayland.
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


#include "dispextern.h"
#include "frame.h"
#include "character.h"
#include "font.h"
#include "sysselect.h"

#ifdef HAVE_GTK3WL

#include <gtk/gtk.h>

extern void gtk3wl_log(const char *file, int lineno, const char *fmt, ...);
#define GTK3WL_TRACE(fmt, ...) gtk3wl_log(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
extern void gtk3wl_backtrace(const char *file, int lineno);
#define GTK3WL_BACKTRACE() gtk3wl_backtrace(__FILE__, __LINE__)

/* could use list to store these, but rest of emacs has a big infrastructure
   for managing a table of bitmap "records" */
struct gtk3wl_bitmap_record
{
  void *img;
  char *file;
  int refcount;
  int height, width, depth;
};

/* this to map between emacs color indices and NSColor objects */
struct gtk3wl_color_table
{
  ptrdiff_t size;
  ptrdiff_t avail;
  void **items;
  void *availIndices;
};
#define GTK3WL_COLOR_CAPACITY 256

#define RGB_TO_ULONG(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define ARGB_TO_ULONG(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

#define ALPHA_FROM_ULONG(color) ((color) >> 24)
#define RED_FROM_ULONG(color) (((color) >> 16) & 0xff)
#define GREEN_FROM_ULONG(color) (((color) >> 8) & 0xff)
#define BLUE_FROM_ULONG(color) ((color) & 0xff)

/* Do not change `* 0x101' in the following lines to `<< 8'.  If
   changed, image masks in 1-bit depth will not work. */
#define RED16_FROM_ULONG(color) (RED_FROM_ULONG(color) * 0x101)
#define GREEN16_FROM_ULONG(color) (GREEN_FROM_ULONG(color) * 0x101)
#define BLUE16_FROM_ULONG(color) (BLUE_FROM_ULONG(color) * 0x101)

struct scroll_bar
{
  /* These fields are shared by all vectors.  */
  union vectorlike_header header;

  /* The window we're a scroll bar for.  */
  Lisp_Object window;

  /* The next and previous in the chain of scroll bars in this frame.  */
  Lisp_Object next, prev;

  /* Fields from `x_window' down will not be traced by the GC.  */

  /* The X window representing this scroll bar.  */
  Window x_window;

  /* The position and size of the scroll bar in pixels, relative to the
     frame.  */
  int top, left, width, height;

  /* The starting and ending positions of the handle, relative to the
     handle area (i.e. zero is the top position, not
     SCROLL_BAR_TOP_BORDER).  If they're equal, that means the handle
     hasn't been drawn yet.

     These are not actually the locations where the beginning and end
     are drawn; in order to keep handles from becoming invisible when
     editing large files, we establish a minimum height by always
     drawing handle bottoms VERTICAL_SCROLL_BAR_MIN_HANDLE pixels below
     where they would be normally; the bottom and top are in a
     different co-ordinate system.  */
  int start, end;

  /* If the scroll bar handle is currently being dragged by the user,
     this is the number of pixels from the top of the handle to the
     place where the user grabbed it.  If the handle isn't currently
     being dragged, this is -1.  */
  int dragging;

#if defined (USE_TOOLKIT_SCROLL_BARS) && defined (USE_LUCID)
  /* Last scroll bar part seen in xaw_jump_callback and xaw_scroll_callback.  */
  enum scroll_bar_part last_seen_part;
#endif

#if defined (USE_TOOLKIT_SCROLL_BARS) && !defined (USE_GTK)
  /* Last value of whole for horizontal scrollbars.  */
  int whole;
#endif

  /* True if the scroll bar is horizontal.  */
  bool horizontal;
};

/* this extends font backend font */
struct gtk3wlfont_info
{
  struct font font;

  char *name;  /* PostScript name, uniquely identifies on GTK3WL systems */

  /* The following metrics are stored as float rather than int. */

  float width;  /* Maximum advance for the font.  */
  float height;
  float underpos;
  float underwidth;
  float size;
  void *gtk3wlfont;
  void *cgfont;
  char bold, ital;  /* convenience flags */
  char synthItal;
  XCharStruct max_bounds;
  /* we compute glyph codes and metrics on-demand in blocks of 256 indexed
     by hibyte, lobyte */
  unsigned short **glyphs; /* map Unicode index to glyph */
  struct font_metrics **metrics;
};


/* init'd in gtk3wl_initialize_display_info () */
struct gtk3wl_display_info
{
  /* Chain of all gtk3wl_display_info structures.  */
  struct gtk3wl_display_info *next;

  /* The generic display parameters corresponding to this GTK3WL display. */
  struct terminal *terminal;

  /* This is a cons cell of the form (NAME . FONT-LIST-CACHE).  */
  Lisp_Object name_list_element;

  /* The number of fonts loaded. */
  int n_fonts;

  /* Minimum width over all characters in all fonts in font_table.  */
  int smallest_char_width;

  /* Minimum font height over all fonts in font_table.  */
  int smallest_font_height;

  struct gtk3wl_bitmap_record *bitmaps;
  ptrdiff_t bitmaps_size;
  ptrdiff_t bitmaps_last;

  struct gtk3wl_color_table *color_table;

  /* DPI resolution of this screen */
  double resx, resy;

  /* Mask of things that cause the mouse to be grabbed */
  int grabbed;

  int n_planes;

  int color_p;

  Window root_window;

  /* Xism */
  XrmDatabase xrdb;

  /* The cursor to use for vertical scroll bars. */
  Cursor vertical_scroll_bar_cursor;

  /* The cursor to use for horizontal scroll bars. */
  Cursor horizontal_scroll_bar_cursor;

  /* Information about the range of text currently shown in
     mouse-face.  */
  Mouse_HLInfo mouse_highlight;

  struct frame *x_highlight_frame;
  struct frame *x_focus_frame;

  /* The last frame mentioned in a FocusIn or FocusOut event.  This is
     separate from x_focus_frame, because whether or not LeaveNotify
     events cause us to lose focus depends on whether or not we have
     received a FocusIn event for it.  */
  struct frame *x_focus_event_frame;

  /* The frame where the mouse was last time we reported a mouse event.  */
  struct frame *last_mouse_frame;

  /* The frame where the mouse was last time we reported a mouse motion.  */
  struct frame *last_mouse_motion_frame;

  /* Position where the mouse was last time we reported a motion.
     This is a position on last_mouse_motion_frame.  */
  int last_mouse_motion_x;
  int last_mouse_motion_y;

  /* Where the mouse was last time we reported a mouse position.  */
  XRectangle last_mouse_glyph;

  /* Time of last mouse movement.  */
  Time last_mouse_movement_time;

  /* The scroll bar in which the last motion event occurred.  */
  void *last_mouse_scroll_bar;

  /* The GDK cursor for scroll bars and popup menus.  */
  GdkCursor *xg_cursor;
};

/* This is a chain of structures for all the GTK3WL displays currently in use.  */
extern struct gtk3wl_display_info *x_display_list;

struct gtk3wl_output
{
  void *view;
  void *miniimage;
  unsigned long cursor_color;
  unsigned long foreground_color;
  unsigned long background_color;
  void *toolbar;

  /* GTK3WLCursors init'ed in initFrameFromEmacs */
  Cursor text_cursor;
  Cursor nontext_cursor;
  Cursor modeline_cursor;
  Cursor hand_cursor;
  Cursor hourglass_cursor;
  Cursor horizontal_drag_cursor;
  Cursor vertical_drag_cursor;
  Cursor left_edge_cursor;
  Cursor top_left_corner_cursor;
  Cursor top_edge_cursor;
  Cursor top_right_corner_cursor;
  Cursor right_edge_cursor;
  Cursor bottom_right_corner_cursor;
  Cursor bottom_edge_cursor;
  Cursor bottom_left_corner_cursor;

  /* GTK3WL-specific */
  Cursor current_pointer;

  XGCValues cursor_xgcv;

  /* lord knows why Emacs needs to know about our Window ids.. */
  Window window_desc, parent_desc;
  char explicit_parent;

  struct font *font;
  int baseline_offset;

  /* If a fontset is specified for this frame instead of font, this
     value contains an ID of the fontset, else -1.  */
  int fontset; /* only used with font_backend */

  int icon_top;
  int icon_left;

  /* The size of the extra width currently allotted for vertical
     scroll bars, in pixels.  */
  int vertical_scroll_bar_extra;

  /* The height of the titlebar decoration (included in GTK3WLWindow's frame). */
  int titlebar_height;

  /* The height of the toolbar if displayed, else 0. */
  int toolbar_height;

  /* This is the Emacs structure for the GTK3WL display this frame is on.  */
  struct gtk3wl_display_info *display_info;

  /* Non-zero if we are zooming (maximizing) the frame.  */
  int zooming;

  /* Non-zero if we are doing an animation, e.g. toggling the tool bar. */
  int in_animation;

  /* The last size hints set.  */
  GdkGeometry size_hints;
  long hint_flags;

  /* The widget of this screen.  This is the window of a top widget.  */
  GtkWidget *widget;
  /* The widget of the edit portion of this screen; the window in
     "window_desc" is inside of this.  */
  GtkWidget *edit_widget;
  /* The widget used for laying out widgets vertically.  */
  GtkWidget *vbox_widget;
  /* The widget used for laying out widgets horizontally.  */
  GtkWidget *hbox_widget;
  /* The menubar in this frame.  */
  GtkWidget *menubar_widget;
  /* The tool bar in this frame  */
  GtkWidget *toolbar_widget;
  /* True if tool bar is packed into the hbox widget (i.e. vertical).  */
  bool_bf toolbar_in_hbox : 1;
  bool_bf toolbar_is_packed : 1;

#ifdef USE_GTK_TOOLTIP
  GtkTooltip *ttip_widget;
  GtkWidget *ttip_lbl;
  GtkWindow *ttip_window;
#endif /* USE_GTK_TOOLTIP */

  /* Height of menu bar widget, in pixels.  This value
     is not meaningful if the menubar is turned off.  */
  int menubar_height;

  /* Height of tool bar widget, in pixels.  top_height is used if tool bar
     at top, bottom_height if tool bar is at the bottom.
     Zero if not using an external tool bar or if tool bar is vertical.  */
  int toolbar_top_height, toolbar_bottom_height;

  /* Width of tool bar widget, in pixels.  left_width is used if tool bar
     at left, right_width if tool bar is at the right.
     Zero if not using an external tool bar or if tool bar is horizontal.  */
  int toolbar_left_width, toolbar_right_width;

#ifdef USE_CAIRO
  /* Cairo drawing context.  */
  cairo_t *cr_context;
  /* Cairo surface for double buffering */
  cairo_surface_t *cr_surface;
#endif

  int has_been_visible;

  /* Relief GCs, colors etc.  */
  struct relief
  {
    XGCValues xgcv;
    unsigned long pixel;
  }
  black_relief, white_relief;

  /* The background for which the above relief GCs were set up.
     They are changed only when a different background is involved.  */
  unsigned long relief_background;

  /* Keep track of focus.  May be EXPLICIT if we received a FocusIn for this
     frame, or IMPLICIT if we received an EnterNotify.
     FocusOut and LeaveNotify clears EXPLICIT/IMPLICIT. */
  int focus_state;
};

/* this dummy decl needed to support TTYs */
struct x_output
{
  int unused;
};

enum
{
  /* Values for focus_state, used as bit mask.
     EXPLICIT means we received a FocusIn for the frame and know it has
     the focus.  IMPLICIT means we received an EnterNotify and the frame
     may have the focus if no window manager is running.
     FocusOut and LeaveNotify clears EXPLICIT/IMPLICIT. */
  FOCUS_NONE     = 0,
  FOCUS_IMPLICIT = 1,
  FOCUS_EXPLICIT = 2
};

/* This gives the gtk3wl_display_info structure for the display F is on.  */
#define FRAME_DISPLAY_INFO(f) ((f)->output_data.gtk3wl->display_info)
#define FRAME_X_OUTPUT(f) ((f)->output_data.gtk3wl)
#define FRAME_GTK3WL_WINDOW(f) ((f)->output_data.gtk3wl->window_desc)
#define FRAME_X_WINDOW(f) ((f)->output_data.gtk3wl->window_desc)

/* This is the `Display *' which frame F is on.  */
#define FRAME_GTK3WL_DISPLAY(f) (0)
#define FRAME_X_DISPLAY(f) (0)
#define FRAME_X_SCREEN(f) (0)
#define FRAME_X_VISUAL(f) FRAME_DISPLAY_INFO(f)->visual

#define DEFAULT_GDK_DISPLAY() gdk_display_get_default()

#define GDK_DISPLAY_XDISPLAY(gdpy) 0

#define FRAME_X_DOUBLE_BUFFERED_P(f) 0

/* Turning a lisp vector value into a pointer to a struct scroll_bar.  */
#define XSCROLL_BAR(vec) ((struct scroll_bar *) XVECTOR (vec))

// `wx` defined in gtkutil.h
#define FRAME_GTK_OUTER_WIDGET(f) ((f)->output_data.wx->widget)
#define FRAME_GTK_WIDGET(f) ((f)->output_data.wx->edit_widget)
#define FRAME_OUTER_WINDOW(f)                                   \
       (FRAME_GTK_OUTER_WIDGET (f) ?                            \
        GTK_WIDGET_TO_X_WIN (FRAME_GTK_OUTER_WIDGET (f)) :      \
         FRAME_X_WINDOW (f))
#define GTK_WIDGET_TO_X_WIN(w) 0

#define FRAME_FOREGROUND_COLOR(f) ((f)->output_data.gtk3wl->foreground_color)
#define FRAME_BACKGROUND_COLOR(f) ((f)->output_data.gtk3wl->background_color)

#define GTK3WL_FACE_FOREGROUND(f) ((f)->foreground)
#define GTK3WL_FACE_BACKGROUND(f) ((f)->background)

#define FRAME_DEFAULT_FACE(f) FACE_FROM_ID_OR_NULL (f, DEFAULT_FACE_ID)

#define FRAME_GTK3WL_VIEW(f) ((f)->output_data.gtk3wl->view)
#define FRAME_CURSOR_COLOR(f) ((f)->output_data.gtk3wl->cursor_color)
#define FRAME_POINTER_TYPE(f) ((f)->output_data.gtk3wl->current_pointer)

#define FRAME_FONT(f) ((f)->output_data.gtk3wl->font)

#define XGTK3WL_SCROLL_BAR(vec) XSAVE_POINTER (vec, 0)

/* Compute pixel height of the frame's titlebar. */
#define FRAME_GTK3WL_TITLEBAR_HEIGHT(f)                                     0
#if 0
  (GTK3WLHeight([FRAME_GTK3WL_VIEW (f) frame]) == 0 ?                           \
   0                                                                    \
   : (int)(GTK3WLHeight([FRAME_GTK3WL_VIEW (f) window].frame)                   \
           - GTK3WLHeight([GTK3WLWindow contentRectForFrameRect:                \
                       [[FRAME_GTK3WL_VIEW (f) window] frame]               \
                       styleMask:[[FRAME_GTK3WL_VIEW (f) window] styleMask]])))
#endif

/* Compute pixel height of the toolbar. */
#define FRAME_TOOLBAR_HEIGHT(f)                                         0
#if 0
  (([[FRAME_GTK3WL_VIEW (f) window] toolbar] == nil                         \
    || ! [[FRAME_GTK3WL_VIEW (f) window] toolbar].isVisible) ?		\
   0                                                                    \
   : (int)(GTK3WLHeight([GTK3WLWindow contentRectForFrameRect:                  \
                     [[FRAME_GTK3WL_VIEW (f) window] frame]                 \
                     styleMask:[[FRAME_GTK3WL_VIEW (f) window] styleMask]]) \
           - GTK3WLHeight([[[FRAME_GTK3WL_VIEW (f) window] contentView] frame])))
#endif

/* Compute pixel size for vertical scroll bars */
#define GTK3WL_SCROLL_BAR_WIDTH(f)					\
  (FRAME_HAS_VERTICAL_SCROLL_BARS (f)					\
   ? rint (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) > 0			\
	   ? FRAME_CONFIG_SCROLL_BAR_WIDTH (f)				\
	   : (FRAME_SCROLL_BAR_COLS (f) * FRAME_COLUMN_WIDTH (f)))	\
   : 0)

/* Compute pixel size for horizontal scroll bars */
#define GTK3WL_SCROLL_BAR_HEIGHT(f)					\
  (FRAME_HAS_HORIZONTAL_SCROLL_BARS (f)					\
   ? rint (FRAME_CONFIG_SCROLL_BAR_HEIGHT (f) > 0			\
	   ? FRAME_CONFIG_SCROLL_BAR_HEIGHT (f)				\
	   : (FRAME_SCROLL_BAR_LINES (f) * FRAME_LINE_HEIGHT (f)))	\
   : 0)

/* Difference btwn char-column-calculated and actual SB widths.
   This is only a concern for rendering when SB on left. */
#define GTK3WL_SCROLL_BAR_ADJUST(w, f)				\
  (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w) ?			\
   (FRAME_SCROLL_BAR_COLS (f) * FRAME_COLUMN_WIDTH (f)		\
    - GTK3WL_SCROLL_BAR_WIDTH (f)) : 0)

/* Difference btwn char-line-calculated and actual SB heights.
   This is only a concern for rendering when SB on top. */
#define GTK3WL_SCROLL_BAR_ADJUST_HORIZONTALLY(w, f)		\
  (WINDOW_HAS_HORIZONTAL_SCROLL_BARS (w) ?		\
   (FRAME_SCROLL_BAR_LINES (f) * FRAME_LINE_HEIGHT (f)	\
    - GTK3WL_SCROLL_BAR_HEIGHT (f)) : 0)

#define FRAME_MENUBAR_HEIGHT(f) ((f)->output_data.wx->menubar_height)

/* Calculate system coordinates of the left and top of the parent
   window or, if there is no parent window, the screen. */
#define GTK3WL_PARENT_WINDOW_LEFT_POS(f)                                    \
  (FRAME_PARENT_FRAME (f) != NULL                                       \
   ? [[FRAME_GTK3WL_VIEW (f) window] parentWindow].frame.origin.x : 0)
#define GTK3WL_PARENT_WINDOW_TOP_POS(f)                                     \
  (FRAME_PARENT_FRAME (f) != NULL                                       \
   ? ([[FRAME_GTK3WL_VIEW (f) window] parentWindow].frame.origin.y          \
      + [[FRAME_GTK3WL_VIEW (f) window] parentWindow].frame.size.height     \
      - FRAME_GTK3WL_TITLEBAR_HEIGHT (FRAME_PARENT_FRAME (f)))              \
   : [[[GTK3WLScreen screegtk3wl] objectAtIndex: 0] frame].size.height)

#define FRAME_GTK3WL_FONT_TABLE(f) (FRAME_DISPLAY_INFO (f)->font_table)

#define FRAME_TOOLBAR_TOP_HEIGHT(f) ((f)->output_data.wx->toolbar_top_height)
#define FRAME_TOOLBAR_BOTTOM_HEIGHT(f) \
  ((f)->output_data.wx->toolbar_bottom_height)
#define FRAME_TOOLBAR_LEFT_WIDTH(f) ((f)->output_data.wx->toolbar_left_width)
#define FRAME_TOOLBAR_RIGHT_WIDTH(f) ((f)->output_data.wx->toolbar_right_width)
#define FRAME_TOOLBAR_WIDTH(f) \
  (FRAME_TOOLBAR_LEFT_WIDTH (f) + FRAME_TOOLBAR_RIGHT_WIDTH (f))

#define FRAME_FONTSET(f) ((f)->output_data.gtk3wl->fontset)

#define FRAME_BASELINE_OFFSET(f) ((f)->output_data.gtk3wl->baseline_offset)
#define BLACK_PIX_DEFAULT(f) 0x000000
#define WHITE_PIX_DEFAULT(f) 0xFFFFFF

/* First position where characters can be shown (instead of scrollbar, if
   it is on left. */
#define FIRST_CHAR_POSITION(f)				\
  (! (FRAME_HAS_VERTICAL_SCROLL_BARS_ON_LEFT (f)) ? 0	\
   : FRAME_SCROLL_BAR_COLS (f))

extern struct gtk3wl_display_info *gtk3wl_term_init (Lisp_Object display_name);
extern void gtk3wl_term_shutdown (int sig);

/* constants for text rendering */
#define GTK3WL_DUMPGLYPH_NORMAL             0
#define GTK3WL_DUMPGLYPH_CURSOR             1
#define GTK3WL_DUMPGLYPH_FOREGROUND         2
#define GTK3WL_DUMPGLYPH_MOUSEFACE          3



/* In gtk3wlfont, called from fontset.c */
extern void gtk3wlfont_make_fontset_for_font (Lisp_Object name,
                                         Lisp_Object font_object);

/* In gtk3wlfont, for debugging */
struct glyph_string;
void gtk3wl_dump_glyphstring (struct glyph_string *s) EXTERNALLY_VISIBLE;

/* Implemented in gtk3wlterm, published in or needed from gtk3wlfns. */
extern Lisp_Object gtk3wl_list_fonts (struct frame *f, Lisp_Object pattern,
                                  int size, int maxnames);
extern void gtk3wl_clear_frame (struct frame *f);

extern char *gtk3wl_xlfd_to_fontname (const char *xlfd);

extern Lisp_Object gtk3wl_map_event_to_object (void);
#ifdef __OBJC__
extern Lisp_Object gtk3wl_string_from_pasteboard (id pb);
extern void gtk3wl_string_to_pasteboard (id pb, Lisp_Object str);
#endif
extern Lisp_Object gtk3wl_get_local_selection (Lisp_Object selection_name,
                                           Lisp_Object target_type);
extern void nxatoms_of_gtk3wlselect (void);
extern void gtk3wl_set_doc_edited (void);

extern bool
gtk3wl_defined_color (struct frame *f,
                  const char *name,
                  XColor *color_def, bool alloc,
                  bool makeIndex);
#if 0
extern void
gtk3wl_query_color (void *col, XColor *color_def, int setPixel);
#endif
void
gtk3wl_query_color (struct frame *f, XColor *color);
void
gtk3wl_query_colors (struct frame *f, XColor *colors, int ncolors);

int gtk3wl_parse_color (const char *color_name, XColor *color);

extern int gtk3wl_lisp_to_color (Lisp_Object color, XColor *col);
#ifdef __OBJC__
extern GTK3WLColor *gtk3wl_lookup_indexed_color (unsigned long idx, struct frame *f);
extern unsigned long gtk3wl_index_color (GTK3WLColor *color, struct frame *f);
extern const char *gtk3wl_get_pending_menu_title (void);
extern void gtk3wl_check_menu_open (GTK3WLMenu *menu);
extern void gtk3wl_check_pending_open_menu (void);
#endif

extern void gtk3wl_clear_area (struct frame *f, int x, int y, int width, int height);
extern int gtk3wl_gtk_to_emacs_modifiers (int state);

/* C access to ObjC functionality */
extern void  gtk3wl_release_object (void *obj);
extern void  gtk3wl_retain_object (void *obj);
extern void *gtk3wl_alloc_autorelease_pool (void);
extern void gtk3wl_release_autorelease_pool (void *);
extern const char *gtk3wl_get_defaults_value (const char *key);
extern void gtk3wl_init_locale (void);


/* in gtk3wlmenu */
#if 0
extern void update_frame_tool_bar (struct frame *f);
extern void free_frame_tool_bar (struct frame *f);
#endif
extern Lisp_Object find_and_return_menu_selection (struct frame *f,
                                                   bool keymaps,
                                                   void *client_data);
extern Lisp_Object gtk3wl_popup_dialog (struct frame *, Lisp_Object header,
                                    Lisp_Object contents);

#define GTK3WLAPP_DATA2_RUNASSCRIPT 10
extern void gtk3wl_run_ascript (void);

#define GTK3WLAPP_DATA2_RUNFILEDIALOG 11
extern void gtk3wl_run_file_dialog (void);

extern const char *gtk3wl_etc_directory (void);
extern const char *gtk3wl_exec_path (void);
extern const char *gtk3wl_load_path (void);
extern void syms_of_gtk3wlterm (void);
extern void syms_of_gtk3wlfns (void);
extern void syms_of_gtk3wlmenu (void);
extern void syms_of_gtk3wlselect (void);

/* From gtk3wlimage.m, needed in image.c */
struct image;
extern void *gtk3wl_image_from_XBM (char *bits, int width, int height,
                                unsigned long fg, unsigned long bg);
extern void *gtk3wl_image_for_XPM (int width, int height, int depth);
extern void *gtk3wl_image_from_file (Lisp_Object file);
extern bool gtk3wl_load_image (struct frame *f, struct image *img,
			   Lisp_Object spec_file, Lisp_Object spec_data);
extern int gtk3wl_image_width (void *img);
extern int gtk3wl_image_height (void *img);
extern unsigned long gtk3wl_get_pixel (void *img, int x, int y);
extern void gtk3wl_put_pixel (void *img, int x, int y, unsigned long argb);
extern void gtk3wl_set_alpha (void *img, int x, int y, unsigned char a);

extern int x_display_pixel_height (struct gtk3wl_display_info *);
extern int x_display_pixel_width (struct gtk3wl_display_info *);

/* This in gtk3wlterm.c */
extern float gtk3wl_antialias_threshold;
extern void x_destroy_window (struct frame *f);
extern void x_set_undecorated (struct frame *f, Lisp_Object new_value,
                               Lisp_Object old_value);
extern void x_set_parent_frame (struct frame *f, Lisp_Object new_value,
                                Lisp_Object old_value);
extern void x_set_no_focus_on_map (struct frame *f, Lisp_Object new_value,
                                   Lisp_Object old_value);
extern void x_set_no_accept_focus (struct frame *f, Lisp_Object new_value,
                                   Lisp_Object old_value);
extern void x_set_z_group (struct frame *f, Lisp_Object new_value,
                           Lisp_Object old_value);
#ifdef GTK3WL_IMPL_COCOA
extern void gtk3wl_set_appearance (struct frame *f, Lisp_Object new_value,
                               Lisp_Object old_value);
extern void gtk3wl_set_tragtk3wlparent_titlebar (struct frame *f,
                                         Lisp_Object new_value,
                                         Lisp_Object old_value);
#endif
extern int gtk3wl_select (int nfds, fd_set *readfds, fd_set *writefds,
		      fd_set *exceptfds, struct timespec *timeout,
		      sigset_t *sigmask);
#ifdef HAVE_PTHREAD
extern void gtk3wl_run_loop_break (void);
#endif
extern unsigned long gtk3wl_get_rgb_color (struct frame *f,
                                       float r, float g, float b, float a);

struct input_event;
extern void gtk3wl_init_events (struct input_event *);
extern void gtk3wl_finish_events (void);

extern cairo_t *gtk3wl_begin_cr_clip (struct frame *f, XGCValues *gc);
extern void gtk3wl_end_cr_clip (struct frame *f);
extern void gtk3wl_set_cr_source_with_gc_foreground (struct frame *f, XGCValues *gc);
extern void gtk3wl_set_cr_source_with_gc_background (struct frame *f, XGCValues *gc);
extern void gtk3wl_set_cr_source_with_color (struct frame *f, unsigned long color);

#ifdef __OBJC__
/* Needed in gtk3wlfgtk3wl.m.  */
extern void
gtk3wl_set_represented_filename (GTK3WLString *fstr, struct frame *f);

#endif

#ifdef GTK3WL_IMPL_GNUSTEP
extern char gnustep_base_version[];  /* version tracking */
#endif

#define MINWIDTH 10
#define MINHEIGHT 10

/* Screen max coordinate
 Using larger coordinates causes movewindow/placewindow to abort */
#define SCREENMAX 16000

#define GTK3WL_SCROLL_BAR_WIDTH_DEFAULT     [EmacsScroller scrollerWidth]
#define GTK3WL_SCROLL_BAR_HEIGHT_DEFAULT    [EmacsScroller scrollerHeight]
/* This is to match emacs on other platforms, ugly though it is. */
#define GTK3WL_SELECTION_BG_COLOR_DEFAULT	@"LightGoldenrod2";
#define GTK3WL_SELECTION_FG_COLOR_DEFAULT	@"Black";
#define RESIZE_HANDLE_SIZE 12

/* Little utility macros */
#define IN_BOUND(min, x, max) (((x) < (min)) \
                                ? (min) : (((x)>(max)) ? (max) : (x)))
#define SCREENMAXBOUND(x) (IN_BOUND (-SCREENMAX, x, SCREENMAX))

extern void
gtk3wl_clear_under_internal_border (struct frame *f);

#endif	/* HAVE_GTK3WL */

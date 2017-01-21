/*
 * fb-background-monitor.c:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *                     2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "bg.h"
#include "panel.h"

//#define DEBUGPRN
#include "dbg.h"


enum {
    CHANGED,
    LAST_SIGNAL
};


struct _FbBgClass {
    GObjectClass   parent_class;
    void         (*changed) (FbBg *monitor);
};

struct _FbBg {
    GObject    parent_instance;

    Window   xroot;
    Atom     id;
    GC       gc;
    Display *dpy;
    Pixmap   pixmap;
};

static void fb_bg_class_init (FbBgClass *klass);
static void fb_bg_init (FbBg *monitor);
static void fb_bg_finalize (GObject *object);
static void fb_bg_changed(FbBg *monitor);
static Pixmap fb_bg_get_xrootpmap_real(FbBg *bg);

static guint signals [LAST_SIGNAL] = { 0 };

static FbBg *default_bg = NULL;

GType
fb_bg_get_type (void)
{
    static GType object_type = 0;

    if (!object_type) {
        static const GTypeInfo object_info = {
            sizeof (FbBgClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) fb_bg_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (FbBg),
            0,              /* n_preallocs */
            (GInstanceInitFunc) fb_bg_init,
        };

        object_type = g_type_register_static (
            G_TYPE_OBJECT, "FbBg", &object_info, 0);
    }

    return object_type;
}



static void
fb_bg_class_init (FbBgClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    ENTER;
    signals [CHANGED] =
        g_signal_new ("changed",
              G_OBJECT_CLASS_TYPE (object_class),
              G_SIGNAL_RUN_FIRST,
              G_STRUCT_OFFSET (FbBgClass, changed),
              NULL, NULL,
              g_cclosure_marshal_VOID__VOID,
              G_TYPE_NONE, 0);
    klass->changed = fb_bg_changed;
    object_class->finalize = fb_bg_finalize;
    RET();
}

static void
fb_bg_init (FbBg *bg)
{
    XGCValues  gcv;
    uint mask;

    ENTER;
    //bg->dpy = GDK_DISPLAY();
    bg->dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    bg->xroot = DefaultRootWindow(bg->dpy);
    bg->id = XInternAtom(bg->dpy, "_XROOTPMAP_ID", False);
    bg->pixmap = fb_bg_get_xrootpmap_real(bg);
    gcv.ts_x_origin = 0;
    gcv.ts_y_origin = 0;
    gcv.fill_style = FillTiled;
    mask = GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle;
    if (bg->pixmap != None) {
        gcv.tile = bg->pixmap;
        mask |= GCTile ;
    }
    bg->gc = XCreateGC (bg->dpy, bg->xroot, mask, &gcv) ;
    RET();
}


FbBg *
fb_bg_new()
{
    ENTER;
    RET(g_object_new (FB_TYPE_BG, NULL));
}

static void
fb_bg_finalize (GObject *object)
{
    FbBg *bg;

    ENTER;
    bg = FB_BG (object);
    XFreeGC(bg->dpy, bg->gc);
    default_bg = NULL;

    RET();
}

Pixmap
fb_bg_get_xrootpmap(FbBg *bg)
{
    ENTER;
    RET(bg->pixmap);
}

static Pixmap
fb_bg_get_xrootpmap_real(FbBg *bg)
{
    Pixmap ret = None;

    ENTER;
    if (bg->id)
    {
        int  act_format, c = 2 ;
        u_long  nitems ;
        u_long  bytes_after ;
        u_char *prop = NULL;
        Atom ret_type;

        do
        {
            if (XGetWindowProperty(bg->dpy, bg->xroot, bg->id, 0, 1,
                    False, XA_PIXMAP, &ret_type, &act_format,
                    &nitems, &bytes_after, &prop) == Success)
            {
                if (ret_type == XA_PIXMAP)
                {
                    ret = *((Pixmap *)prop);
                    c = -c ; //to quit loop
                }
                XFree(prop);
            }
        } while (--c > 0);
    }
    RET(ret);

}



//GdkPixmap *
//fb_bg_get_xroot_pix_for_area(FbBg *bg, gint x, gint y,
cairo_surface_t *
fb_bg_get_xroot_pix_for_area(FbBg *bg, gint x, gint y, gint width, gint height)
{
    //GdkPixmap *gbgpix;
    cairo_surface_t *gbgpix;
    Pixmap bgpix;

    ENTER;
    if (bg->pixmap == None)
        RET(NULL);
   
    /* berte: TODO */
    //gdk_pixmap_new(NULL, width, height, depth);
    gbgpix = cairo_image_surface_create (CAIRO_FORMAT_A1, width, height);
    
    if (!gbgpix) {
        ERR("gdk_pixmap_new failed\n");
        RET(NULL);
    }
    //bgpix =  gdk_x11_drawable_get_xid(gbgpix);
    //bgpix =  gdk_x11_window_get_xid(gbgpix);
    GtkWidget* widget = gtk_offscreen_window_new();
    bgpix = gdk_x11_window_get_xid(gtk_widget_get_window(widget));

    XSetTSOrigin(bg->dpy, bg->gc, -x, -y) ;
    XFillRectangle(bg->dpy, bgpix, bg->gc, 0, 0, width, height);
    
    RET(gbgpix);
}

//GdkPixmap *
cairo_surface_t *
fb_bg_get_xroot_pix_for_win(FbBg *bg, GtkWidget *widget)
{
    Window win;
    Window dummy;
    Pixmap bgpix;
    //GdkPixmap *gbgpix;
    cairo_surface_t  *gbgpix;
    guint  width, height, border, depth;
    int  x, y;
//    GtkWidget* widget = NULL;

    ENTER;
    if (bg->pixmap == None)
        RET(NULL);

    //win = GDK_WINDOW_XWINDOW(widget->window);
    widget = gtk_offscreen_window_new();
    win = GDK_WINDOW_XID( gtk_widget_get_window(widget) );
    if (!XGetGeometry(bg->dpy, win, &dummy, &x, &y, &width, &height, &border,
              &depth)) {
        DBG2("XGetGeometry failed\n");
        RET(NULL);
    }
    if (width <= 1 || height <= 1)
        RET(NULL);

    XTranslateCoordinates(bg->dpy, win, bg->xroot, 0, 0, &x, &y, &dummy);
    DBG("win=%lx %dx%d%+d%+d\n", win, width, height, x, y);
    //gbgpix = gdk_pixmap_new(NULL, width, height, depth);
    gbgpix = cairo_image_surface_create (CAIRO_FORMAT_A1, width, height);
    if (!gbgpix) {
        ERR("gdk_pixmap_new failed\n");
        RET(NULL);
    }
    //bgpix =  gdk_x11_drawable_get_xid(gbgpix);
    //bgpix =  gdk_x11_window_get_xid(gbgpix);
    //GtkWidget* widget = gtk_offscreen_window_new();
    bgpix = gdk_x11_window_get_xid(gtk_widget_get_window(widget));

    XSetTSOrigin(bg->dpy, bg->gc, -x, -y) ;
    XFillRectangle(bg->dpy, bgpix, bg->gc, 0, 0, width, height);
    RET(gbgpix);
}

void
//fb_bg_composite(GdkDrawable *base, GdkGC *gc, guint32 tintcolor, gint alpha)
fb_bg_composite(GdkWindow *base, guint32 tintcolor, gint alpha)
{
    cairo_t *cr;
    GdkDrawingContext *content;
    ENTER;
    
//    cr = gdk_cairo_create(base);
    content = gdk_window_begin_draw_frame(base, cairo_region_create());
    cr = gdk_drawing_context_get_cairo_context(content);
    //gdk_cairo_set_source_color(cr, tintcolor);
    gdk_cairo_set_source_rgba(cr, NULL); //check GdkRGBA
    cairo_paint_with_alpha(cr, (double) alpha / 255);
    cairo_destroy(cr);
    fb_bg_changed(fb_bg_get_for_display());
    
    RET();
}


static void
fb_bg_changed(FbBg *bg)
{
    ENTER;
    bg->pixmap = fb_bg_get_xrootpmap_real(bg);
    if (bg->pixmap != None) {
        XGCValues  gcv;

        gcv.tile = bg->pixmap;
        XChangeGC(bg->dpy, bg->gc, GCTile, &gcv);
        DBG("changed\n");
    }
    RET();
}


void fb_bg_notify_changed_bg(FbBg *bg)
{
    ENTER;
    g_signal_emit (bg, signals [CHANGED], 0);
    RET();
}

FbBg *fb_bg_get_for_display(void)
{
    ENTER;
    if (!default_bg)
        default_bg = fb_bg_new();
    else
        g_object_ref(default_bg);
    RET(default_bg);
}


/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <string.h>
#include "gtkbgbox.h"
#include "bg.h"
#include <gtk/gtkbin.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
//#include <gdk/gdkpixmap.h>
#include <gdk/gdkprivate.h>
#include <glib.h>
#include <glib-object.h>


//#define DEBUGPRN
#include "dbg.h"

typedef struct {
    //GdkPixmap *pixmap;
    cairo_surface_t *surface;
    guint32 tintcolor;
    gint alpha;
    int bg_type;
    FbBg *bg;
    gulong sid;
} GtkBgboxPrivate;



#define GTK_BGBOX_GET_PRIVATE(obj)  G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_BGBOX, GtkBgboxPrivate)

static void gtk_bgbox_class_init    (GtkBgboxClass *klass);
static void gtk_bgbox_init          (GtkBgbox *bgbox);
static void gtk_bgbox_realize       (GtkWidget *widget);
//static void gtk_bgbox_size_request  (GtkWidget *widget, GtkRequisition   *requisition);
static void gtk_bgbox_size_allocate (GtkWidget *widget, GtkAllocation    *allocation);
static void gtk_bgbox_style_set (GtkWidget *widget, GtkStyle  *previous_style);
static gboolean gtk_bgbox_configure_event(GtkWidget *widget, GdkEventConfigure *e);
#if 0
static gboolean gtk_bgbox_destroy_event (GtkWidget *widget, GdkEventAny *event);
static gboolean gtk_bgbox_delete_event (GtkWidget *widget, GdkEventAny *event);
#endif

static void gtk_bgbox_finalize (GObject *object);

static void gtk_bgbox_set_bg_root(GtkWidget *widget, GtkBgboxPrivate *priv);
static void gtk_bgbox_set_bg_inherit(GtkWidget *widget, GtkBgboxPrivate *priv);
static void gtk_bgbox_bg_changed(FbBg *bg, GtkWidget *widget);

static GtkBinClass *parent_class = NULL;

GType
gtk_bgbox_get_type (void)
{
    static GType bgbox_type = 0;

    if (!bgbox_type)
    {
        static const GTypeInfo bgbox_info =
            {
                sizeof (GtkBgboxClass),
                NULL,		/* base_init */
                NULL,		/* base_finalize */
                (GClassInitFunc) gtk_bgbox_class_init,
                NULL,		/* class_finalize */
                NULL,		/* class_data */
                sizeof (GtkBgbox),
                0,		/* n_preallocs */
                (GInstanceInitFunc) gtk_bgbox_init,
            };

        bgbox_type = g_type_register_static (GTK_TYPE_BIN, "GtkBgbox",
              &bgbox_info, 0);
    }

    return bgbox_type;
}

static void
gtk_bgbox_class_init (GtkBgboxClass *class)
{
    GtkRequisition nat;
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    parent_class = g_type_class_peek_parent (class);

    widget_class->realize         = gtk_bgbox_realize;
//    widget_class->size_request    = gtk_bgbox_size_request;
    gtk_widget_get_preferred_size((GtkWidget*)widget_class, NULL, &nat);
    widget_class->size_allocate   = gtk_bgbox_size_allocate;
    widget_class->style_set       = gtk_bgbox_style_set;
    widget_class->configure_event = gtk_bgbox_configure_event;
    //widget_class->destroy_event   = gtk_bgbox_destroy_event;
    //widget_class->delete_event    = gtk_bgbox_delete_event;

    object_class->finalize = gtk_bgbox_finalize;
    g_type_class_add_private (class, sizeof (GtkBgboxPrivate));
}

static void
gtk_bgbox_init (GtkBgbox *bgbox)
{
    GtkBgboxPrivate *priv;

    ENTER;
    //GTK_WIDGET_UNSET_FLAGS (bgbox, GTK_NO_WINDOW);
    gtk_widget_set_receives_default ((GtkWidget*)bgbox, FALSE);

    priv = GTK_BGBOX_GET_PRIVATE (bgbox);
    priv->bg_type = BG_NONE;
    priv->sid = 0;
    RET();
}

GtkWidget*
gtk_bgbox_new (void)
{
    ENTER;
    RET(g_object_new (GTK_TYPE_BGBOX, NULL));
}

static void
gtk_bgbox_finalize (GObject *object)
{
    GtkBgboxPrivate *priv;

    ENTER;
    priv = GTK_BGBOX_GET_PRIVATE(GTK_WIDGET(object));
    if (priv->surface) {
        g_object_unref(priv->surface);
        priv->surface = NULL;
    }
    if (priv->sid) {
        g_signal_handler_disconnect(priv->bg, priv->sid);
        priv->sid = 0;
    }
    if (priv->bg) {
        g_object_unref(priv->bg);
        priv->bg = NULL;
    }
    RET();
}

static GdkFilterReturn
gtk_bgbox_event_filter(GdkXEvent *xevent, GdkEvent *event, GtkWidget *widget)
{
    XEvent *ev = (XEvent *) xevent;

    ENTER;
    if (ev->type == ConfigureNotify) {
        gtk_widget_queue_draw(widget);
        //gtk_bgbox_style_set(widget, NULL);
        DBG("ConfigureNotify %d %d %d %d\n",
              ev->xconfigure.x,
              ev->xconfigure.y,
              ev->xconfigure.width,
              ev->xconfigure.height
            );
    }
    RET(GDK_FILTER_CONTINUE);
}

static void
gtk_bgbox_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    gint border_width;
    GtkBgboxPrivate *priv;

    ENTER;
    GtkAllocation* alloc = g_new(GtkAllocation, 1);
    gtk_widget_get_allocation(widget, alloc);

    //GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    gtk_widget_set_realized(widget, TRUE);

    //border_width = GTK_CONTAINER (widget)->border_width;
    border_width = gtk_container_get_border_width((GtkContainer *)widget);
    	

    attributes.x = alloc->x + border_width;
    attributes.y = alloc->y + border_width;
    attributes.width = alloc->width - 2 * border_width;
    attributes.height = alloc->height - 2 * border_width;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON_PRESS_MASK
        | GDK_BUTTON_RELEASE_MASK
        | GDK_ENTER_NOTIFY_MASK
        | GDK_LEAVE_NOTIFY_MASK
        | GDK_EXPOSURE_MASK
        | GDK_STRUCTURE_MASK;

    priv = GTK_BGBOX_GET_PRIVATE (widget);

    attributes.visual = gtk_widget_get_visual (widget);
//    attributes.colormap = gtk_widget_get_colormap (widget);
//gtk_widget_set_colormap
    attributes.wclass = GDK_INPUT_OUTPUT;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL ;//| GDK_WA_COLORMAP;

    gtk_widget_set_parent_window (widget, gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask) );

    gdk_window_set_user_data (gtk_widget_get_window(widget), widget);

    //gtk_widget_set_style(widget, gtk_style_attach(gtk_widget_get_style_context(widget), gtk_widget_get_window(widget)));
    //
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context, NULL, GTK_STYLE_PROVIDER_PRIORITY_USER);

    
    if (priv->bg_type == BG_NONE)
        gtk_bgbox_set_background(widget, BG_STYLE, 0, 0);

    gdk_window_add_filter(gtk_widget_get_window(widget), (GdkFilterFunc) gtk_bgbox_event_filter, widget);

    RET();
}


static void
gtk_bgbox_style_set (GtkWidget *widget, GtkStyle  *previous_style)
{
    GtkBgboxPrivate *priv;

    ENTER;
    priv = GTK_BGBOX_GET_PRIVATE (widget);
    if (gtk_widget_get_realized(widget) && !gtk_widget_get_has_window (widget)) {
        gtk_bgbox_set_background(widget, priv->bg_type, priv->tintcolor, priv->alpha);
    }
    RET();
}

/* gtk discards configure_event for GTK_WINDOW_CHILD. too pitty */
static  gboolean
gtk_bgbox_configure_event (GtkWidget *widget, GdkEventConfigure *e)
{
    ENTER;
    DBG("geom: size (%d, %d). pos (%d, %d)\n", e->width, e->height, e->x, e->y);
    RET(FALSE);

}
/*
static void
gtk_bgbox_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    GtkBin *bin = GTK_BIN (widget);
    ENTER;
    requisition->width = gtk_container_get_border_width(GTK_CONTAINER(widget)) * 2;
    requisition->height = gtk_container_get_border_width(GTK_CONTAINER(widget)) * 2;

    if (gtk_bin_get_child(bin) && gtk_widget_get_visible (gtk_bin_get_child(bin)))
    {
        GtkRequisition child_requisition;

        gtk_widget_size_request (gtk_bin_get_child(bin), &child_requisition);

        requisition->width += child_requisition.width;
        requisition->height += child_requisition.height;
    }
    RET();
}
*/
/* calls with same allocation are usually refer to exactly same background
 * and we just skip them for optimization reason.
 * so if you see artifacts or unupdated background - reallocate bg on every call
 */
static void
gtk_bgbox_size_allocate (GtkWidget *widget, GtkAllocation *wa)
{
    GtkBin *bin;
    GtkAllocation ca;
    GtkBgboxPrivate *priv;
    int same_alloc, border;

    GtkAllocation *allocation = g_new0 (GtkAllocation, 1);
    gtk_widget_get_allocation(GTK_WIDGET(widget), allocation); 

    ENTER;
    same_alloc = !memcmp(allocation, wa, sizeof(*wa));
    DBG("same alloc = %d\n", same_alloc);
    DBG("x=%d y=%d w=%d h=%d\n", wa->x, wa->y, wa->width, wa->height);
    DBG("x=%d y=%d w=%d h=%d\n", widget->allocation.x, widget->allocation.y,
          widget->allocation.width, widget->allocation.height);
    allocation = wa;
    bin = GTK_BIN (widget);
    border = gtk_container_get_border_width(GTK_CONTAINER (widget));
    ca.x = border;
    ca.y = border;
    ca.width  = MAX (wa->width  - border * 2, 0);
    ca.height = MAX (wa->height - border * 2, 0);

    if (gtk_widget_get_realized(widget) && !gtk_widget_get_has_window (widget) && !same_alloc) {
        priv = GTK_BGBOX_GET_PRIVATE (widget);
        DBG("move resize pos=%d,%d geom=%dx%d\n", wa->x, wa->y, wa->width, wa->height);
        gdk_window_move_resize (gtk_widget_get_window(widget), wa->x, wa->y, wa->width, wa->height);
        gtk_bgbox_set_background(widget, priv->bg_type, priv->tintcolor, priv->alpha);
    }

    if (gtk_bin_get_child(bin))
        gtk_widget_size_allocate (gtk_bin_get_child(bin), &ca);
    RET();
}


static void
gtk_bgbox_bg_changed(FbBg *bg, GtkWidget *widget)
{
    GtkBgboxPrivate *priv;

    ENTER;
    priv = GTK_BGBOX_GET_PRIVATE (widget);
    if (gtk_widget_get_realized(widget) && !gtk_widget_get_has_window (widget)) {
        gtk_bgbox_set_background(widget, priv->bg_type, priv->tintcolor, priv->alpha);
    }
    RET();
}

void
gtk_bgbox_set_background(GtkWidget *widget, int bg_type, guint32 tintcolor, gint alpha)
{
    GtkBgboxPrivate *priv;

    GtkAllocation *alloc = g_new0 (GtkAllocation, 1);
    gtk_widget_get_allocation(GTK_WIDGET(widget), alloc); 

    ENTER;
    if (!(GTK_IS_BGBOX (widget)))
        RET();

    priv = GTK_BGBOX_GET_PRIVATE (widget);
    cairo_t *cr = cairo_create (priv->surface);
    
    DBG("widget=%p bg_type old:%d new:%d\n", widget, priv->bg_type, bg_type);
    if (priv->surface) {
        g_object_unref(priv->surface);
        priv->surface = NULL;
    }
    priv->bg_type = bg_type;
    if (priv->bg_type == BG_STYLE) {
//        gtk_style_set_background(gtk_widget_get_style_context(widget), gtk_widget_get_window(widget), gdk_window_get_state(gtk_widget_get_window(widget)) );
	
	gtk_render_background(gtk_style_context_new(), cr, 0, 0, alloc->width, alloc->height);

        if (priv->sid) {
            g_signal_handler_disconnect(priv->bg, priv->sid);
            priv->sid = 0;
        }
        if (priv->bg) {
            g_object_unref(priv->bg);
            priv->bg = NULL;
        }
    } else {
        if (!priv->bg)
            priv->bg = fb_bg_get_for_display();
        if (!priv->sid)
            priv->sid = g_signal_connect(G_OBJECT(priv->bg), "changed", G_CALLBACK(gtk_bgbox_bg_changed), widget);

        if (priv->bg_type == BG_ROOT) {
            priv->tintcolor = tintcolor;
            priv->alpha = alpha;
            gtk_bgbox_set_bg_root(widget, priv);
        } else if (priv->bg_type == BG_INHERIT) {
            gtk_bgbox_set_bg_inherit(widget, priv);
        }
    }
    gtk_widget_queue_draw(widget);
    g_object_notify(G_OBJECT (widget), "style");

    DBG("queue draw all %p\n", widget);
    RET();
}

static void
gtk_bgbox_set_bg_root(GtkWidget *widget, GtkBgboxPrivate *priv)
{
    priv = GTK_BGBOX_GET_PRIVATE (widget);
	
    cairo_t *cr = cairo_create (priv->surface);
    GtkAllocation* alloc = g_new(GtkAllocation, 1);
    gtk_widget_get_allocation(widget, alloc);

    ENTER;
    priv->surface = fb_bg_get_xroot_pix_for_win(priv->bg, widget);
    if (!priv->surface /*|| priv->pixmap ==  GDK_NO_BG*/) {
        //priv->bg_type = BG_NONE;
        priv->surface = NULL;
//      gtk_style_set_background(widget->style, widget->window, widget->state);
        gtk_render_background(gtk_style_context_new(), cr, 0, 0, alloc->width, alloc->height);
        gtk_widget_queue_draw_area(widget, 0, 0, alloc->width, alloc->height);
        DBG("no root pixmap was found\n");
        RET();
    }
    if (priv->alpha)
        fb_bg_composite(gtk_widget_get_window(widget), priv->tintcolor, priv->alpha);

    //gdk_window_set_back_pixmap(gtk_widget_get_window(widget), priv->pixmap, FALSE);
    gdk_window_ensure_native(gtk_widget_get_window(widget));
    
    RET();
}

static void
gtk_bgbox_set_bg_inherit(GtkWidget *widget, GtkBgboxPrivate *priv)
{
    priv = GTK_BGBOX_GET_PRIVATE (widget);

    ENTER;
    
    //gdk_window_set_back_pixmap(gtk_widget_get_window(widget), NULL, TRUE);
    gdk_window_ensure_native(gtk_widget_get_window(widget));
    
    RET();
}

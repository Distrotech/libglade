/* -*- Mode: C; c-basic-offset: 4 -*-
 * libglade - a library for building interfaces from XML files at runtime
 * Copyright (C) 1998-2001  James Henstridge <james@daa.com.au>
 *
 * glade-xml.c: implementation of core public interface functions
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
 * Boston, MA  02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glade/glade-xml.h>
#include <glade/glade-init.h>
#include <glade/glade-build.h>
#include <glade/glade-private.h>
#include <gmodule.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtklabel.h>
#include <atk/atk.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#endif

static const gchar *glade_xml_tree_key     = "GladeXML::tree";
static GQuark       glade_xml_tree_id      = 0;
static const gchar *glade_xml_name_key     = "GladeXML::name";
static GQuark       glade_xml_name_id      = 0;
static const gchar *glade_xml_tooltips_key = "GladeXML::tooltips";
static GQuark       glade_xml_tooltips_id  = 0;


static void glade_xml_init(GladeXML *xml);
static void glade_xml_class_init(GladeXMLClass *class);

static GObjectClass *parent_class;
static void glade_xml_finalize(GObject *object);

static void glade_xml_build_interface(GladeXML *xml, GladeInterface *iface,
				      const char *root);

/**
 * glade_xml_get_type:
 *
 * Creates the typecode for the GladeXML object type.
 *
 * Returns: the typecode for the GladeXML object type.
 */
GType
glade_xml_get_type(void)
{
    static GType xml_type = 0;

    if (!xml_type) {
	static const GTypeInfo xml_info = {
	    sizeof(GladeXMLClass),
	    (GBaseInitFunc) NULL,
	    (GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) glade_xml_class_init,
	    (GClassFinalizeFunc) NULL,
	    NULL,

	    sizeof (GladeXML),
	    0, /* n_preallocs */
	    (GInstanceInitFunc) glade_xml_init,
	};

	xml_type = g_type_register_static(G_TYPE_OBJECT, "GladeXML",
					  &xml_info, 0);
    }
    return xml_type;
}

static void
glade_xml_class_init (GladeXMLClass *class)
{
    parent_class = g_type_class_peek_parent (class);

    G_OBJECT_CLASS(class)->finalize = glade_xml_finalize;

    glade_xml_tree_id = g_quark_from_static_string(glade_xml_tree_key);
    glade_xml_name_id = g_quark_from_static_string(glade_xml_name_key);
    glade_xml_tooltips_id = g_quark_from_static_string(glade_xml_tooltips_key);
}

static void
glade_xml_init (GladeXML *self)
{
    GladeXMLPrivate *priv;
	
    self->priv = priv = g_new0 (GladeXMLPrivate, 1);

    self->filename = NULL;

    priv->tree = NULL;
    priv->tooltips = gtk_tooltips_new();
    gtk_tooltips_enable(priv->tooltips);
    priv->name_hash = g_hash_table_new(g_str_hash, g_str_equal);
    priv->signals = g_hash_table_new(g_str_hash, g_str_equal);
    priv->radio_groups = g_hash_table_new (g_str_hash, g_str_equal);
    priv->toplevel = NULL;
    priv->accel_groups = NULL;
    priv->default_widget = NULL;
    priv->focus_widget = NULL;
    priv->deferred_props = NULL;
}

/**
 * glade_xml_new:
 * @fname: the XML file name.
 * @root: the widget node in @fname to start building from (or %NULL)
 * @domain: the translation domain for the XML file (or %NULL for default)
 *
 * Creates a new GladeXML object (and the corresponding widgets) from the
 * XML file @fname.  Optionally it will only build the interface from the
 * widget node @root (if it is not %NULL).  This feature is useful if you
 * only want to build say a toolbar or menu from the XML file, but not the
 * window it is embedded in.  Note also that the XML parse tree is cached
 * to speed up creating another GladeXML object for the same file
 *
 * Returns: the newly created GladeXML object, or NULL on failure.
 */
GladeXML *
glade_xml_new(const char *fname, const char *root, const char *domain)
{
    GladeXML *self = g_object_new(GLADE_TYPE_XML, NULL);

    if (!glade_xml_construct(self, fname, root, domain)) {
	g_object_unref(G_OBJECT(self));
	return NULL;
    }
    return self;
}

/**
 * glade_xml_construct:
 * @self: the GladeXML object
 * @fname: the XML filename
 * @root: the root widget node (or %NULL for none)
 * @domain: the translation domain (or %NULL for the default)
 *
 * This routine can be used by bindings (such as gtk--) to help construct
 * a GladeXML object, if it is needed.
 *
 * Returns: TRUE if the construction succeeded.
 */
gboolean
glade_xml_construct (GladeXML *self, const char *fname, const char *root,
		     const char *domain)
{
    GladeInterface *iface;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(fname != NULL, FALSE);

    iface = glade_parser_parse_file(fname, domain);

    if (!iface)
	return FALSE;

    self->priv->tree = iface;
    if (self->filename)
	g_free(self->filename);
    self->filename = g_strdup(fname);
    glade_xml_build_interface(self, iface, root);

    return TRUE;
}

/**
 * glade_xml_new_from_buffer:
 * @buffer: the memory buffer containing the XML document.
 * @size: the size of the buffer.
 * @root: the widget node in @buffer to start building from (or %NULL)
 * @domain: the translation domain to use for this interface (or %NULL)
 *
 * Creates a new GladeXML object (and the corresponding widgets) from the
 * buffer @buffer.  Optionally it will only build the interface from the
 * widget node @root (if it is not %NULL).  This feature is useful if you
 * only want to build say a toolbar or menu from the XML document, but not the
 * window it is embedded in.
 *
 * Returns: the newly created GladeXML object, or NULL on failure.
 */
GladeXML *
glade_xml_new_from_buffer(const char *buffer, int size, const char *root,
			  const char *domain)
{
    GladeXML *self;
    GladeInterface *iface = glade_parser_parse_buffer(buffer, size, domain);

    if (!iface)
	return NULL;
    self = g_object_new(GLADE_TYPE_XML, NULL);

    self->priv->tree = iface;
    self->filename = NULL;
    glade_xml_build_interface(self, iface, root);

    return self;
}

/**
 * glade_xml_signal_connect:
 * @self: the GladeXML object
 * @handlername: the signal handler name
 * @func: the signal handler function
 *
 * In the glade interface descriptions, signal handlers are specified for
 * widgets by name.  This function allows you to connect a C function to
 * all signals in the GladeXML file with the given signal handler name.
 */
void
glade_xml_signal_connect (GladeXML *self, const char *handlername,
			  GtkSignalFunc func)
{
    GList *signals;

    g_return_if_fail(self != NULL);
    g_return_if_fail(handlername != NULL);
    g_return_if_fail(func != NULL);

    signals = g_hash_table_lookup(self->priv->signals, handlername);
    for (; signals != NULL; signals = signals->next) {
	GladeSignalData *data = signals->data;

	if (data->connect_object) {
	    GtkObject *other = g_hash_table_lookup(self->priv->name_hash,
						   data->connect_object);
	    if (data->signal_after)
		gtk_signal_connect_object_after(data->signal_object,
						data->signal_name,
						func, other);
	    else
		gtk_signal_connect_object(data->signal_object,
					  data->signal_name, func, other);
	} else {
	    /* the signal_data argument is just a string, but may be
	     * helpful for someone */
	    if (data->signal_after)
		gtk_signal_connect_after(data->signal_object,
					 data->signal_name, func, NULL);
	    else
		gtk_signal_connect(data->signal_object, data->signal_name,
				   func, NULL);
	}
    }
}

static void
autoconnect_foreach(const char *signal_handler, GList *signals,
		    GModule *allsymbols)
{
    GtkSignalFunc func;

    if (!g_module_symbol(allsymbols, signal_handler, (gpointer *)&func))
	g_warning("could not find signal handler '%s'.", signal_handler);
    else
	for (; signals != NULL; signals = signals->next) {
	    GladeSignalData *data = signals->data;
	    if (data->connect_object) {
		GladeXML *self = glade_get_widget_tree(
					GTK_WIDGET(data->signal_object));
		GtkObject *other = g_hash_table_lookup(self->priv->name_hash,
						       data->connect_object);
		if (data->signal_after)
		    gtk_signal_connect_object_after(data->signal_object,
						    data->signal_name,
						    func, other);
		else
		    gtk_signal_connect_object(data->signal_object,
					      data->signal_name, func, other);
	    } else {
		/* the signal_data argument is just a string, but may
		 * be helpful for someone */
		if (data->signal_after)
		    gtk_signal_connect_after(data->signal_object,
					     data->signal_name, func, NULL);
		else
		    gtk_signal_connect(data->signal_object, data->signal_name,
				       func, NULL);
	    }
	}
}

/**
 * glade_xml_signal_autoconnect:
 * @self: the GladeXML object.
 *
 * This function is a variation of glade_xml_signal_connect.  It uses
 * gmodule's introspective features (by openning the module %NULL) to
 * look at the application's symbol table.  From here it tries to match
 * the signal handler names given in the interface description with
 * symbols in the application and connects the signals.
 * 
 * Note that this function will not work correctly if gmodule is not
 * supported on the platform.
 */
void
glade_xml_signal_autoconnect (GladeXML *self)
{
    GModule *allsymbols;

    g_return_if_fail(self != NULL);
    if (!g_module_supported())
	g_error("glade_xml_signal_autoconnect requires working gmodule");

    /* get a handle on the main executable -- use this to find symbols */
    allsymbols = g_module_open(NULL, 0);
    g_hash_table_foreach(self->priv->signals, (GHFunc)autoconnect_foreach,
			 allsymbols);
}


typedef struct {
    GladeXMLConnectFunc func;
    gpointer user_data;
} connect_struct;

static void
autoconnect_full_foreach(const char *signal_handler, GList *signals,
			 connect_struct *conn)
{
    GladeXML *self = NULL;

    for (; signals != NULL; signals = signals->next) {
	GladeSignalData *data = signals->data;
	GtkObject *connect_object = NULL;
		
	if (data->connect_object) {
	    if (!self)
		self = glade_get_widget_tree(GTK_WIDGET(data->signal_object));
	    connect_object = g_hash_table_lookup(self->priv->name_hash,
						 data->connect_object);
	}

	(* conn->func) (signal_handler, data->signal_object,
			data->signal_name, NULL,
			connect_object, data->signal_after,
			conn->user_data);
    }
}

/**
 * GladeXMLConnectFunc:
 * @handler_name: the name of the handler function to connect.
 * @object: the object to connect the signal to.
 * @signal_name: the name of the signal.
 * @signal_data: the string value of the signal data given in the XML file.
 * @connect_object: non NULL if gtk_signal_connect_object should be used.
 * @after: TRUE if the connection should be made with gtk_signal_connect_after.
 * @user_data: the user data argument.
 *
 * This is the signature of a function used to connect signals.  It is used
 * by the glade_xml_signal_connect_full and glade_xml_signal_autoconnect_full
 * functions.  It is mainly intented for interpreted language bindings, but
 * could be useful where the programmer wants more control over the signal
 * connection process.
 */

/**
 * glade_xml_signal_connect_full:
 * @self: the GladeXML object.
 * @handler_name: the name of the signal handler that we want to connect.
 * @func: the function to use to connect the signals.
 * @user_data: arbitrary data to pass to the connect function.
 *
 * This function is similar to glade_xml_signal_connect, except that it
 * allows you to give an arbitrary function that will be used for actually
 * connecting the signals.  This is mainly useful for writers of interpreted
 * language bindings, or applications where you need more control over the
 * signal connection process.
 */
void
glade_xml_signal_connect_full(GladeXML *self, const gchar *handler_name,
			      GladeXMLConnectFunc func, gpointer user_data)
{
    connect_struct conn;
    GList *signals;

    g_return_if_fail(self != NULL);
    g_return_if_fail(handler_name != NULL);
    g_return_if_fail (func != NULL);

    /* rather than rewriting the code from the autoconnect_full
     * version, just reuse its helper function */
    conn.func = func;
    conn.user_data = user_data;
    signals = g_hash_table_lookup(self->priv->signals, handler_name);
    autoconnect_full_foreach(handler_name, signals, &conn);
}

/**
 * glade_xml_signal_autoconnect_full:
 * @self: the GladeXML object.
 * @func: the function used to connect the signals.
 * @user_data: arbitrary data that will be passed to the connection function.
 *
 * This function is similar to glade_xml_signal_connect_full, except that it
 * will try to connect all signals in the interface, not just a single
 * named handler.  It can be thought of the interpeted language binding
 * version of glade_xml_signal_autoconnect, except that it does not
 * require gmodule to function correctly.
 */
void
glade_xml_signal_autoconnect_full (GladeXML *self, GladeXMLConnectFunc func,
				   gpointer user_data)
{
    connect_struct conn;

    g_return_if_fail(self != NULL);
    g_return_if_fail (func != NULL);

    conn.func = func;
    conn.user_data = user_data;
    g_hash_table_foreach(self->priv->signals,
			 (GHFunc)autoconnect_full_foreach, &conn);
}

/**
 * glade_xml_signal_connect_data:
 * @self: the GladeXML object
 * @handlername: the signal handler name
 * @func: the signal handler function
 * @user_data: the signal handler data
 *
 * In the glade interface descriptions, signal handlers are specified for
 * widgets by name.  This function allows you to connect a C function to
 * all signals in the GladeXML file with the given signal handler name.
 *
 * It differs from glade_xml_signal_connect since it allows you to
 * specify the data parameter for the signal handler.  It is also a small
 * demonstration of how to use glade_xml_signal_connect_full.
 */
typedef struct {
    GtkSignalFunc func;
    gpointer user_data;
} connect_data_data;

static void
connect_data_connect_func(const gchar *handler_name, GtkObject *object,
			  const gchar *signal_name, const gchar *signal_data,
			  GtkObject *connect_object, gboolean after,
			  gpointer user_data)
{
    connect_data_data *data = (connect_data_data *)user_data;

    if (connect_object) {
	if (after)
	    gtk_signal_connect_object_after(object, signal_name,
					    data->func, connect_object);
	else
	    gtk_signal_connect_object(object, signal_name,
				      data->func, connect_object);
    } else {
	if (after)
	    gtk_signal_connect_after(object, signal_name,
				     data->func, data->user_data);
	else
	    gtk_signal_connect(object, signal_name,
			       data->func, data->user_data);
    }
}

void
glade_xml_signal_connect_data (GladeXML *self, const char *handlername,
			       GtkSignalFunc func, gpointer user_data)
{
    connect_data_data data;

    data.func = func;
    data.user_data = user_data;

    glade_xml_signal_connect_full(self, handlername,
				  connect_data_connect_func, &data);
}

/**
 * glade_xml_get_widget:
 * @self: the GladeXML object.
 * @name: the name of the widget.
 *
 * This function is used to get a pointer to the GtkWidget corresponding to
 * @name in the interface description.  You would use this if you have to do
 * anything to the widget after loading.
 *
 * Returns: the widget matching @name, or %NULL if none exists.
 */
GtkWidget *
glade_xml_get_widget (GladeXML *self, const char *name)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    return g_hash_table_lookup(self->priv->name_hash, name);
}


/**
 * glade_xml_get_widget_prefix:
 * @self: the GladeXML object.
 * @name: the name of the widget.
 *
 * This function is used to get a list of pointers to the GtkWidget(s)
 * with names that start with the string @name in the interface description.
 * You would use this if you have to do something  to all of these widgets
 * after loading.
 *
 * Returns: A list of the widget that match @name as the start of their
 * name, or %NULL if none exists.
 */
typedef struct {
    const gchar *name;
    GList *list;
} widget_prefix_data;

static void
widget_prefix_add_to_list (gchar *name, gpointer value,
			   widget_prefix_data *data)
{
    if (!strncmp (data->name, name, strlen (data->name)))
	data->list = g_list_prepend (data->list, value);
}

GList *
glade_xml_get_widget_prefix (GladeXML *self, const gchar *prefix)
{
    widget_prefix_data data;

    data.name = prefix;
    data.list = NULL;

    g_hash_table_foreach (self->priv->name_hash,
			  (GHFunc) widget_prefix_add_to_list, &data);

    return data.list;
}

/**
 * glade_xml_relative_file
 * @self: the GladeXML object.
 * @filename: the filename.
 *
 * This function resolves a relative pathname, using the directory of the
 * XML file as a base.  If the pathname is absolute, then the original
 * filename is returned.
 *
 * Returns: the filename.  The result must be g_free'd.
 */
gchar *
glade_xml_relative_file(GladeXML *self, const gchar *filename)
{
    gchar *dirname, *tmp;

    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(filename != NULL, NULL);

    if (g_path_is_absolute(filename)) /* an absolute pathname */
	return g_strdup(filename);
    /* prepend XML file's dir onto filename */
    dirname = g_dirname(self->filename);	
    tmp = g_strconcat(dirname, G_DIR_SEPARATOR_S, filename, NULL);
    g_free(dirname);
    return tmp;
}

/**
 * glade_get_widget_name:
 * @widget: the widget
 *
 * Used to get the name of a widget that was generated by a GladeXML object.
 *
 * Returns: the name of the widget.
 */
const char *
glade_get_widget_name(GtkWidget *widget)
{
    g_return_val_if_fail(widget != NULL, NULL);

    return (const char *)g_object_get_qdata(G_OBJECT(widget),
					    glade_xml_name_id);
}

/**
 * glade_get_widget_tree:
 * @widget: the widget
 *
 * Description:
 * This function is used to get the GladeXML object that built this widget.
 *
 * Returns: the GladeXML object that built this widget.
 */
GladeXML *
glade_get_widget_tree(GtkWidget *widget)
{
    g_return_val_if_fail(widget != NULL, NULL);

    return g_object_get_qdata(G_OBJECT(widget), glade_xml_tree_id);
}

/* ------------------------------------------- */


/**
 * glade_xml_set_toplevel:
 * @xml: the GladeXML object.
 * @window: the toplevel.
 *
 * This is used while the tree is being built to set the toplevel window that
 * is currently being built.  It is mainly used to enable GtkAccelGroup's to
 * be bound to the correct window, but could have other uses.
 */
void
glade_xml_set_toplevel(GladeXML *xml, GtkWindow *window)
{
    if (xml->priv->focus_widget)
	gtk_widget_grab_focus(xml->priv->focus_widget);
    if (xml->priv->default_widget)
	gtk_widget_grab_default(xml->priv->default_widget);
    xml->priv->focus_widget = NULL;
    xml->priv->default_widget = NULL;
    xml->priv->toplevel = window;
    /* new toplevel needs new accel group */
    if (xml->priv->accel_groups)
	glade_xml_pop_accel(xml);
    /* maybe put an assert that xml->priv->accel_groups == NULL here? */
    xml->priv->accel_groups = NULL;
    /* the window should hold a reference to the tooltips object */
    gtk_object_ref(GTK_OBJECT(xml->priv->tooltips));
    g_object_set_qdata_full(G_OBJECT(window), glade_xml_tooltips_id,
			    xml->priv->tooltips,
			    (GDestroyNotify)gtk_object_unref);
}

/**
 * glade_xml_handle_widget_prop
 * @xml: the GladeXML object
 * @widget: the property the widget to set the property on.
 * @prop_name: the name of the property.
 * @value_name: the name of the widget used as the value for the property.
 *
 * Some widgets have properties of type GtkWidget.  These are
 * represented as the widget name in the glade file.  When
 * constructing the interface, the widget specified as the value for a
 * property may not exist yet.
 *
 * Rather than setting the property directly, this function should be
 * used.  It will perform the name to GtkWidget conversion, and if the
 * widget is yet to be constructed, defer setting the property until
 * the widget is constructed.
 */
void
glade_xml_handle_widget_prop(GladeXML *self, GtkWidget *widget,
			     const gchar *prop_name, const gchar *value_name)
{
    GtkWidget *value_widget;

    g_return_if_fail(GLADE_IS_XML(self));

    value_widget = g_hash_table_lookup(self->priv->name_hash, value_name);
    if (value_widget) {
	g_object_set(G_OBJECT(widget), prop_name, value_widget, NULL);
    } else {
	GladeDeferredProperty *dprop = g_new(GladeDeferredProperty, 1);

	dprop->target_name = value_name;
	dprop->type = DEFERRED_PROP;
	dprop->d.prop.object = G_OBJECT(widget);
	dprop->d.prop.prop_name = prop_name;

	self->priv->deferred_props = g_list_prepend(self->priv->deferred_props,
						    dprop);
    }
}

/**
 * glade_xml_push_accel:
 * @xml: the GladeXML object.
 *
 * Make a new accel group amd push onto the stack.  This is intended for use
 * by GtkNotebook so that each notebook page can set up its own accelerators.
 * Returns: The current GtkAccelGroup after push.
 */
GtkAccelGroup *
glade_xml_push_accel(GladeXML *xml)
{
    GtkAccelGroup *accel = gtk_accel_group_new();
       
    xml->priv->accel_groups = g_slist_prepend(xml->priv->accel_groups, accel);
    return accel;
}

/**
 * glade_xml_pop_accel:
 * @xml: the GladeXML object.
 *
 * Pops the accel group.  This will usually be called after a GtkNotebook page
 * has been built.
 * Returns: The current GtkAccelGroup after pop.
 */
GtkAccelGroup *
glade_xml_pop_accel(GladeXML *xml)
{
    GtkAccelGroup *accel;

    g_return_val_if_fail(xml->priv->accel_groups != NULL, NULL);

    /* the accel group at the top of the stack */
    accel = xml->priv->accel_groups->data;
    xml->priv->accel_groups = g_slist_remove(xml->priv->accel_groups, accel);
    /* unref the accel group that was at the top of the stack */
    gtk_accel_group_unref(accel);
    return xml->priv->accel_groups ? xml->priv->accel_groups->data : NULL;
}

/**
 * glade_xml_ensure_accel:
 * @xml: the GladeXML object.
 *
 * This function is used to get the current GtkAccelGroup.  If there isn't
 * one, a new one is created and bound to the current toplevel window (if
 * a toplevel has been set).
 * Returns: the current GtkAccelGroup.
 */
GtkAccelGroup *
glade_xml_ensure_accel(GladeXML *xml)
{
    if (!xml->priv->accel_groups) {
	glade_xml_push_accel(xml);
	if (xml->priv->toplevel)
	    gtk_window_add_accel_group(xml->priv->toplevel,
				       xml->priv->accel_groups->data);
    }
    return (GtkAccelGroup *)xml->priv->accel_groups->data;
}

/* this is a private function */
static void
glade_xml_add_signals(GladeXML *xml, GtkWidget *w, GladeWidgetInfo *info)
{
    gint i;

    for (i = 0; i < info->n_signals; i++) {
	GladeSignalInfo *sig = &info->signals[i];
	GladeSignalData *data = g_new0(GladeSignalData, 1);
	GList *list;

	data->signal_object = GTK_OBJECT(w);
	data->signal_name = sig->name;
	data->connect_object = sig->object;
	data->signal_after = sig->after;

	list = g_hash_table_lookup(xml->priv->signals, sig->handler);
	list = g_list_prepend(list, data);
	g_hash_table_insert(xml->priv->signals, sig->handler, list);
    }
}

/* this is a private function */
static void
glade_xml_add_atk_actions(GladeXML *xml, GtkWidget *w, GladeWidgetInfo *info)
{
    gint i;
    gint n;
    AtkObject *atko = gtk_widget_get_accessible (w);
    AtkAction *action;

    if (info->n_atk_actions) {
        if (!ATK_IS_ACTION (atko)) {
            g_warning("widgets of type %s don't have actions, but one is specified",
	  	  G_OBJECT_TYPE_NAME (w));
	    return;
        }

        action = ATK_ACTION (atko);
        n = atk_action_get_n_actions (action);    
    
        for (i = 0; i < info->n_atk_actions; i++) {
    	    GladeAtkActionInfo *action_info = &info->atk_actions[i];
	    int j;
	    for (j=0; j<n; ++j) {
	        if (strcmp (atk_action_get_name (action, j),
			    action_info->action_name) == 0) {
	            break;
	        }
	    }
	    if (j < n) {
	        atk_action_set_description (ATK_ACTION (atko),
					    j,
					    action_info->description);	
	    } else {
		    g_warning("could not find action named %s", action_info->action_name);
	    }
        }
    }	
}

/* helper function for adding relations */
static void
add_relation(AtkRelationSet *relations, AtkRelationType relation_type,
	     AtkObject *target_accessible)
{
    AtkRelation *relation;

    relation = atk_relation_set_get_relation_by_type (relations,
						      relation_type);
    if (relation) {
	/* add new target accessible to relation */
	GPtrArray* target_array = atk_relation_get_target (relation);

	g_ptr_array_remove (target_array, target_accessible);
	g_ptr_array_add (target_array, target_accessible);
    } else {
	/* the relation hasn't been created yet ... */
	atk_relation_set_add (relations,
			      atk_relation_new (&target_accessible, 1,
						relation_type));
    }
}

/* this is a private function */
static void
glade_xml_add_atk_relations(GladeXML *xml, GtkWidget *w, GladeWidgetInfo *info)
{
    gint i;
    AtkObject *atko = gtk_widget_get_accessible (w);
    AtkRelationSet *relations = atk_object_ref_relation_set (atko);
    
    for (i = 0; i < info->n_relations; i++) {
	GladeAtkRelationInfo *rinfo = &info->relations[i];
	GtkWidget *target_widget = glade_xml_get_widget (xml, rinfo->target);
	AtkRelationType relation_type;

	relation_type = atk_relation_type_from_string (rinfo->type);
	if (target_widget) {
	    AtkObject *target_accessible;

	    target_accessible = gtk_widget_get_accessible (target_widget);

	    add_relation(relations, relation_type, target_accessible);
	} else {
	    GladeDeferredProperty *dprop = g_new(GladeDeferredProperty, 1);

	    dprop->target_name = rinfo->target;
	    dprop->type = DEFERRED_REL;
	    dprop->d.rel.relation_set = g_object_ref(relations);
	    dprop->d.rel.relation_type = relation_type;

	    xml->priv->deferred_props =
		g_list_prepend(xml->priv->deferred_props, dprop);
	}
    }
    g_object_unref (relations);
}

static void
glade_xml_add_accels(GladeXML *xml, GtkWidget *w, GladeWidgetInfo *info)
{
    gint i;
    for (i = 0; i < info->n_accels; i++) {
	GladeAccelInfo *accel = &info->accels[i];

	debug(g_message("New Accel: key=%d,mod=%d -> %s:%s",
			accel->key, accel->modifiers,
			gtk_widget_get_name(w), accel->signal));
	gtk_widget_add_accelerator(w, accel->signal,
				   glade_xml_ensure_accel(xml),
				   accel->key, accel->modifiers,
				   GTK_ACCEL_VISIBLE);
    }
}

static void
glade_xml_add_accessibility_info(GladeXML *xml, GtkWidget *w, GladeWidgetInfo *info)
{
    gint i;
    AtkObject *atko = gtk_widget_get_accessible (w);
    
    for (i = 0; i < info->n_atk_props; i++) {
	GParamSpec *pspec;
	GValue value = { 0 };

	pspec = g_object_class_find_property(g_type_class_ref(ATK_TYPE_OBJECT),
					     info->atk_props[i].name);
	if (!pspec) {
	    g_warning("unknown property `%s' for class `%s'",
		      info->atk_props[i].name, g_type_name(ATK_TYPE_OBJECT));
	    continue;
	} else if (glade_xml_set_value_from_string (xml, pspec, info->atk_props[i].value, &value)) {
	    g_object_set_property (G_OBJECT (atko), info->atk_props[i].name, &value);
	    g_value_unset (&value);
        }
	
	debug(g_message("Adding accessibility property %s:%s",
			info->atk_props[i].name,
			info->atk_props[i].value));
    }

    /* Hang on, we're not done yet ;-) */
    glade_xml_add_atk_actions(xml, w, info);
    glade_xml_add_atk_relations(xml, w, info);
    /*
       hmm, might have to defer some relations,
       if the widgets they refer to haven't
       been constructed yet (will look at the other prop-as-widget
       stuff for ideas)
    */
}

static void
glade_xml_destroy_signals(char *key, GList *signal_datas)
{
    GList *tmp;

    for (tmp = signal_datas; tmp; tmp = tmp->next) {
	GladeSignalData *data = tmp->data;
	g_free(data);
    }
    g_list_free(signal_datas);
}

static void
free_radio_groups(gpointer key, gpointer value, gpointer user_data)
{
    g_free(key);  /* the string name */
}

static void
remove_data_func(gpointer key, gpointer value, gpointer user_data)
{
    GObject *object = value;

    g_object_set_qdata(object, glade_xml_tree_id, NULL);
    g_object_set_qdata(object, glade_xml_name_id, NULL);
}

static void
glade_xml_finalize(GObject *object)
{
    GladeXML *self = GLADE_XML(object);
    GladeXMLPrivate *priv = self->priv;
	
    g_free(self->filename);
    self->filename = NULL;

    if (priv) {
	/* remove data items from all the widgets that are still
         * live. */
	g_hash_table_foreach(priv->name_hash, remove_data_func, self);
	/* strings are owned in the GladeInterface structure */
	g_hash_table_destroy(priv->name_hash);

	g_hash_table_foreach(priv->signals,
			     (GHFunc)glade_xml_destroy_signals, NULL);
	g_hash_table_destroy(priv->signals);

	/* the group name strings are owned by the GladeWidgetTree */
	g_hash_table_foreach(self->priv->radio_groups,
			     free_radio_groups, NULL);
	g_hash_table_destroy(priv->radio_groups);

	if (priv->tooltips)
	    gtk_object_unref(GTK_OBJECT(priv->tooltips));

	/* there should only be at most one accel group on stack */
	if (priv->accel_groups)
	    glade_xml_pop_accel(self);

	if (priv->tree)
	    glade_interface_destroy(priv->tree);

	g_free (self->priv);
    }
    self->priv = NULL;
    if (parent_class->finalize)
	(* parent_class->finalize)(object);
}

/**
 * glade_set_custom_handler
 * @handler: the custom widget handler
 * @user_data: user data passed to the custom handler
 *
 * Calling this function allows you to override the default behaviour
 * when a Custom widget is found in an interface.  This could be used by
 * a language binding to call some other function, or to limit what
 * functions can be called to create custom widgets.
 */
static GtkWidget *
default_custom_handler(GladeXML *xml, gchar *func_name, gchar *name,
		       gchar *string1, gchar *string2, gint int1, gint int2,
		       gpointer user_data)
{
    typedef GtkWidget *(* create_func)(gchar *name,
				       gchar *string1, gchar *string2,
				       gint int1, gint int2);
    GModule *allsymbols;
    create_func func;

    if (!g_module_supported()) {
	g_error("custom_new requires gmodule to work correctly");
	return NULL;
    }
    allsymbols = g_module_open(NULL, 0);
    if (g_module_symbol(allsymbols, func_name, (gpointer)&func))
	return (* func)(name, string1, string2, int1, int2);
    g_warning("could not find widget creation function");
    return NULL;
}

static GladeXMLCustomWidgetHandler custom_handler = default_custom_handler;
static gpointer custom_user_data = NULL;

void
glade_set_custom_handler(GladeXMLCustomWidgetHandler handler,
			 gpointer user_data)
{
    custom_handler = handler;
    custom_user_data = user_data;
}

/**
 * glade_set_custom_handler
 * @xml: the GladeXML object
 * @func_name: the name of the widget creation function
 * @name: the name of the widget
 * @string1: the first string argument
 * @string2: the second string argument
 * @int1: the first integer argument
 * @int2: the second integer argument
 *
 * Invokes the custom widget creation function.
 * Returns: the new widget or NULL on error.
 */
GtkWidget *
glade_create_custom(GladeXML *xml, gchar *func_name, gchar *name,
		    gchar *string1, gchar *string2, gint int1, gint int2)
{
    return (* custom_handler)(xml, func_name, name, string1, string2,
				  int1, int2, custom_user_data);
}

/**
 * glade_enum_from_string
 * @type: the GType for this enum type.
 * @string: the string representation of the enum value.
 *
 * This helper routine is designed to be used by widget build routines to
 * convert the string representations of enumeration values found in the
 * XML descriptions to the integer values that can be used to configure
 * the widget.
 *
 * Returns: the integer value for this enumeration, or 0 if it couldn't be
 * found.
 */
gint
glade_enum_from_string (GType type, const char *string)
{
    GEnumClass *eclass;
    GEnumValue *ev;
    gchar *endptr;
    gint ret = 0;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    eclass = g_type_class_ref(type);
    ev = g_enum_get_value_by_name(eclass, string);
    if (!ev) ev = g_enum_get_value_by_nick(eclass, string);
    if (ev)  ret = ev->value;

    g_type_class_unref(eclass);

    return ret;
}

/**
 * glade_flags_from_string
 * @type: the GType for this flags type.
 * @string: the string representation of the flags value.
 *
 * This helper routine is designed to be used by widget build routines
 * to convert the string representations of flags values found in the
 * XML descriptions to the integer values that can be used to
 * configure the widget.  The string is composed of string names or
 * nicknames for various flags separated by '|'.
 *
 * Returns: the integer value for this flags string
 */
guint
glade_flags_from_string (GType type, const char *string)
{
    GFlagsClass *fclass;
    gchar *endptr;
    guint i, j, ret = 0;
    char *flagstr;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    fclass = g_type_class_ref(type);


    flagstr = g_strdup (string);
    for (ret = i = j = 0; ; i++) {
	gboolean eos;

	eos = flagstr [i] == '\0';
	
	if (eos || flagstr [i] == '|') {
	    GFlagsValue *fv;
	    const char  *flag;

	    flag = &flagstr [j];

	    if (!eos) {
		flagstr [i++] = '\0';
		j = i;
	    }

	    fv = g_flags_get_value_by_name (fclass, flag);

	    if (!fv)
		fv = g_flags_get_value_by_nick (fclass, flag);

	    if (fv)
		ret |= fv->value;

	    if (eos)
		break;
	} else
	    i++;
    }
    
    g_free (flagstr);

    g_type_class_unref(fclass);

    return ret;
}

static void
glade_xml_build_interface(GladeXML *self, GladeInterface *iface,
			  const char *root)
{
    gint i;
    GladeWidgetInfo *wid;
    GtkWidget *w;

    /* make sure required modules are loaded */
    for (i = 0; i < iface->n_requires; i++)
	glade_require(iface->requires[i]);

    if (root) {
	wid = g_hash_table_lookup(iface->names, root);
	g_return_if_fail(wid != NULL);
	w = glade_xml_build_widget(self, wid);
	if (GTK_IS_WINDOW(w)) {
	    if (self->priv->focus_widget)
		gtk_widget_grab_focus(self->priv->focus_widget);
	    if (self->priv->default_widget)
		gtk_widget_grab_default(self->priv->default_widget);
	}
    } else {
	/* build all toplevel nodes */
	for (i = 0; i < iface->n_toplevels; i++) {
	    w = glade_xml_build_widget(self, iface->toplevels[i]);
	}
	if (self->priv->focus_widget)
	    gtk_widget_grab_focus(self->priv->focus_widget);
	if (self->priv->default_widget)
	    gtk_widget_grab_default(self->priv->default_widget);
    }
}

/* below are functions from glade-build.h */

static GHashTable *widget_table = NULL;

/**
 * glade_register_widgets:
 * @widgets: a NULL terminated array of GladeWidgetBuildData structures.
 *
 * This function is used to register new sets of widget building functions.
 * each member of @widgets contains the widget name, a function to build
 * a widget of that type, and optionally a function to build the children
 * of this widget.  The child building routine would call
 * glade_xml_build_widget on each child node to create the child before
 * packing it.
 *
 * This function is mainly useful for addon widget modules for libglade
 * (it would get called from the glade_init_module function).
 */
void
glade_register_widgets(const GladeWidgetBuildData *widgets)
{
    int i = 0;

    if (!widget_table)
	widget_table = g_hash_table_new(g_str_hash, g_str_equal);
    while (widgets[i].name != NULL) {
	g_hash_table_insert(widget_table, widgets[i].name,
			    (gpointer)&widgets[i]);
	i++;
    }
}

/**
 * glade_xml_set_value_from_string
 * @xml: the GladeXML object.
 * @pspec: the GParamSpec for the property
 * @string: the string representation of the value.
 * @value: the GValue to store the result in.
 *
 * This function demarshals a value from a string.  This function
 * calls g_value_init() on the @value argument, so it need not be
 * initialised beforehand.
 *
 * This function can handle char, uchar, boolean, int, uint, long,
 * ulong, enum, flags, float, double, string, GdkColor and
 * GtkAdjustment type values.  Support for GtkWidget type values is
 * still to come.
 *
 * Returns: %TRUE on success.
 */
gboolean
glade_xml_set_value_from_string (GladeXML *xml,
				 GParamSpec *pspec,
				 const gchar *string,
				 GValue *value)
{
    GType prop_type;
    gboolean ret = TRUE;

    prop_type = G_PARAM_SPEC_VALUE_TYPE(pspec);
    g_value_init(value, prop_type);
    switch (G_TYPE_FUNDAMENTAL(prop_type)) {
    case G_TYPE_CHAR:
	g_value_set_char(value, string[0]);
	break;
    case G_TYPE_UCHAR:
	g_value_set_uchar(value, (guchar)string[0]);
	break;
    case G_TYPE_BOOLEAN:
	g_value_set_boolean(value, string[0] == 'T' || string[0] == 'y');
	break;
    case G_TYPE_INT:
	g_value_set_int(value, strtol(string, NULL, 0));
	break;
    case G_TYPE_UINT:
	g_value_set_uint(value, strtoul(string, NULL, 0));
	break;
    case G_TYPE_LONG:
	g_value_set_long(value, strtol(string, NULL, 0));
	break;
    case G_TYPE_ULONG:
	g_value_set_ulong(value, strtoul(string, NULL, 0));
	break; 
    case G_TYPE_ENUM:
	g_value_set_enum(value, glade_enum_from_string(prop_type, string));
	break;
    case G_TYPE_FLAGS:
	g_value_set_flags(value, glade_flags_from_string(prop_type, string));
	break;
    case G_TYPE_FLOAT:
	g_value_set_float(value, g_strtod(string, NULL));
	break;
    case G_TYPE_DOUBLE:
	g_value_set_double(value, g_strtod(string, NULL));
	break;
    case G_TYPE_STRING:
	g_value_set_string(value, string);
	break;
    case G_TYPE_BOXED:
	if (G_VALUE_HOLDS(value, GDK_TYPE_COLOR)) {
	    GdkColor colour = { 0, };

	    if (gdk_color_parse(string, &colour) &&
		gdk_colormap_alloc_color(gtk_widget_get_default_colormap(),
					 &colour, FALSE, TRUE)) {
		g_value_set_boxed(value, &colour);
	    } else {
		g_warning ("could not parse colour name `%s'", string);
		ret = FALSE;
	    }
	} else
	    ret = FALSE;
	break;
    case G_TYPE_OBJECT:
	if (G_VALUE_HOLDS(value, GTK_TYPE_ADJUSTMENT)) {
	    GtkAdjustment *adj =
		GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 10, 10));
	    gchar *ptr = (gchar *)string;

	    adj->value = g_strtod(ptr, &ptr);
	    adj->lower = g_strtod(ptr, &ptr);
	    adj->upper = g_strtod(ptr, &ptr);
	    adj->step_increment = g_strtod(ptr, &ptr);
	    adj->page_increment = g_strtod(ptr, &ptr);
	    adj->page_size = g_strtod(ptr, &ptr);

	    g_value_set_object(value, adj);
	    gtk_object_sink(GTK_OBJECT(adj));
	} else if (G_VALUE_HOLDS(value, GDK_TYPE_PIXBUF)) {
	    gchar *filename;
	    GError *error = NULL;
	    GdkPixbuf *pixbuf;

	    filename = glade_xml_relative_file(xml, string);
	    pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	    if (pixbuf) {
		g_value_set_object(value, pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
	    } else {
		g_warning("Error loading image: %s", error->message);
		g_error_free(error);
		ret = FALSE;
	    }
	    g_free(filename);
	} else
	    ret = FALSE;
	break;
    default:
	ret = FALSE;
	break;
    }

    if (!ret) {
	g_warning("unsupported type `%s' for property `%s'",
		  g_type_name(prop_type), pspec->name);
	g_value_unset(value);
    }
    return ret;
}

/**
 * glade_standard_build_widget
 * @xml: the GladeXML object.
 * @widget_type: the GType of the widget.
 * @info: the GladeWidgetInfo structure.
 *
 * This is the standard widget building function.  It processes all
 * the widget properties using the standard object properties
 * interfaces.  This function will be sufficient for most widget
 * types, thus reducing the ammount of work needed to wrap a library.
 *
 * Returns: the constructed widget.
 */
GtkWidget *
glade_standard_build_widget(GladeXML *xml, GType widget_type,
			    GladeWidgetInfo *info)
{
    static GArray *props_array = NULL;
    GObjectClass *oclass;
    GtkWidget *widget;
    GList *deferred_props = NULL, *tmp;
    guint i;

    if (!props_array)
	props_array = g_array_new(FALSE, FALSE, sizeof(GParameter));

    /* we ref the class here as a slight optimisation */
    oclass = g_type_class_ref(widget_type);

    /* collect properties */
    for (i = 0; i < info->n_properties; i++) {
	GParameter param = { NULL };
	GParamSpec *pspec;

	pspec = g_object_class_find_property(oclass, info->properties[i].name);
	if (!pspec) {
	    g_warning("unknown property `%s' for class `%s'",
		      info->properties[i].name, g_type_name(widget_type));
	    continue;
	}

	/* this should catch all properties wanting a GtkWidget
         * subclass.  We also look for types that could hold a
         * GtkWidget in order to catch things like the
         * GtkAccelLabel::accel_object property.  Since we don't do
         * any handling of GObject or GtkObject directly in
         * glade_xml_set_value_from_string, this shouldn't be a
         * problem. */
	if (g_type_is_a(GTK_TYPE_WIDGET, G_PARAM_SPEC_VALUE_TYPE(pspec)) ||
	    g_type_is_a(G_PARAM_SPEC_VALUE_TYPE(pspec), GTK_TYPE_WIDGET)) {
	    deferred_props = g_list_prepend(deferred_props,
					    &info->properties[i]);
	    continue;
	}

	if (glade_xml_set_value_from_string(xml, pspec,
					    info->properties[i].value,
					    &param.value)) {
	    param.name = info->properties[i].name;
	    g_array_append_val(props_array, param);
	}
    }
    widget = g_object_newv(widget_type, props_array->len,
			   (GParameter *)props_array->data);

    /* clean up props_array */
    for (i = 0; i < props_array->len; i++) {
	g_array_index(props_array, GParameter, i).name = NULL;
	g_value_unset(&g_array_index(props_array, GParameter, i).value);
    }

    /* handle deferred properties */
    for (tmp = deferred_props; tmp; tmp = tmp->next) {
	GladeProperty *prop = tmp->data;

	glade_xml_handle_widget_prop(xml, widget, prop->name, prop->value);
    }
    g_list_free(tmp);

    g_array_set_size(props_array, 0);
    g_type_class_unref(oclass);

    return widget;
}

/**
 * glade_xml_set_packing_property:
 * @self: the GladeXML object.
 * @parent: the container widget.
 * @child: the contained child
 * @name: the name of the property
 * @value: it's stringified value
 * 
 * This sets the packing property on container @parent of widget
 * @child with @name to @value
 **/
static void
glade_xml_set_packing_property (GladeXML   *self,
				GtkWidget  *parent, GtkWidget  *child,
				const char *name,   const char *value)
{
    GValue gvalue = { 0 };
    GParamSpec *pspec;

    pspec = gtk_container_class_find_child_property(
	G_OBJECT_GET_CLASS(parent), name);
    if (!pspec)
	g_warning("unknown child property `%s' for container `%s'",
		  name, G_OBJECT_TYPE_NAME(parent));
    else if (glade_xml_set_value_from_string(self, pspec, value, &gvalue)) {
	gtk_container_child_set_property(GTK_CONTAINER(parent), child,
					 name, &gvalue);
	g_value_unset(&gvalue);
    }
}

/**
 * glade_standard_build_children
 * @self: the GladeXML object.
 * @parent: the container widget.
 * @info: the GladeWidgetInfo structure.
 *
 * This is the standard child building function.  It simply calls
 * gtk_container_add on each child to add them to the parent, and
 * process any packing properties using the generic container packing
 * properties interfaces.
 *
 * This function will be sufficient for most container widgets
 * provided that they implement the GtkContainer child packing
 * properties interfaces.
 */
void
glade_standard_build_children(GladeXML *self, GtkWidget *parent,
			      GladeWidgetInfo *info)
{
    gint i, j;

    g_object_ref(G_OBJECT(parent));
    for (i = 0; i < info->n_children; i++) {
	GladeWidgetInfo *childinfo = info->children[i].child;
	GtkWidget *child;

	/* handle any internal children */
	if (info->children[i].internal_child) {
	    glade_xml_handle_internal_child(self, parent, &info->children[i]);
	    continue;
	}

	child = glade_xml_build_widget(self, childinfo);

	g_object_ref(G_OBJECT(child));
	gtk_widget_freeze_child_notify(child);

	gtk_container_add(GTK_CONTAINER(parent), child);

	for (j = 0; j < info->children[i].n_properties; j++)
	    glade_xml_set_packing_property (
		self, parent, child,
		info->children[i].properties[j].name,
		info->children[i].properties[j].value);

	gtk_widget_thaw_child_notify(child);
	g_object_unref(G_OBJECT(child));
    }
    g_object_unref(G_OBJECT(parent));
}

#ifndef ENABLE_NLS
/* a slight optimisation when gettext is off */
#define glade_xml_gettext(xml, msgid) (msgid)
#endif

/**
 * GladeNewFunc
 * @xml: The GladeXML object.
 * @widget_type: the GType code of the widget.
 * @info: the GladeWidgetInfo structure for this widget.
 *
 * This function signature should be used by functions that build particular
 * widget types.  The function should create the new widget and set any non
 * standard widget parameters (ie. don't set visibility, size, etc), as
 * this is handled by glade_xml_build_widget, which calls these functions.
 *
 * Returns: the new widget.
 */
/**
 * GladeBuildChildrenFunc
 * @xml: the GladeXML object.
 * @parent: the parent.
 * @info: the GladeWidgetInfo structure for the parent.
 *
 * This function signature should be used by functions that are responsible
 * for adding children to a container widget.  To create each child widget,
 * glade_xml_build_widget should be called.
 */
/**
 * GladeFindInternalChildFunc
 * @xml: the GladeXML object.
 * @parent: the parent widget.
 * @childname: the name of the internal child
 *
 * When some composite widgets are created, a number of children are
 * added at the same time (for example, the vbox in a GtkDialog).
 * These widgets are identified in the XML interface file by the
 * internal-child attribute on their &lt;child&gt; element.
 *
 * When libglade encounters an internal child, rather than creating a
 * new widget instance, libglade walks up the tree until it finds the
 * first non internal-child parent.  It then calls the
 * find_internal_child callback for that parent's class.
 *
 * That callback should return the internal child corresponding to the
 * name passed in as the third argument.
 *
 * Returns: the named internal child.
 */
/**
 * glade_xml_build_widget:
 * @self: the GladeXML object.
 * @info: the GladeWidgetInfo structure for the widget.
 *
 * This function is not intended for people who just use libglade.  Instead
 * it is for people extending it (it is designed to be called in the child
 * build routine defined for the parent widget).  It first checks the type
 * of the widget from the class tag, then calls the corresponding widget
 * creation routine.  This routine sets up all the settings specific to that
 * type of widget.  Then general widget settings are performed on the widget.
 * Then it sets up accelerators for the widget, and extracts any signal
 * information for the widget.  Then it checks to see if there are any
 * child widget nodes for this widget, and if so calls the widget's
 * build routine, which will create the children with this function and add
 * them to the widget in the appropriate way.  Finally it returns the widget.
 * 
 * Returns: the newly created widget.
 */
GtkWidget *
glade_xml_build_widget(GladeXML *self, GladeWidgetInfo *info)
{
    GladeWidgetBuildData *data;
    GtkWidget *ret;

    debug(g_message("Widget class: %s", info->class));
    if (!strcmp(info->class, "Placeholder")) {
	g_warning("placeholders exist in interface description");
	ret = gtk_label_new("[placeholder]");
	gtk_widget_show(ret);
	return ret;
    }
    data = g_hash_table_lookup(widget_table, info->class);
    if (data == NULL) {
	char buf[50];
	g_warning("unknown widget class '%s'", info->class);
	g_snprintf(buf, 49, "[a %s]", info->class);
	ret = gtk_label_new(buf);
	gtk_widget_show(ret);
    } else {
	if (!data->typecode && data->get_type_func)
	    data->typecode = data->get_type_func();
	g_assert(data->new);
	g_assert(data->typecode);
	ret = data->new(self, data->typecode, info);
    }
    glade_xml_set_common_params(self, ret, info);
    return ret;
}

/**
 * glade_xml_handle_internal_child
 * @self: the GladeXML object.
 * @parent: the parent widget.
 * @child_info: the GladeChildInfo structure for the child.
 *
 * This function is intended to be called by the build_children
 * callback for container widgets.  If the build_children callback
 * encounters a child with the internal-child attribute set, then it
 * should call this function to handle it and then continue on to the
 * next child.
 */
void
glade_xml_handle_internal_child(GladeXML *self, GtkWidget *parent,
				GladeChildInfo *child_info)
{
    GladeWidgetBuildData *parent_build_data = NULL;
    GtkWidget *child;

    /* walk up the widget heirachy until we find a parent with a
     * find_internal_child handler */
    while (parent_build_data == NULL && parent != NULL) {
	parent_build_data = g_hash_table_lookup(widget_table,
						G_OBJECT_TYPE_NAME(parent));

	if (parent_build_data != NULL &&
	    parent_build_data->find_internal_child != NULL)
	    break;

	parent_build_data = NULL; /* set to NULL if no find_internal_child */
	parent = parent->parent;
    }

    if (!parent_build_data || !parent_build_data->find_internal_child) {
	g_warning("could not find a parent that handles internal"
		  " children for `%s'", child_info->internal_child);
	return;
    }

    child = parent_build_data->find_internal_child(self, parent,
						   child_info->internal_child);

    if (!child) {
	g_warning("could not find internal child `%s' in parent of type `%s'",
		  child_info->internal_child, G_OBJECT_TYPE_NAME(parent));
	return;
    }

    glade_xml_set_common_params(self, child, child_info->child);
}


static void
glade_xml_widget_destroy(GtkObject *object, GladeXML *xml)
{
    gchar *name;

    g_return_if_fail(GTK_IS_OBJECT(object));
    g_return_if_fail(GLADE_IS_XML(xml));

    name = g_object_get_qdata(G_OBJECT(object), glade_xml_name_id);

    if (!name) return;
    g_hash_table_remove(xml->priv->name_hash, name);
    g_object_set_qdata(G_OBJECT(object), glade_xml_tree_id, NULL);
    g_object_set_qdata(G_OBJECT(object), glade_xml_name_id, NULL);
}

/**
 * glade_xml_set_common_params
 * @self: the GladeXML widget.
 * @widget: the widget to set parameters on.
 * @info: the GladeWidgetInfo structure for the widget.
 *
 * This function sets the common parameters on a widget, and is responsible
 * for inserting it into the GladeXML object's internal structures.  It will
 * also add the children to this widget.  Usually this function is only called
 * by glade_xml_build_widget, but is exposed for difficult cases, such as
 * setting up toolbar buttons and the like.
 */
void
glade_xml_set_common_params(GladeXML *self, GtkWidget *widget,
			    GladeWidgetInfo *info)
{
    GList *tmp;
    GladeWidgetBuildData *data;

    /* get the build data */
    if (!widget_table)
	widget_table = g_hash_table_new(g_str_hash, g_str_equal);
    data = g_hash_table_lookup(widget_table, info->class);
    glade_xml_add_signals(self, widget, info);
    glade_xml_add_accels(self, widget, info);

    gtk_widget_set_name(widget, info->name);
    glade_xml_add_accessibility_info(self, widget, info);

#if 0
    if (info->tooltip) {
	gtk_tooltips_set_tip(self->priv->tooltips,
			     widget,
			     glade_xml_gettext(self, info->tooltip),
			     NULL);
    }
#endif

    /* store this information as data of the widget. */
    g_object_set_qdata(G_OBJECT(widget), glade_xml_tree_id, self);
    g_object_set_qdata(G_OBJECT(widget), glade_xml_name_id, info->name);
    /* store widgets in hash table, for easy lookup */
    g_hash_table_insert(self->priv->name_hash, info->name, widget);

    /* set up function to remove widget from GladeXML object's
     * name_hash on destruction. Use connect_object so the handler is
     * automatically removed on finalization of the GladeXML
     * object. */
    g_signal_connect_object(G_OBJECT(widget), "destroy",
			    G_CALLBACK(glade_xml_widget_destroy),
			    G_OBJECT(self), 0);

    /* handle any deferred properties using this widget */
    tmp = self->priv->deferred_props;
    while (tmp) {
	GladeDeferredProperty *dprop = tmp->data;

	if (!strcmp(info->name, dprop->target_name)) {
	    tmp = tmp->next;
	    self->priv->deferred_props =
		g_list_remove(self->priv->deferred_props, dprop);

	    switch (dprop->type) {
	    case DEFERRED_PROP:
		g_object_set(G_OBJECT(dprop->d.prop.object),
			     dprop->d.prop.prop_name, G_OBJECT(widget),
			     NULL);
		break;
	    case DEFERRED_REL:
		{
		    AtkObject *target = gtk_widget_get_accessible(widget);

		    add_relation(dprop->d.rel.relation_set,
				 dprop->d.rel.relation_type, target);
		    g_object_unref(dprop->d.rel.relation_set);
		}
		break;
	    default:
		g_warning("unknown deferred property type");
	    }
	    g_free(dprop);
	} else {
	    tmp = tmp->next;
	}
    }

    if (data && data->build_children && info->children)
	data->build_children(self, widget, info);
}

gchar **
glade_xml_get_toplevel_names (GladeXML *self)
{
    int i;
    gchar **ret;
    GladeInterface *tree;
    
    tree = self->priv->tree;

    ret = g_new (gchar *, tree->n_toplevels + 1);

    for (i = 0; i < tree->n_toplevels; i++)
	ret [i] = g_strdup (tree->toplevels [i]->name);
    ret[i] = NULL;

    return ret;
}

/* -*- Mode: C; c-basic-offset: 4 -*-
 * libglade - a library for building interfaces from XML files at runtime
 * Copyright (C) 1998-2001  James Henstridge <james@daa.com.au>
 *
 * glade-build.h: functions useful for adding support for new widgets.
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
#ifndef GLADE_BUILD_H
#define GLADE_BUILD_H

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <glade/glade-xml.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkadjustment.h>

#include <glade/glade-parser.h>

G_BEGIN_DECLS
	
/* create a new widget of some type.  Don't parse `standard' widget options */
typedef GtkWidget *(* GladeNewFunc) (GladeXML *xml,
				     GType widget_type,
				     GladeWidgetInfo *info);
/* call glade_xml_build_widget on each child node, and pack in self */
typedef void (* GladeBuildChildrenFunc) (GladeXML *xml,
					 GtkWidget *parent,
					 GladeWidgetInfo *info);
typedef GtkWidget *(* GladeFindInternalChildFunc) (GladeXML *xml,
						   GtkWidget *parent,
						   const char *childname);

typedef struct _GladeWidgetBuildData GladeWidgetBuildData;
struct _GladeWidgetBuildData {
    char *name;
    GladeNewFunc new;
    GladeBuildChildrenFunc build_children;
    GType (* get_type_func)(void);
    GType typecode;  /* this member takes precedence over get_type_func */
    GladeFindInternalChildFunc find_internal_child;
};

/* widgets is a static, NULL terminated array of GladeWidgetBuildData's.
 * They will be added to a hash table, with the name as the key, to be
 * used by glade_xml_build_widget */
void glade_register_widgets(const GladeWidgetBuildData *widgets);

/* set the current toplevel widget while building (use NULL to unset) */
void glade_xml_set_toplevel(GladeXML *xml, GtkWindow *window);

/* push and pop accelerator groups on a stack */
GtkAccelGroup *glade_xml_push_accel(GladeXML *xml);
GtkAccelGroup *glade_xml_pop_accel(GladeXML *xml);

/* make sure that xml->priv->accel_group is a valid AccelGroup */
GtkAccelGroup *glade_xml_ensure_accel(GladeXML *xml);

#if 0
/* this is the handler for GtkLabel accelerators */
void           glade_xml_handle_label_accel(GladeXML *xml, const gchar *target,
					    guint key);
guint          glade_xml_get_parent_accel  (GladeXML *xml);
#endif

/* this function is called to build the interface by GladeXML */
GtkWidget *glade_xml_build_widget(GladeXML *self, GladeWidgetInfo *info);

/* this function is used to get a pointer to the internal child of a
 * container widget.  It would generally be called by the
 * build_children callback for any children with the internal_child
 * name set. */
void glade_xml_handle_internal_child(GladeXML *self, GtkWidget *parent,
				     GladeChildInfo *child_info);

/* This function performs half of what glade_xml_build_widget does.  It is
 * useful when the widget has already been created.  Usually it would not
 * have any use at all. */
void       glade_xml_set_common_params(GladeXML *self,
				       GtkWidget *widget,
				       GladeWidgetInfo *info);

gboolean glade_xml_set_value_from_string (GParamSpec *pspec,
					  const gchar *string,
					  GValue *value);

GtkWidget *glade_standard_build_widget(GladeXML *xml, GType widget_type,
				       GladeWidgetInfo *info);

/* A standard child building routine that can be used in widget builders */
void glade_standard_build_children(GladeXML *self, GtkWidget *parent,
				   GladeWidgetInfo *info);

/* this is a wrapper for gtk_type_enum_find_value, that just returns the
 * integer value for the enum */
gint  glade_enum_from_string(GType type, const char *string);
guint glade_flags_from_string(GType type, const char *string);

/* a wrapper for gettext */
char *glade_xml_gettext(GladeXML *xml, const char *msgid);

/* invoke the custom widget creation function */
GtkWidget *glade_create_custom(GladeXML *xml, gchar *func_name, gchar *name,
			       gchar *string1, gchar *string2,
			       gint int1, gint int2);


/* the module dynamic loading interface ... */

/* increase this when there is a binary incompatible change in the
 * libglade module API */
#define GLADE_MODULE_API_VERSION 1
gchar *glade_module_check_version(gint version);

#define GLADE_MODULE_CHECK_INIT \
G_MODULE_EXPORT const gchar *g_module_check_init(GModule *gmodule); \
const gchar * \
g_module_check_init(GModule *gmodule) \
{ \
  return glade_module_check_version(GLADE_MODULE_API_VERSION); \
}

/* prototype for plugin init function (should be implemented by plugin) */
G_MODULE_EXPORT void glade_module_register_widgets(void);

G_END_DECLS
	
#endif

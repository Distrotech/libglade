/* -*- Mode: C; c-basic-offset: 4 -*-
 * libglade - a library for building interfaces from XML files at runtime
 * Copyright (C) 1998-2001  James Henstridge <james@daa.com.au>
 *
 * glade.h: the main include file for libglade.
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

#ifndef GLADE_INIT_H
#define GLADE_INIT_H

#include <glib.h>

G_BEGIN_DECLS

/* left for compatibility.  Libglade will now automatically initialise */
void glade_init(void);

/* handle dynamic loading of libglade extensions */
void glade_require(const gchar *library);
void glade_provide(const gchar *library);

#ifndef LIBGLADE_DISABLE_DEPRECATED
/* libglade now loads modules based on information
   in the glade file - these are deprecated */
#define glade_gnome_init  glade_init
#define glade_bonobo_init glade_init
#endif

G_END_DECLS

#endif
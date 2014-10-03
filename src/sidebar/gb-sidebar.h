/*
 * Copyright (c) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author:
 *      Ikey Doherty <michael.i.doherty@intel.com>
 */

/*
 * This file is taken from Gb+, and modified so we can use it until
 * it is available as a dependency. Change to Gb for 3.16.
 */

#ifndef __GB_SIDEBAR_H__
#define __GB_SIDEBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SIDEBAR                 (gb_sidebar_get_type ())
#define GB_SIDEBAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SIDEBAR, GbSidebar))
#define GB_IS_SIDEBAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SIDEBAR))
#define GB_SIDEBAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GB_TYPE_SIDEBAR, GbSidebarClass))
#define GB_IS_SIDEBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GB_TYPE_SIDEBAR))
#define GB_SIDEBAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GB_TYPE_SIDEBAR, GbSidebarClass))

typedef struct _GbSidebar        GbSidebar;
typedef struct _GbSidebarPrivate GbSidebarPrivate;
typedef struct _GbSidebarClass   GbSidebarClass;

struct _GbSidebar
{
  GtkBin parent;
};

struct _GbSidebarClass
{
  GtkBinClass parent_class;

  /* Padding for future expansion */
  void (*_gb_reserved1) (void);
  void (*_gb_reserved2) (void);
  void (*_gb_reserved3) (void);
  void (*_gb_reserved4) (void);
};

GType      gb_sidebar_get_type  (void) G_GNUC_CONST;
GtkWidget *gb_sidebar_new       (void);
void       gb_sidebar_set_stack (GbSidebar *sidebar,
                                 GtkStack  *stack);
GtkStack  *gb_sidebar_get_stack (GbSidebar *sidebar);

G_END_DECLS

#endif /* __GB_SIDEBAR_H__ */

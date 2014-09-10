/*
 * gedit-menu-stack-switcher.h
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Steve Frécinaux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEDIT_MENU_STACK_SWITCHER_H__
#define __GEDIT_MENU_STACK_SWITCHER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_MENU_STACK_SWITCHER             (gedit_menu_stack_switcher_get_type())
#define GEDIT_MENU_STACK_SWITCHER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_MENU_STACK_SWITCHER, GeditMenuStackSwitcher))
#define GEDIT_MENU_STACK_SWITCHER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_MENU_STACK_SWITCHER, GeditMenuStackSwitcherClass))
#define GEDIT_IS_MENU_STACK_SWITCHER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_MENU_STACK_SWITCHER))
#define GEDIT_IS_MENU_STACK_SWITCHER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_MENU_STACK_SWITCHER))
#define GEDIT_MENU_STACK_SWITCHER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_MENU_STACK_SWITCHER, GeditMenuStackSwitcherClass))

typedef struct _GeditMenuStackSwitcher        GeditMenuStackSwitcher;
typedef struct _GeditMenuStackSwitcherClass   GeditMenuStackSwitcherClass;
typedef struct _GeditMenuStackSwitcherPrivate GeditMenuStackSwitcherPrivate;

struct _GeditMenuStackSwitcher
{
  GtkMenuButton parent;

  /*< private >*/
  GeditMenuStackSwitcherPrivate *priv;
};

struct _GeditMenuStackSwitcherClass
{
  GtkMenuButtonClass parent_class;

  /* Padding for future expansion */
  void (*_gedit_reserved1) (void);
  void (*_gedit_reserved2) (void);
  void (*_gedit_reserved3) (void);
  void (*_gedit_reserved4) (void);
};

GType        gedit_menu_stack_switcher_get_type   (void) G_GNUC_CONST;

GtkWidget *  gedit_menu_stack_switcher_new 	      (void);

void         gedit_menu_stack_switcher_set_stack  (GeditMenuStackSwitcher *switcher,
                                                   GtkStack               *stack);

GtkStack *   gedit_menu_stack_switcher_get_stack  (GeditMenuStackSwitcher *switcher);

G_END_DECLS

#endif  /* __GEDIT_MENU_STACK_SWITCHER_H__  */

/* ex:set ts=2 sw=2 et: */

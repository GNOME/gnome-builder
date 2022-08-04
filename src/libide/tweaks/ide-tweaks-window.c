/* ide-tweaks-window.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-tweaks-window"

#include "config.h"

#include "ide-tweaks-panel-private.h"
#include "ide-tweaks-panel-list-private.h"
#include "ide-tweaks-window.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkStackPage, g_object_unref)

struct _IdeTweaksWindow
{
  AdwWindow       parent_instance;

  IdeTweaks      *tweaks;

  GtkStack       *panel_stack;
  GtkStack       *panel_list_stack;
  AdwWindowTitle *sidebar_title;
  GtkSearchBar   *sidebar_search_bar;
  GtkSearchEntry *sidebar_search_entry;

  guint           can_navigate_back : 1;
  guint           folded : 1;
};

enum {
  PROP_0,
  PROP_CAN_NAVIGATE_BACK,
  PROP_FOLDED,
  PROP_TWEAKS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksWindow, ide_tweaks_window, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];

static IdeTweaksPage *
ide_tweaks_window_get_current_page (IdeTweaksWindow *self)
{
  GtkWidget *visible_child;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  if ((visible_child = gtk_stack_get_visible_child (self->panel_stack)) &&
      IDE_IS_TWEAKS_PANEL (visible_child))
    return ide_tweaks_panel_get_page (IDE_TWEAKS_PANEL (visible_child));

  return NULL;
}

static IdeTweaksItem *
ide_tweaks_window_get_current_list_item (IdeTweaksWindow *self)
{
  GtkWidget *visible_child;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  if ((visible_child = gtk_stack_get_visible_child (self->panel_list_stack)))
    return ide_tweaks_panel_list_get_item (IDE_TWEAKS_PANEL_LIST (visible_child));

  return NULL;
}

static void
ide_tweaks_window_update_actions (IdeTweaksWindow *self)
{
  GtkWidget *visible_child;
  gboolean can_navigate_back = FALSE;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  if ((visible_child = gtk_stack_get_visible_child (self->panel_list_stack)))
    {
      IdeTweaksPanelList *list = IDE_TWEAKS_PANEL_LIST (visible_child);
      IdeTweaksItem *item = ide_tweaks_panel_list_get_item (list);

      can_navigate_back = !IDE_IS_TWEAKS (item);
    }

  if (can_navigate_back != self->can_navigate_back)
    {
      self->can_navigate_back = can_navigate_back;
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "navigation.back", can_navigate_back);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_NAVIGATE_BACK]);
    }
}

static void
ide_tweaks_window_page_activated_cb (IdeTweaksWindow    *self,
                                     IdeTweaksPage      *page,
                                     IdeTweaksPanelList *list)
{
  const char *name;
  GtkWidget *panel;
  gboolean has_subpages;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS_PAGE (page));
  g_assert (IDE_IS_TWEAKS_PANEL_LIST (list));

  if (page == ide_tweaks_window_get_current_page (self))
    return;

  name = ide_tweaks_item_get_id (IDE_TWEAKS_ITEM (page));
  has_subpages = ide_tweaks_page_get_has_subpage (page);

  /* Re-use a panel if it is already in the stack. This can happen if
   * we haven't yet reached a notify::transition-running that caused the
   * old page to be discarded.
   *
   * However, if there are subpages, we will instead jump right to the
   * subpage instead of this item as a page.
   */
  if (!has_subpages)
    {
      if (!(panel = gtk_stack_get_child_by_name (self->panel_stack, name)))
        {
          panel = ide_tweaks_panel_new (page);
          gtk_stack_add_named (self->panel_stack, panel, name);
        }

      gtk_stack_set_visible_child (self->panel_stack, panel);
    }
  else
    {
      GtkWidget *sublist;

      gtk_search_bar_set_search_mode (self->sidebar_search_bar, FALSE);

      sublist = ide_tweaks_panel_list_new (IDE_TWEAKS_ITEM (page));
      g_signal_connect_object (sublist,
                               "page-activated",
                               G_CALLBACK (ide_tweaks_window_page_activated_cb),
                               self,
                               G_CONNECT_SWAPPED);
      gtk_stack_add_named (self->panel_list_stack,
                           sublist,
                           ide_tweaks_item_get_id (IDE_TWEAKS_ITEM (page)));
      gtk_stack_set_visible_child (self->panel_list_stack, sublist);
      ide_tweaks_panel_list_set_search_mode (IDE_TWEAKS_PANEL_LIST (sublist), TRUE);
      ide_tweaks_panel_list_select_first (IDE_TWEAKS_PANEL_LIST (sublist));

      ide_tweaks_window_update_actions (self);
    }
}

static void
ide_tweaks_window_clear (IdeTweaksWindow *self)
{
  GtkWidget *child;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->panel_list_stack))))
    gtk_stack_remove (self->panel_list_stack, child);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->panel_stack))))
    gtk_stack_remove (self->panel_stack, child);
}

static void
ide_tweaks_window_rebuild (IdeTweaksWindow *self)
{
  GtkWidget *list;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));

  list = ide_tweaks_panel_list_new (IDE_TWEAKS_ITEM (self->tweaks));
  g_signal_connect_object (list,
                           "page-activated",
                           G_CALLBACK (ide_tweaks_window_page_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_stack_add_named (self->panel_list_stack,
                       list,
                       ide_tweaks_item_get_id (IDE_TWEAKS_ITEM (self->tweaks)));
  ide_tweaks_panel_list_select_first (IDE_TWEAKS_PANEL_LIST (list));

  ide_tweaks_window_update_actions (self);
}

static void
panel_stack_notify_transition_running_cb (IdeTweaksWindow *self,
                                          GParamSpec      *pspec,
                                          GtkStack        *stack)
{
  GtkSelectionModel *model;
  IdeTweaksPage *current_page;
  guint n_items;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (GTK_IS_STACK (stack));

  if (gtk_stack_get_transition_running (stack))
    return;

  if (!(current_page = ide_tweaks_window_get_current_page (self)))
    return;

  model = gtk_stack_get_pages (stack);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(GtkStackPage) page = g_list_model_get_item (G_LIST_MODEL (model), i - 1);
      IdeTweaksPanel *panel;
      IdeTweaksPage *item;

      panel = IDE_TWEAKS_PANEL (gtk_stack_page_get_child (page));
      item = ide_tweaks_panel_get_page (panel);

      if (item != NULL &&
          item != current_page &&
          !ide_tweaks_item_is_ancestor (IDE_TWEAKS_ITEM (current_page), IDE_TWEAKS_ITEM (item)))
        gtk_stack_remove (stack, GTK_WIDGET (panel));
    }
}

static void
panel_list_stack_notify_transition_running_cb (IdeTweaksWindow *self,
                                               GParamSpec      *pspec,
                                               GtkStack        *stack)
{
  IdeTweaksItem *current_list_item;
  GListModel *model;
  guint n_items;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (GTK_IS_STACK (stack));

  if (gtk_stack_get_transition_running (stack))
    return;

  if (!(current_list_item = ide_tweaks_window_get_current_list_item (self)))
    return;

  model = G_LIST_MODEL (gtk_stack_get_pages (stack));
  n_items = g_list_model_get_n_items (model);

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(GtkStackPage) page = g_list_model_get_item (G_LIST_MODEL (model), i - 1);
      IdeTweaksPanelList *list = IDE_TWEAKS_PANEL_LIST (gtk_stack_page_get_child (page));
      IdeTweaksItem *item = ide_tweaks_panel_list_get_item (list);

      if (item != NULL &&
          item != current_list_item &&
          !ide_tweaks_item_is_ancestor (current_list_item, item))
        gtk_stack_remove (stack, GTK_WIDGET (list));
    }
}

static void
panel_list_stack_notify_visible_child_cb (IdeTweaksWindow *self,
                                          GParamSpec      *pspec,
                                          GtkStack        *stack)
{
  IdeTweaksPanelList *list;
  IdeTweaksItem *item;
  const char *title;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (GTK_IS_STACK (stack));

  if (!(list = IDE_TWEAKS_PANEL_LIST (gtk_stack_get_visible_child (stack))))
    return;

  if ((item = ide_tweaks_panel_list_get_item (list)) && IDE_IS_TWEAKS_PAGE (item))
    title = ide_tweaks_page_get_title (IDE_TWEAKS_PAGE (item));
  else
    title = gtk_window_get_title (GTK_WINDOW (self));

  adw_window_title_set_title (self->sidebar_title, title);
}

static void
ide_tweaks_window_navigate_back_action (GtkWidget  *widget,
                                        const char *action_name,
                                        GVariant   *param)
{
  ide_tweaks_window_navigate_back (IDE_TWEAKS_WINDOW (widget));
}

static void
ide_tweaks_window_set_folded (IdeTweaksWindow *self,
                              gboolean         folded)
{
  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  folded = !!folded;

  if (self->folded != folded)
    {
      GtkSelectionMode selection_mode;
      GtkWidget *child;

      self->folded = folded;

      selection_mode = folded ? GTK_SELECTION_NONE : GTK_SELECTION_SINGLE;

      if ((child = gtk_stack_get_visible_child (self->panel_stack)))
        ide_tweaks_panel_set_folded (IDE_TWEAKS_PANEL (child), folded);

      if ((child = gtk_stack_get_visible_child (self->panel_list_stack)))
        ide_tweaks_panel_list_set_selection_mode (IDE_TWEAKS_PANEL_LIST (child), selection_mode);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOLDED]);
    }
}

static void
ide_tweaks_window_dispose (GObject *object)
{
  IdeTweaksWindow *self = (IdeTweaksWindow *)object;

  if (self->tweaks)
    {
      ide_tweaks_window_clear (self);
      g_clear_object (&self->tweaks);
    }

  G_OBJECT_CLASS (ide_tweaks_window_parent_class)->dispose (object);
}

static void
ide_tweaks_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksWindow *self = IDE_TWEAKS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CAN_NAVIGATE_BACK:
      g_value_set_boolean (value, ide_tweaks_window_get_can_navigate_back (self));
      break;

    case PROP_FOLDED:
      g_value_set_boolean (value, self->folded);
      break;

    case PROP_TWEAKS:
      g_value_set_object (value, ide_tweaks_window_get_tweaks (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksWindow *self = IDE_TWEAKS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_FOLDED:
      ide_tweaks_window_set_folded (self, g_value_get_boolean (value));
      break;

    case PROP_TWEAKS:
      ide_tweaks_window_set_tweaks (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_window_class_init (IdeTweaksWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_window_dispose;
  object_class->get_property = ide_tweaks_window_get_property;
  object_class->set_property = ide_tweaks_window_set_property;

  properties[PROP_CAN_NAVIGATE_BACK] =
    g_param_spec_boolean ("can-navigate-back", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_FOLDED] =
    g_param_spec_boolean ("folded", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TWEAKS] =
    g_param_spec_object ("tweaks", NULL, NULL,
                         IDE_TYPE_TWEAKS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-window.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, panel_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, panel_list_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, sidebar_title);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, sidebar_search_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, sidebar_search_entry);
  gtk_widget_class_bind_template_callback (widget_class, panel_list_stack_notify_transition_running_cb);
  gtk_widget_class_bind_template_callback (widget_class, panel_list_stack_notify_visible_child_cb);
  gtk_widget_class_bind_template_callback (widget_class, panel_stack_notify_transition_running_cb);

  gtk_widget_class_install_action (widget_class, "navigation.back", NULL, ide_tweaks_window_navigate_back_action);

  g_type_ensure (IDE_TYPE_TWEAKS_PANEL);
  g_type_ensure (IDE_TYPE_TWEAKS_PANEL_LIST);
}

static void
ide_tweaks_window_init (IdeTweaksWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "navigation.back", FALSE);
}

GtkWidget *
ide_tweaks_window_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_WINDOW, NULL);
}

/**
 * ide_tweaks_window_get_tweaks:
 * @self: a #IdeTweaksWindow
 *
 * Gets the tweaks property of the window.
 *
 * Returns: (transfer none) (nullable): an #IdeTweaks or %NULL
 */
IdeTweaks *
ide_tweaks_window_get_tweaks (IdeTweaksWindow *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WINDOW (self), NULL);

  return self->tweaks;
}

/**
 * ide_tweaks_window_set_tweaks:
 * @self: a #IdeTweaksWindow
 * @tweaks: (nullable): an #IdeTweaks
 *
 * Sets the tweaks to be displayed in the window.
 */
void
ide_tweaks_window_set_tweaks (IdeTweaksWindow *self,
                              IdeTweaks       *tweaks)
{
  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));
  g_return_if_fail (!tweaks || IDE_IS_TWEAKS (tweaks));

  if (self->tweaks == tweaks)
    return;

  if (self->tweaks != NULL)
    {
      ide_tweaks_window_clear (self);
      g_clear_object (&self->tweaks);
    }

  if (tweaks != NULL)
    {
      g_set_object (&self->tweaks, tweaks);
      ide_tweaks_window_rebuild (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TWEAKS]);
}

/**
 * ide_tweaks_window_navigate_to:
 * @self: a #IdeTweaksWindow
 * @item: (nullable): an #IdeTweaksItem or %NULL
 *
 * Navigates to @item.
 *
 * If @item is %NULL and #IdeTweaksWindow:tweaks is set, then navigates
 * to the topmost item.
 */
void
ide_tweaks_window_navigate_to (IdeTweaksWindow *self,
                               IdeTweaksItem   *item)
{
  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));
  g_return_if_fail (!item || IDE_IS_TWEAKS_ITEM (item));

  if (item == NULL)
    item = IDE_TWEAKS_ITEM (self->tweaks);

  if (item == NULL)
    return;

}

static IdeTweaksPanelList *
ide_tweaks_window_find_list_for_item (IdeTweaksWindow *self,
                                      IdeTweaksItem   *item)
{
  GListModel *model;
  guint n_items;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS_ITEM (item));

  model = G_LIST_MODEL (gtk_stack_get_pages (GTK_STACK (self->panel_list_stack)));
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkStackPage) page = g_list_model_get_item (model, i);
      IdeTweaksPanelList *list = IDE_TWEAKS_PANEL_LIST (gtk_stack_page_get_child (page));
      IdeTweaksItem *list_item = ide_tweaks_panel_list_get_item (list);

      if (item == list_item)
        return list;
    }

  return NULL;
}

void
ide_tweaks_window_navigate_back (IdeTweaksWindow *self)
{
  IdeTweaksPanelList *list;
  IdeTweaksItem *item;
  GtkWidget *visible_child;

  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));

  if (!(visible_child = gtk_stack_get_visible_child (self->panel_list_stack)))
    g_return_if_reached ();

  list = IDE_TWEAKS_PANEL_LIST (visible_child);
  item = ide_tweaks_panel_list_get_item (list);

  while ((item = ide_tweaks_item_get_parent (item)))
    {
      if ((list = ide_tweaks_window_find_list_for_item (self, item)))
        {
          gtk_stack_set_visible_child (self->panel_list_stack, GTK_WIDGET (list));
          ide_tweaks_window_update_actions (self);
          return;
        }
    }

  g_warning ("Failed to locate parent panel list");
}

gboolean
ide_tweaks_window_get_can_navigate_back (IdeTweaksWindow *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WINDOW (self), FALSE);

  return self->can_navigate_back;
}

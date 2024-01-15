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

#include <glib/gi18n.h>

#include <libpeas.h>

#include "ide-tweaks-addin.h"
#include "ide-tweaks-item-private.h"
#include "ide-tweaks-panel-private.h"
#include "ide-tweaks-panel-list-private.h"
#include "ide-tweaks-window.h"

struct _IdeTweaksWindow
{
  AdwWindow               parent_instance;

  IdeTweaks              *tweaks;
  PeasExtensionSet       *addins;
  IdeActionMuxer         *muxer;

  AdwNavigationSplitView *split_view;
  AdwNavigationView      *sidebar_nav_view;

  guint                   folded : 1;
};

enum {
  PROP_0,
  PROP_FOLDED,
  PROP_TWEAKS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksWindow, ide_tweaks_window, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];

static IdeTweaksPanelList *
ide_tweaks_window_get_current_list (IdeTweaksWindow *self)
{
  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  return IDE_TWEAKS_PANEL_LIST (adw_navigation_view_get_visible_page (self->sidebar_nav_view));
}

static void
ide_tweaks_window_update_title (IdeTweaksWindow *self)
{
  g_autofree char *window_title = NULL;
  g_autofree char *project_title = NULL;
  IdeTweaksPanelList *list;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  if (!(list = ide_tweaks_window_get_current_list (self)))
    return;

  if ((context = ide_tweaks_get_context (self->tweaks)))
    project_title = ide_context_dup_title (context);

  if (project_title != NULL)
    window_title = g_strdup_printf (_("Builder — %s — Preferences"), project_title);

  gtk_window_set_title (GTK_WINDOW (self),
                        window_title ? window_title
                                     : _("Builder — Preferences"));
}

static void
ide_tweaks_window_page_activated_cb (IdeTweaksWindow    *self,
                                     IdeTweaksPage      *page,
                                     IdeTweaksPanelList *list)
{
  GtkWidget *panel;
  gboolean has_subpages;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS_PAGE (page));
  g_assert (IDE_IS_TWEAKS_PANEL_LIST (list));

  has_subpages = ide_tweaks_page_get_has_subpage (page);

  /* If there are subpages, we will jump right to the subpage instead of this item as a page. */
  if (!has_subpages)
    {
      panel = ide_tweaks_panel_new (page);
      adw_navigation_split_view_set_content (self->split_view, ADW_NAVIGATION_PAGE (panel));
      adw_navigation_split_view_set_show_content (self->split_view, TRUE);
    }
  else
    {
      GtkWidget *sublist;

      sublist = ide_tweaks_panel_list_new (IDE_TWEAKS_ITEM (page));
      g_object_bind_property (page, "title", sublist, "title", G_BINDING_SYNC_CREATE);
      g_signal_connect_object (sublist,
                               "page-activated",
                               G_CALLBACK (ide_tweaks_window_page_activated_cb),
                               self,
                               G_CONNECT_SWAPPED);
      adw_navigation_view_push (self->sidebar_nav_view, ADW_NAVIGATION_PAGE (sublist));
      ide_tweaks_panel_list_set_search_mode (IDE_TWEAKS_PANEL_LIST (sublist),
                                             ide_tweaks_page_get_show_search (page));

      if (self->folded)
        ide_tweaks_panel_list_set_selection_mode (IDE_TWEAKS_PANEL_LIST (sublist),
                                                  GTK_SELECTION_NONE);
      else
        ide_tweaks_panel_list_select_first (IDE_TWEAKS_PANEL_LIST (sublist));
    }
}

static void
ide_tweaks_window_clear (IdeTweaksWindow *self)
{
  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));

  g_clear_object (&self->addins);

  adw_navigation_view_replace (self->sidebar_nav_view, NULL, 0);
  adw_navigation_split_view_set_content (self->split_view, NULL);
}

static void
ide_tweaks_window_addin_added_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  GObject    *exten,
                                  gpointer          user_data)
{
  IdeTweaksWindow *self = user_data;
  IdeTweaksAddin *addin = (IdeTweaksAddin *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TWEAKS_ADDIN (addin));
  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));

  ide_tweaks_addin_load (addin, self->tweaks);
}

static void
ide_tweaks_window_addin_removed_cb (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject    *exten,
                                    gpointer          user_data)
{
  IdeTweaksWindow *self = user_data;
  IdeTweaksAddin *addin = (IdeTweaksAddin *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TWEAKS_ADDIN (addin));
  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));

  ide_tweaks_addin_unload (addin, self->tweaks);
}

static void
ide_tweaks_window_add_initial_list (IdeTweaksWindow *self)
{
  GtkWidget *list;
  IdeContext *context;

  g_assert (IDE_IS_TWEAKS_WINDOW (self));

  /* Now create our list for toplevel pages */
  list = ide_tweaks_panel_list_new (IDE_TWEAKS_ITEM (self->tweaks));

  if ((context = ide_tweaks_get_context (self->tweaks)))
    {
      g_autofree char *project_title = ide_context_dup_title (context);
      adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (list), project_title);
    }
  else
    adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (list), _("Preferences"));

  g_signal_connect_object (list,
                           "page-activated",
                           G_CALLBACK (ide_tweaks_window_page_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  adw_navigation_view_push (self->sidebar_nav_view, ADW_NAVIGATION_PAGE (list));

  /* Setup initial selection state */
  if (self->folded)
    ide_tweaks_panel_list_set_selection_mode (IDE_TWEAKS_PANEL_LIST (list), GTK_SELECTION_NONE);
  else
    ide_tweaks_panel_list_select_first (IDE_TWEAKS_PANEL_LIST (list));
}

static void
ide_tweaks_window_rebuild (IdeTweaksWindow *self)
{
  g_assert (IDE_IS_TWEAKS_WINDOW (self));
  g_assert (IDE_IS_TWEAKS (self->tweaks));
  g_assert (self->addins == NULL);

  /* Allow addins to extend the tweaks instance */
  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_TWEAKS_ADDIN,
                                         NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_tweaks_window_addin_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_tweaks_window_addin_removed_cb),
                    self);
  peas_extension_set_foreach (self->addins, ide_tweaks_window_addin_added_cb, self);

  ide_tweaks_window_add_initial_list (self);

  ide_tweaks_window_update_title (self);
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
      AdwNavigationPage *page;

      self->folded = folded;

      selection_mode = folded ? GTK_SELECTION_NONE : GTK_SELECTION_SINGLE;

      for (page = adw_navigation_view_get_visible_page (self->sidebar_nav_view);
           page != NULL;
           page = adw_navigation_view_get_previous_page (self->sidebar_nav_view, page))
        ide_tweaks_panel_list_set_selection_mode (IDE_TWEAKS_PANEL_LIST (page), selection_mode);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOLDED]);
    }
}

static void
ide_tweaks_window_dispose (GObject *object)
{
  IdeTweaksWindow *self = (IdeTweaksWindow *)object;

  if (self->muxer != NULL)
    {
      gtk_widget_insert_action_group (GTK_WIDGET (self), "settings", NULL);
      ide_action_muxer_remove_all (self->muxer);
      g_clear_object (&self->muxer);
    }

  if (self->tweaks)
    {
      ide_tweaks_window_clear (self);
      g_clear_object (&self->tweaks);
    }

  g_assert (self->addins == NULL);
  g_assert (self->tweaks == NULL);
  g_assert (self->muxer == NULL);

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
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksWindow, sidebar_nav_view);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);

  g_type_ensure (IDE_TYPE_TWEAKS_PANEL);
  g_type_ensure (IDE_TYPE_TWEAKS_PANEL_LIST);
}

static void
ide_tweaks_window_init (IdeTweaksWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "navigation.back", FALSE);

  self->muxer = ide_action_muxer_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "settings",
                                  G_ACTION_GROUP (self->muxer));
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "app",
                                  G_ACTION_GROUP (g_application_get_default ()));
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
      ide_action_muxer_remove_all (self->muxer);
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
  IdeTweaksPanelList *list;

  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));
  g_return_if_fail (!item || IDE_IS_TWEAKS_ITEM (item));

  if (item == NULL)
    item = IDE_TWEAKS_ITEM (self->tweaks);

  if (!IDE_IS_TWEAKS_PAGE (item))
    return;

  /* We can only navigate to this page if it's in the current stack. To
   * support beyond that would require walking up the stack and pushing
   * each page into new panel lists.
   */
  if (!(list = ide_tweaks_window_get_current_list (self)))
    return;

  ide_tweaks_panel_list_select_item (list, item);
}

void
ide_tweaks_window_navigate_initial (IdeTweaksWindow *self)
{
  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));

  if (self->tweaks != NULL)
    {
      adw_navigation_split_view_set_show_content (self->split_view, FALSE);

      while (adw_navigation_view_pop (self->sidebar_nav_view)) {}
    }
}

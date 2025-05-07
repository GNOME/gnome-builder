/* ide-tweaks-panel.c
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

#define G_LOG_DOMAIN "ide-tweaks-panel"

#include "config.h"

#include "ide-tweaks.h"
#include "ide-tweaks-group.h"
#include "ide-tweaks-page.h"
#include "ide-tweaks-panel-private.h"
#include "ide-tweaks-widget-private.h"

struct _IdeTweaksPanel
{
  AdwNavigationPage    parent_instance;

  AdwPreferencesPage  *prefs_page;
  AdwPreferencesGroup *current_group;
  GtkListBox          *current_list;

  IdeTweaksPage       *page;
  IdeActionMuxer      *muxer;

  guint                current_list_has_non_rows : 1;
};

enum {
  PROP_0,
  PROP_PAGE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksPanel, ide_tweaks_panel, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];

static gboolean
listbox_keynav_failed_cb (GtkListBox       *list_box,
                          GtkDirectionType  direction)
{
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (list_box)));

  if (toplevel == NULL)
    return FALSE;

  if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    return FALSE;

  return gtk_widget_child_focus (toplevel, direction == GTK_DIR_UP ?
                                 GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

static IdeTweaksItemVisitResult
ide_tweaks_panel_visitor_cb (IdeTweaksItem *item,
                             gpointer       user_data)
{
  IdeTweaksPanel *self = user_data;

  g_assert (IDE_IS_TWEAKS_ITEM (item));
  g_assert (IDE_IS_TWEAKS_PANEL (self));

  if (FALSE) {}
  else if (IDE_IS_TWEAKS_GROUP (item))
    {
      IdeTweaksGroup *group = IDE_TWEAKS_GROUP (item);
      g_autofree char *title_escaped = NULL;
      const char *title;

      if ((title = ide_tweaks_group_get_title (group)))
        title_escaped = g_markup_escape_text (title, -1);

      self->current_list = NULL;
      self->current_list_has_non_rows = FALSE;
      self->current_group = g_object_new (ADW_TYPE_PREFERENCES_GROUP,
                                          "title", title_escaped,
                                          "visible", FALSE,
                                          NULL);
      adw_preferences_page_add (self->prefs_page, self->current_group);

      return IDE_TWEAKS_ITEM_VISIT_RECURSE;
    }
  else if (IDE_IS_TWEAKS_WIDGET (item))
    {
      GtkWidget *child = _ide_tweaks_widget_create_for_item (IDE_TWEAKS_WIDGET (item), item);

      g_assert (!child || GTK_IS_WIDGET (child));

      if (child == NULL)
        return IDE_TWEAKS_ITEM_VISIT_CONTINUE;

      if (self->current_group == NULL)
        {
          g_critical ("Attempt to add #%s without a group, this is discouraged",
                      ide_tweaks_item_get_id (item));
          self->current_list = NULL;
          self->current_list_has_non_rows = FALSE;
          self->current_group = g_object_new (ADW_TYPE_PREFERENCES_GROUP, NULL);
          adw_preferences_page_add (self->prefs_page, self->current_group);
          adw_preferences_group_add (self->current_group, child);
        }
      else
        {
          if (GTK_IS_LIST_BOX_ROW (child))
            {
              if (self->current_list == NULL && !self->current_list_has_non_rows)
                {
                  adw_preferences_group_add (self->current_group, child);
                }
              else if (self->current_list == NULL)
                {
                  self->current_list = g_object_new (GTK_TYPE_LIST_BOX,
                                                     "css-classes", IDE_STRV_INIT ("boxed-list"),
                                                     "selection-mode", GTK_SELECTION_NONE,
                                                     NULL);
                  g_signal_connect (self->current_list,
                                    "keynav-failed",
                                    G_CALLBACK (listbox_keynav_failed_cb),
                                    NULL);
                  adw_preferences_group_add (self->current_group, GTK_WIDGET (self->current_list));
                  gtk_list_box_append (self->current_list, child);
                }
              else
                {
                  gtk_list_box_append (self->current_list, child);
                }
            }
          else
            {
              self->current_list = NULL;
              self->current_list_has_non_rows = TRUE;
              adw_preferences_group_add (self->current_group, child);
            }

          gtk_widget_set_visible (GTK_WIDGET (self->current_group), TRUE);
        }
    }

  return IDE_TWEAKS_ITEM_VISIT_CONTINUE;
}

static void
ide_tweaks_panel_rebuild (IdeTweaksPanel *self)
{
  g_assert (IDE_IS_TWEAKS_PANEL (self));
  g_assert (IDE_IS_TWEAKS_PAGE (self->page));

  ide_tweaks_item_visit_children (IDE_TWEAKS_ITEM (self->page),
                                  ide_tweaks_panel_visitor_cb,
                                  self);
}

static void
ide_tweaks_panel_constructed (GObject *object)
{
  IdeTweaksPanel *self = (IdeTweaksPanel *)object;

  g_assert (IDE_IS_TWEAKS_PANEL (self));

  G_OBJECT_CLASS (ide_tweaks_panel_parent_class)->constructed (object);

  if (self->page != NULL)
    ide_tweaks_panel_rebuild (self);
}

static void
ide_tweaks_panel_dispose (GObject *object)
{
  IdeTweaksPanel *self = (IdeTweaksPanel *)object;

  g_clear_object (&self->page);
  g_clear_object (&self->muxer);

  G_OBJECT_CLASS (ide_tweaks_panel_parent_class)->dispose (object);
}

static void
ide_tweaks_panel_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksPanel *self = IDE_TWEAKS_PANEL (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      g_value_set_object (value, ide_tweaks_panel_get_page (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksPanel *self = IDE_TWEAKS_PANEL (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      self->page = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_class_init (IdeTweaksPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_tweaks_panel_constructed;
  object_class->dispose = ide_tweaks_panel_dispose;
  object_class->get_property = ide_tweaks_panel_get_property;
  object_class->set_property = ide_tweaks_panel_set_property;

  properties[PROP_PAGE] =
    g_param_spec_object ("page", NULL, NULL,
                         IDE_TYPE_TWEAKS_PAGE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "IdeTweaksPanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksPanel, prefs_page);
}

static void
ide_tweaks_panel_init (IdeTweaksPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->muxer = ide_action_muxer_new ();
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "settings",
                                  G_ACTION_GROUP (self->muxer));
}

GtkWidget *
ide_tweaks_panel_new (IdeTweaksPage *page)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PAGE (page), NULL);

  return g_object_new (IDE_TYPE_TWEAKS_PANEL,
                       "page", page,
                       NULL);
}

IdeTweaksPage *
ide_tweaks_panel_get_page (IdeTweaksPanel *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL (self), NULL);

  return self->page;
}

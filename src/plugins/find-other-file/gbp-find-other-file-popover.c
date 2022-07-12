/* gbp-find-other-file-popover.c
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

#define G_LOG_DOMAIN "gbp-find-other-file-popover"

#include "config.h"

#include <libide-gui.h>
#include <libide-projects.h>

#include "gbp-find-other-file-popover.h"
#include "gbp-found-file.h"

struct _GbpFindOtherFilePopover
{
  GtkPopover   parent_instance;
  GListModel  *model;
  GtkListView *list_view;
};

G_DEFINE_FINAL_TYPE (GbpFindOtherFilePopover, gbp_find_other_file_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_find_other_file_popover_activate_cb (GbpFindOtherFilePopover *self,
                                         guint                    position,
                                         GtkListView             *list_view)
{
  IdeWorkspace *workspace;
  GbpFoundFile *file;
  GListModel *model;

  IDE_ENTRY;

  g_assert (GBP_IS_FIND_OTHER_FILE_POPOVER (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  g_debug ("Activating file row at position %u", position);

  model = G_LIST_MODEL (gtk_list_view_get_model (list_view));
  file = g_list_model_get_item (model, position);
  workspace = ide_widget_get_workspace (GTK_WIDGET (self));

  gtk_popover_popdown (GTK_POPOVER (self));
  gbp_found_file_open (file, workspace);

  IDE_EXIT;
}

static gpointer
file_to_found_file (gpointer item,
                    gpointer user_data)
{
  GFile *workdir = user_data;
  g_autoptr(GFile) file = item;

  g_assert (G_IS_FILE (workdir));
  g_assert (G_IS_FILE (file));

  return gbp_found_file_new (workdir, file);
}

void
gbp_find_other_file_popover_set_model (GbpFindOtherFilePopover *self,
                                       GListModel              *model)
{
  g_assert (GBP_IS_FIND_OTHER_FILE_POPOVER (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  if (g_set_object (&self->model, model))
    {
      g_autoptr(GtkNoSelection) selection = NULL;

      if (model != NULL)
        {
          IdeContext *context = ide_widget_get_context (GTK_WIDGET (self));
          GtkMapListModel *map;

          map = gtk_map_list_model_new (g_object_ref (model),
                                        file_to_found_file,
                                        ide_context_ref_workdir (context),
                                        g_object_unref);
          selection = gtk_no_selection_new (G_LIST_MODEL (map));
        }
      else
        {
          selection = gtk_no_selection_new (G_LIST_MODEL (g_list_store_new (GBP_TYPE_FOUND_FILE)));
        }

      gtk_list_view_set_model (self->list_view, GTK_SELECTION_MODEL (selection));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
    }
}

static void
gbp_find_other_file_popover_dispose (GObject *object)
{
  GbpFindOtherFilePopover *self = (GbpFindOtherFilePopover *)object;

  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_find_other_file_popover_parent_class)->dispose (object);
}

static void
gbp_find_other_file_popover_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpFindOtherFilePopover *self = GBP_FIND_OTHER_FILE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_find_other_file_popover_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpFindOtherFilePopover *self = GBP_FIND_OTHER_FILE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gbp_find_other_file_popover_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_find_other_file_popover_class_init (GbpFindOtherFilePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_find_other_file_popover_dispose;
  object_class->get_property = gbp_find_other_file_popover_get_property;
  object_class->set_property = gbp_find_other_file_popover_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/find-other-file/gbp-find-other-file-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpFindOtherFilePopover, list_view);
  gtk_widget_class_bind_template_callback (widget_class, gbp_find_other_file_popover_activate_cb);

  g_type_ensure (GBP_TYPE_FOUND_FILE);
}

static void
gbp_find_other_file_popover_init (GbpFindOtherFilePopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

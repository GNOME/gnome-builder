/* gbp-vcsui-switcher-popover.c
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

#define G_LOG_DOMAIN "gbp-vcsui-switcher-popover"

#include "config.h"

#include "gbp-vcsui-switcher-popover.h"

struct _GbpVcsuiSwitcherPopover
{
  GtkPopover parent_instance;

  IdeVcs *vcs;

  GtkListView *branches_view;
  GListStore  *branches_model;
  GtkListView *tags_view;
  GListStore  *tags_model;
};

enum {
  PROP_0,
  PROP_VCS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpVcsuiSwitcherPopover, gbp_vcsui_switcher_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
gbp_vcsui_switcher_popover_list_branches_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(GbpVcsuiSwitcherPopover) self = user_data;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_VCSUI_SWITCHER_POPOVER (self));

  if (!(ar = ide_vcs_list_branches_finish (vcs, result, &error)))
    {
      g_warning ("Failed to list branches: %s\n", error->message);
      IDE_EXIT;
    }

  g_ptr_array_set_free_func (ar, g_object_unref);
  g_list_store_remove_all (self->branches_model);

  for (guint i = 0; i < ar->len; i++)
    g_list_store_append (self->branches_model, g_ptr_array_index (ar, i));

  IDE_EXIT;
}

static void
gbp_vcsui_switcher_popover_list_tags_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(GbpVcsuiSwitcherPopover) self = user_data;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_VCSUI_SWITCHER_POPOVER (self));

  if (!(ar = ide_vcs_list_tags_finish (vcs, result, &error)))
    {
      g_warning ("Failed to list tags: %s\n", error->message);
      IDE_EXIT;
    }

  g_ptr_array_set_free_func (ar, g_object_unref);
  g_list_store_remove_all (self->tags_model);

  for (guint i = 0; i < ar->len; i++)
    g_list_store_append (self->tags_model, g_ptr_array_index (ar, i));

  IDE_EXIT;
}

static void
gbp_vcsui_switcher_popover_dispose (GObject *object)
{
  GbpVcsuiSwitcherPopover *self = (GbpVcsuiSwitcherPopover *)object;

  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (gbp_vcsui_switcher_popover_parent_class)->dispose (object);
}

static void
gbp_vcsui_switcher_popover_show (GtkWidget *widget)
{
  GbpVcsuiSwitcherPopover *self = (GbpVcsuiSwitcherPopover *)widget;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_SWITCHER_POPOVER (self));

  if (self->vcs != NULL)
    {
      ide_vcs_list_branches_async (self->vcs,
                                   NULL,
                                   gbp_vcsui_switcher_popover_list_branches_cb,
                                   g_object_ref (self));
      ide_vcs_list_tags_async (self->vcs,
                               NULL,
                               gbp_vcsui_switcher_popover_list_tags_cb,
                               g_object_ref (self));
    }

  GTK_WIDGET_CLASS (gbp_vcsui_switcher_popover_parent_class)->show (widget);

  IDE_EXIT;
}

static void
gbp_vcsui_switcher_popover_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpVcsuiSwitcherPopover *self = GBP_VCSUI_SWITCHER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      g_value_set_object (value, self->vcs);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vcsui_switcher_popover_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpVcsuiSwitcherPopover *self = GBP_VCSUI_SWITCHER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_VCS:
      gbp_vcsui_switcher_popover_set_vcs (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_vcsui_switcher_popover_class_init (GbpVcsuiSwitcherPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_vcsui_switcher_popover_dispose;
  object_class->get_property = gbp_vcsui_switcher_popover_get_property;
  object_class->set_property = gbp_vcsui_switcher_popover_set_property;

  widget_class->show = gbp_vcsui_switcher_popover_show;

  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "Vcs",
                         "The version control system",
                         IDE_TYPE_VCS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/vcsui/gbp-vcsui-switcher-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiSwitcherPopover, branches_view);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiSwitcherPopover, branches_model);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiSwitcherPopover, tags_view);
  gtk_widget_class_bind_template_child (widget_class, GbpVcsuiSwitcherPopover, tags_model);
}

static void
gbp_vcsui_switcher_popover_init (GbpVcsuiSwitcherPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeVcs *
gbp_vcsui_switcher_popover_get_vcs (GbpVcsuiSwitcherPopover *self)
{
  g_return_val_if_fail (GBP_IS_VCSUI_SWITCHER_POPOVER (self), NULL);

  return self->vcs;
}

void
gbp_vcsui_switcher_popover_set_vcs (GbpVcsuiSwitcherPopover *self,
                                    IdeVcs                  *vcs)
{
  g_return_if_fail (GBP_IS_VCSUI_SWITCHER_POPOVER (self));
  g_return_if_fail (!vcs || IDE_IS_VCS (vcs));

  if (g_set_object (&self->vcs, vcs))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VCS]);
    }
}

GtkWidget *
gbp_vcsui_switcher_popover_new (void)
{
  return g_object_new (GBP_TYPE_VCSUI_SWITCHER_POPOVER, NULL);
}

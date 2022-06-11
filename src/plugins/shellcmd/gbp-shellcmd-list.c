/* gbp-shellcmd-list.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-list"

#include "config.h"

#include "gbp-shellcmd-list.h"

struct _GbpShellcmdList
{
  GtkWidget    parent_instance;

  GSettings   *settings;

  GtkListView *list_view;
  GtkStack    *stack;
};

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpShellcmdList, gbp_shellcmd_list, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
gbp_shellcmd_list_dispose (GObject *object)
{
  GbpShellcmdList *self = (GbpShellcmdList *)object;

  g_clear_pointer ((GtkWidget **)&self->stack, gtk_widget_unparent);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gbp_shellcmd_list_parent_class)->dispose (object);
}

static void
gbp_shellcmd_list_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbpShellcmdList *self = GBP_SHELLCMD_LIST (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_list_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbpShellcmdList *self = GBP_SHELLCMD_LIST (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_list_class_init (GbpShellcmdListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_shellcmd_list_dispose;
  object_class->get_property = gbp_shellcmd_list_get_property;
  object_class->set_property = gbp_shellcmd_list_set_property;

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-list.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdList, list_view);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdList, stack);
}

static void
gbp_shellcmd_list_init (GbpShellcmdList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

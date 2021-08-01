/* gbp-meson-tool-row.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "gbp-meson-tool-row"

#include <glib/gi18n.h>

#include "gbp-meson-tool-row.h"
#include "gbp-meson-utils.h"

struct _GbpMesonToolRow
{
  GtkListBoxRow parent_instance;
  gchar *tool_path;
  gchar *tool_id;
  gchar *lang_id;

  GtkLabel *name_label;
  GtkButton *delete_button;
};

G_DEFINE_FINAL_TYPE (GbpMesonToolRow, gbp_meson_tool_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_TOOL_PATH,
  PROP_TOOL_ID,
  PROP_LANG_ID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  TOOL_REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

GbpMesonToolRow *
gbp_meson_tool_row_new (const gchar *tool_id,
                        const gchar *tool_path,
                        const gchar *lang_id)
{
  GbpMesonToolRow *tool_row;

  g_return_val_if_fail (tool_id != NULL, NULL);
  g_return_val_if_fail (tool_path != NULL, NULL);

  tool_row = g_object_new (GBP_TYPE_MESON_TOOL_ROW,
                           "tool-id", tool_id,
                           "tool-path", tool_path,
                           "lang-id", lang_id,
                           "visible", TRUE,
                           NULL);

  return tool_row;
}

static void
meson_tool_row_delete (GbpMesonToolRow *self,
                       gpointer         user_data)
{
  g_assert (GBP_IS_MESON_TOOL_ROW (self));

  g_signal_emit (self, signals[TOOL_REMOVED], 0);

  gtk_widget_destroy (GTK_WIDGET (self));
}

const gchar *
gbp_meson_tool_row_get_tool_id (GbpMesonToolRow *tool_row)
{
  g_return_val_if_fail (GBP_IS_MESON_TOOL_ROW (tool_row), NULL);

  return tool_row->tool_id;
}

static void
gbp_meson_tool_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpMesonToolRow *self = GBP_MESON_TOOL_ROW (object);

  switch (prop_id)
    {
    case PROP_TOOL_PATH:
      g_value_set_string (value, self->tool_path);
      break;

    case PROP_TOOL_ID:
      g_value_set_string (value, self->tool_id);
      break;

    case PROP_LANG_ID:
      g_value_set_string (value, self->lang_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_tool_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpMesonToolRow *self = GBP_MESON_TOOL_ROW (object);

  switch (prop_id)
    {
    case PROP_TOOL_PATH:
      self->tool_path = g_value_dup_string (value);
      break;

    case PROP_TOOL_ID:
      self->tool_id = g_value_dup_string (value);
      break;

    case PROP_LANG_ID:
      self->lang_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_tool_row_finalize (GObject *object)
{
  GbpMesonToolRow *self = (GbpMesonToolRow *) object;

  g_clear_pointer (&self->tool_path, g_free);
  g_clear_pointer (&self->tool_id, g_free);
  g_clear_pointer (&self->lang_id, g_free);

  G_OBJECT_CLASS (gbp_meson_tool_row_parent_class)->finalize (object);
}

static void
gbp_meson_tool_row_constructed (GObject *object)
{
  GbpMesonToolRow *self = (GbpMesonToolRow *) object;
  const gchar *tool_name = gbp_meson_get_tool_display_name (self->tool_id);
  if (self->lang_id != NULL && g_strcmp0 (self->lang_id, "*") != 0)
    {
      g_autofree gchar *complete_name = g_strdup_printf ("%s (%s)", tool_name, self->lang_id);
      gtk_label_set_label (self->name_label, complete_name);
    }
  else
    gtk_label_set_label (self->name_label, tool_name);
}

static void
gbp_meson_tool_row_class_init (GbpMesonToolRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_meson_tool_row_finalize;
  object_class->get_property = gbp_meson_tool_row_get_property;
  object_class->set_property = gbp_meson_tool_row_set_property;
  object_class->constructed = gbp_meson_tool_row_constructed;

  properties [PROP_TOOL_PATH] =
    g_param_spec_string ("tool-path",
                         "Tool Path",
                         "The absolute path of the tool",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TOOL_ID] =
    g_param_spec_string ("tool-id",
                         "Tool ID",
                         "The internal identifier of the tool",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANG_ID] =
    g_param_spec_string ("lang-id",
                         "Tool Path",
                         "The language the tool should be used for",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [TOOL_REMOVED] =
    g_signal_new_class_handler ("tool-removed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/meson/gbp-meson-tool-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GbpMesonToolRow, delete_button);
}

static void
gbp_meson_tool_row_init (GbpMesonToolRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->delete_button, "clicked", G_CALLBACK (meson_tool_row_delete), self);
  g_object_bind_property (self, "tool-path", self, "tooltip-text", G_BINDING_DEFAULT);
}

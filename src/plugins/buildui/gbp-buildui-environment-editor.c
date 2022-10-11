/* gbp-buildui-environment-editor.c
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

#define G_LOG_DOMAIN "gbp-buildui-environment-editor"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gtk.h>

#include "gbp-buildui-environment-editor.h"
#include "gbp-buildui-environment-row.h"

struct _GbpBuilduiEnvironmentEditor
{
  GtkWidget         parent_instance;

  IdeTweaksBinding *binding;

  GtkBox           *box;
  GtkListBox       *list_box;
};

enum {
  PROP_0,
  PROP_BINDING,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpBuilduiEnvironmentEditor, gbp_buildui_environment_editor, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
on_row_remove_cb (GbpBuilduiEnvironmentEditor *self,
                  GbpBuilduiEnvironmentRow    *row)
{
  g_auto(GStrv) value = NULL;
  const char *variable;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_EDITOR (self));
  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_ROW (row));

  if (self->binding == NULL)
    return;

  if (!(variable = gbp_buildui_environment_row_get_variable (row)))
    return;

  value = ide_tweaks_binding_dup_strv (self->binding);
  if (ide_strv_remove_from_set (value, variable))
    ide_tweaks_binding_set_strv (self->binding, (const char * const *)value);
}

static GtkWidget *
gbp_buildui_environment_editor_create_row_cb (gpointer item,
                                              gpointer user_data)
{
  GbpBuilduiEnvironmentEditor *self = user_data;
  GtkStringObject *object = item;
  const char *str = gtk_string_object_get_string (object);
  GtkWidget *row = gbp_buildui_environment_row_new (str);

  g_signal_connect_object (row,
                           "remove",
                           G_CALLBACK (on_row_remove_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return row;
}

static void
on_binding_changed_cb (GbpBuilduiEnvironmentEditor *self,
                       IdeTweaksBinding            *binding)
{
  g_auto(GStrv) strv = NULL;
  g_autoptr(GtkStringList) model = NULL;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  strv = ide_tweaks_binding_dup_strv (binding);
  model = gtk_string_list_new ((const char * const *)strv);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (model),
                           gbp_buildui_environment_editor_create_row_cb,
                           self, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->list_box),
                          g_list_model_get_n_items (G_LIST_MODEL (model)) > 0);
}

static void
on_entry_activate_cb (GbpBuilduiEnvironmentEditor *self,
                      const char                  *text,
                      IdeEntryPopover             *popover)
{
  g_autofree char *copy = NULL;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  /* First copy the text so we can clear it */
  copy = g_strdup (ide_entry_popover_get_text (popover));

  /* Clear and dismiss popover */
  ide_entry_popover_set_text (popover, "");
  gtk_popover_popdown (GTK_POPOVER (popover));

  /* Now request that the variable be added */
  if (!ide_str_empty0 (copy))
    gtk_widget_activate_action (GTK_WIDGET (self), "variable.add", "s", text);
}

static void
on_entry_changed_cb (GbpBuilduiEnvironmentEditor *self,
                     IdeEntryPopover             *popover)
{
  const char *errstr = NULL;
  gboolean valid = FALSE;
  const char *text;
  const char *eq;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENTRY_POPOVER (popover));

  text = ide_entry_popover_get_text (popover);
  eq = strchr (text, '=');

  if (!ide_str_empty0 (text) && eq == NULL)
    errstr = _("Use KEY=VALUE to set an environment variable");

  if (eq != NULL && eq != text)
    {
      if (g_unichar_isdigit (g_utf8_get_char (text)))
        {
          errstr = _("Keys may not start with a number");
          goto failure;

        }
      for (const char *iter = text; iter < eq; iter = g_utf8_next_char (iter))
        {
          gunichar ch = g_utf8_get_char (iter);

          if (!g_unichar_isalnum (ch) && ch != '_')
            {
              errstr = _("Keys may only contain alpha-numerics or underline.");
              goto failure;
            }
        }

      if (g_ascii_isalpha (*text))
        valid = TRUE;
    }

failure:
  ide_entry_popover_set_ready (popover, valid);
  ide_entry_popover_set_message (popover, errstr);
}

static void
variable_add_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *param)
{
  GbpBuilduiEnvironmentEditor *self = (GbpBuilduiEnvironmentEditor *)widget;
  g_auto(GStrv) value = NULL;
  const char *text;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_EDITOR (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  if (self->binding == NULL)
    return;

  text = g_variant_get_string (param, NULL);
  value = ide_tweaks_binding_dup_strv (self->binding);

  if (ide_strv_add_to_set (&value, g_strdup (text)))
    ide_tweaks_binding_set_strv (self->binding, (const char * const *)value);
}

static void
gbp_buildui_environment_editor_constructed (GObject *object)
{
  GbpBuilduiEnvironmentEditor *self = (GbpBuilduiEnvironmentEditor *)object;
  GType type;

  G_OBJECT_CLASS (gbp_buildui_environment_editor_parent_class)->constructed (object);

  if (self->binding == NULL)
    return;

  if (!ide_tweaks_binding_get_expected_type (self->binding, &type) || type != G_TYPE_STRV)
    return;

  g_signal_connect_object (self->binding,
                           "changed",
                           G_CALLBACK (on_binding_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  on_binding_changed_cb (self, self->binding);
}

static void
gbp_buildui_environment_editor_dispose (GObject *object)
{
  GbpBuilduiEnvironmentEditor *self = (GbpBuilduiEnvironmentEditor *)object;

  g_clear_pointer ((GtkWidget **)&self->box, gtk_widget_unparent);
  g_clear_object (&self->binding);

  G_OBJECT_CLASS (gbp_buildui_environment_editor_parent_class)->dispose (object);
}

static void
gbp_buildui_environment_editor_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  GbpBuilduiEnvironmentEditor *self = GBP_BUILDUI_ENVIRONMENT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BINDING:
      g_value_set_object (value, self->binding);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_environment_editor_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  GbpBuilduiEnvironmentEditor *self = GBP_BUILDUI_ENVIRONMENT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_BINDING:
      self->binding = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_environment_editor_class_init (GbpBuilduiEnvironmentEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_buildui_environment_editor_constructed;
  object_class->dispose = gbp_buildui_environment_editor_dispose;
  object_class->get_property = gbp_buildui_environment_editor_get_property;
  object_class->set_property = gbp_buildui_environment_editor_set_property;

  properties[PROP_BINDING] =
    g_param_spec_object ("binding", NULL, NULL,
                         IDE_TYPE_TWEAKS_BINDING,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-environment-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiEnvironmentEditor, box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiEnvironmentEditor, list_box);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed_cb);

  gtk_widget_class_install_action (widget_class, "variable.add", "s", variable_add_action);

  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
}

static void
gbp_buildui_environment_editor_init (GbpBuilduiEnvironmentEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_buildui_environment_editor_new (IdeTweaksBinding *binding)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (binding), NULL);

  return g_object_new (GBP_TYPE_BUILDUI_ENVIRONMENT_EDITOR,
                       "binding", binding,
                       NULL);
}

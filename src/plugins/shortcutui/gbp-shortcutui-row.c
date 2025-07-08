/* gbp-shortcutui-row.c
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

#define G_LOG_DOMAIN "gbp-shortcutui-row"

#include "config.h"

#include <libide-gui.h>

#include "gbp-shortcutui-shortcut.h"
#include "gbp-shortcutui-row.h"

struct _GbpShortcutuiRow
{
  AdwActionRow           parent_instance;

  GbpShortcutuiShortcut *shortcut;

  GtkWidget             *label;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiRow, gbp_shortcutui_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_SHORTCUT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_shortcutui_row_reset_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *params)
{
  GbpShortcutuiRow *self = (GbpShortcutuiRow *)widget;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_ROW (self));

  if (!gbp_shortcutui_shortcut_override (self->shortcut, NULL, &error))
    g_warning ("Failed to override shortcut: %s", error->message);

  IDE_EXIT;
}

static char *
null_to_string (gpointer    instance,
                const char *param)
{
  return g_strdup (param ? param : "");
}

static void
gbp_shortcutui_row_constructed (GObject *object)
{
  GbpShortcutuiRow *self = (GbpShortcutuiRow *)object;

  G_OBJECT_CLASS (gbp_shortcutui_row_parent_class)->constructed (object);

  /* This just avoids bindings/expressions for something rather static */
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 gbp_shortcutui_shortcut_get_title (self->shortcut));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self),
                               gbp_shortcutui_shortcut_get_subtitle (self->shortcut));
}

static void
gbp_shortcutui_row_dispose (GObject *object)
{
  GbpShortcutuiRow *self = (GbpShortcutuiRow *)object;

  g_clear_object (&self->shortcut);

  G_OBJECT_CLASS (gbp_shortcutui_row_parent_class)->dispose (object);
}

static void
gbp_shortcutui_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpShortcutuiRow *self = GBP_SHORTCUTUI_ROW (object);

  switch (prop_id)
    {
    case PROP_SHORTCUT:
      g_value_set_object (value, gbp_shortcutui_row_get_shortcut (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpShortcutuiRow *self = GBP_SHORTCUTUI_ROW (object);

  switch (prop_id)
    {
    case PROP_SHORTCUT:
      self->shortcut = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_row_class_init (GbpShortcutuiRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_shortcutui_row_dispose;
  object_class->constructed = gbp_shortcutui_row_constructed;
  object_class->get_property = gbp_shortcutui_row_get_property;
  object_class->set_property = gbp_shortcutui_row_set_property;

  properties [PROP_SHORTCUT] =
    g_param_spec_object ("shortcut", NULL, NULL,
                         GBP_TYPE_SHORTCUTUI_SHORTCUT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shortcutui/gbp-shortcutui-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiRow, label);
  gtk_widget_class_bind_template_callback (widget_class, null_to_string);

  gtk_widget_class_install_action (widget_class, "shortcut.reset", NULL, gbp_shortcutui_row_reset_action);

  g_type_ensure (GBP_TYPE_SHORTCUTUI_SHORTCUT);
}

static void
gbp_shortcutui_row_init (GbpShortcutuiRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_shortcutui_row_update_header (GbpShortcutuiRow *self,
                                  GbpShortcutuiRow *before)
{
  GtkWidget *header = NULL;

  g_return_if_fail (GBP_IS_SHORTCUTUI_ROW (self));
  g_return_if_fail (!before || GBP_IS_SHORTCUTUI_ROW (before));

  if (self->shortcut == NULL)
    return;

  if (before == NULL ||
      !ide_str_equal0 (gbp_shortcutui_shortcut_get_page (self->shortcut),
                       gbp_shortcutui_shortcut_get_page (before->shortcut)) ||
      !ide_str_equal0 (gbp_shortcutui_shortcut_get_group (self->shortcut),
                       gbp_shortcutui_shortcut_get_group (before->shortcut)))
    {
      const char *page = gbp_shortcutui_shortcut_get_page (self->shortcut);
      const char *group = gbp_shortcutui_shortcut_get_group (self->shortcut);

      if (page != NULL && group != NULL)
        {
          g_autofree char *title = g_strdup_printf ("%s / %s", page, group);

          header = g_object_new (GTK_TYPE_LABEL,
                                 "css-classes", IDE_STRV_INIT ("heading"),
                                 "halign", GTK_ALIGN_START,
                                 "hexpand", TRUE,
                                 "label", title,
                                 "use-markup", TRUE,
                                 NULL);
        }
    }

  if (header)
    gtk_widget_add_css_class (GTK_WIDGET (self), "has-header");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "has-header");

  gtk_list_box_row_set_header (GTK_LIST_BOX_ROW (self), header);
}

GbpShortcutuiRow *
gbp_shortcutui_row_new (GbpShortcutuiShortcut *shortcut)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (shortcut), NULL);

  return g_object_new (GBP_TYPE_SHORTCUTUI_ROW,
                       "activatable", TRUE,
                       "shortcut", shortcut,
                       NULL);
}

GbpShortcutuiShortcut *
gbp_shortcutui_row_get_shortcut (GbpShortcutuiRow *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_ROW (self), NULL);

  return self->shortcut;
}

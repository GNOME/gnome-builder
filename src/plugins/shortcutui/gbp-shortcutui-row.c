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

#include "ide-shortcut-observer-private.h"

#include "gbp-shortcutui-action.h"
#include "gbp-shortcutui-row.h"

struct _GbpShortcutuiRow
{
  AdwActionRow parent_instance;
  IdeShortcutObserver *observer;
  GbpShortcutuiAction *action;
  GtkWidget *shortcut;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiRow, gbp_shortcutui_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_ACTION,
  PROP_OBSERVER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_shortcutui_row_set_action (GbpShortcutuiRow    *self,
                               GbpShortcutuiAction *action)
{
  g_auto(GValue) title = G_VALUE_INIT;
  g_auto(GValue) subtitle = G_VALUE_INIT;
  g_auto(GValue) accelerator = G_VALUE_INIT;

  g_assert (GBP_IS_SHORTCUTUI_ROW (self));
  g_assert (GBP_IS_SHORTCUTUI_ACTION (action));

  g_set_object (&self->action, action);

  g_value_init (&title, G_TYPE_STRING);
  g_value_init (&subtitle, G_TYPE_STRING);
  g_value_init (&accelerator, G_TYPE_STRING);

  g_object_get_property (G_OBJECT (action), "title", &title);
  g_object_get_property (G_OBJECT (action), "subtitle", &subtitle);
  g_object_get_property (G_OBJECT (action), "accelerator", &accelerator);

  g_object_set_property (G_OBJECT (self), "title", &title);
  g_object_set_property (G_OBJECT (self), "subtitle", &subtitle);
  g_object_set_property (G_OBJECT (self->shortcut), "accelerator", &accelerator);
}

static void
on_accel_changed_cb (GbpShortcutuiRow *self,
                     const char       *action_name,
                     const char       *accel)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SHORTCUTUI_ROW (self));
  g_assert (action_name != NULL);

  g_debug ("Action \"%s\" changed to accel %s", action_name, accel);

  g_object_set (self->shortcut,
                "accelerator", accel,
                NULL);

  IDE_EXIT;
}

static void
gbp_shortcutui_row_constructed (GObject *object)
{
  GbpShortcutuiRow *self = (GbpShortcutuiRow *)object;
  g_autofree char *detail = NULL;
  const char *action_name;

  G_OBJECT_CLASS (gbp_shortcutui_row_parent_class)->constructed (object);

  if (!(action_name = gbp_shortcutui_action_get_action_name (self->action)))
    return;

  detail = g_strdup_printf ("accel-changed::%s", action_name);

  g_signal_connect_object (self->observer,
                           detail,
                           G_CALLBACK (on_accel_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_shortcutui_row_dispose (GObject *object)
{
  GbpShortcutuiRow *self = (GbpShortcutuiRow *)object;

  g_clear_object (&self->action);
  g_clear_object (&self->observer);

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
    case PROP_ACTION:
      g_value_set_object (value, self->action);
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
    case PROP_ACTION:
      gbp_shortcutui_row_set_action (self, g_value_get_object (value));
      break;

    case PROP_OBSERVER:
      self->observer = g_value_dup_object (value);
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

  object_class->constructed = gbp_shortcutui_row_constructed;
  object_class->dispose = gbp_shortcutui_row_dispose;
  object_class->get_property = gbp_shortcutui_row_get_property;
  object_class->set_property = gbp_shortcutui_row_set_property;

  properties [PROP_ACTION] =
    g_param_spec_object ("action", NULL, NULL,
                         GBP_TYPE_SHORTCUTUI_ACTION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_OBSERVER] =
    g_param_spec_object ("observer", NULL, NULL,
                         IDE_TYPE_SHORTCUT_OBSERVER,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shortcutui/gbp-shortcutui-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiRow, shortcut);
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

  if (self->action == NULL)
    return;

  if (before == NULL ||
      !gbp_shortcutui_action_is_same_group (self->action, before->action))
    {
      const char *page = gbp_shortcutui_action_get_page (self->action);
      const char *group = gbp_shortcutui_action_get_group (self->action);

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

const char *
gbp_shortcutui_row_get_accelerator (GbpShortcutuiRow *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_ROW (self), NULL);

  return gbp_shortcutui_action_get_accelerator (self->action);
}

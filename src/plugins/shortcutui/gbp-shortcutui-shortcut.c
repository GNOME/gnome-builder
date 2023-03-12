/*
 * gbp-shortcutui-shortcut.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shortcutui-shortcut"

#include "config.h"

#include <libide-gui.h>

#include "ide-shortcut-bundle-private.h"

#include "gbp-shortcutui-shortcut.h"

#define GET_INFO(o) ((IdeShortcut*)g_object_get_data(G_OBJECT(GTK_SHORTCUT(o)), "IDE_SHORTCUT"))

struct _GbpShortcutuiShortcut
{
  GObject      parent_instance;
  GtkShortcut *shortcut;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiShortcut, gbp_shortcutui_shortcut, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_HAS_OVERRIDE,
  PROP_SHORTCUT,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_shortcutui_shortcut_dispose (GObject *object)
{
  GbpShortcutuiShortcut *self = (GbpShortcutuiShortcut *)object;

  g_clear_object (&self->shortcut);

  G_OBJECT_CLASS (gbp_shortcutui_shortcut_parent_class)->dispose (object);
}

static void
gbp_shortcutui_shortcut_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpShortcutuiShortcut *self = GBP_SHORTCUTUI_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_take_string (value, gbp_shortcutui_shortcut_dup_accelerator (self));
      break;

    case PROP_HAS_OVERRIDE:
      g_value_set_boolean (value, gbp_shortcutui_shortcut_has_override (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gbp_shortcutui_shortcut_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gbp_shortcutui_shortcut_get_subtitle (self));
      break;

    case PROP_SHORTCUT:
      g_value_set_object (value, self->shortcut);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_shortcut_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbpShortcutuiShortcut *self = GBP_SHORTCUTUI_SHORTCUT (object);

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
gbp_shortcutui_shortcut_class_init (GbpShortcutuiShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_shortcutui_shortcut_dispose;
  object_class->get_property = gbp_shortcutui_shortcut_get_property;
  object_class->set_property = gbp_shortcutui_shortcut_set_property;

  properties [PROP_SHORTCUT] =
    g_param_spec_object ("shortcut", NULL, NULL,
                         GTK_TYPE_SHORTCUT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_OVERRIDE] =
    g_param_spec_boolean ("has-override", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                        NULL,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shortcutui_shortcut_init (GbpShortcutuiShortcut *self)
{
}

GbpShortcutuiShortcut *
gbp_shortcutui_shortcut_new (GtkShortcut *shortcut)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (shortcut), NULL);
  g_return_val_if_fail (GET_INFO (shortcut) != NULL, NULL);

  return g_object_new (GBP_TYPE_SHORTCUTUI_SHORTCUT,
                       "shortcut", shortcut,
                       NULL);
}

const char *
gbp_shortcutui_shortcut_get_title (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  /* TODO: Get from menu info */

  return NULL;
}

const char *
gbp_shortcutui_shortcut_get_subtitle (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  /* TODO: Get from menu info */

  return NULL;
}

static GtkShortcutTrigger *
get_trigger (GbpShortcutuiShortcut *self)
{
  /* TODO: Lookup override */

  return GET_INFO (self->shortcut)->trigger;
}

char *
gbp_shortcutui_shortcut_dup_accelerator (GbpShortcutuiShortcut *self)
{
  GtkShortcutTrigger *trigger;

  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  trigger = get_trigger (self);

  if (!trigger || GTK_IS_NEVER_TRIGGER (trigger))
    return NULL;

  return gtk_shortcut_trigger_to_string (trigger);
}

gboolean
gbp_shortcutui_shortcut_has_override (GbpShortcutuiShortcut *self)
{
  GtkShortcutTrigger *trigger;

  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), FALSE);

  trigger = get_trigger (self);

  return trigger != GET_INFO (self->shortcut)->trigger;
}

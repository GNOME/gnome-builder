/* gbp-shortcutui-action.c
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

#define G_LOG_DOMAIN "gbp-shortcutui-action"

#include "config.h"

#include "gbp-shortcutui-action.h"

struct _GbpShortcutuiAction
{
  GObject parent_instance;

  char *accelerator;
  char *action_name;
  char *group;
  char *page;
  char *search_text;
  char *subtitle;
  char *title;

  GVariant *action_target;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  PROP_SUBTITLE,
  PROP_TITLE,
  PROP_GROUP,
  PROP_PAGE,
  PROP_SEARCH_TEXT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiAction, gbp_shortcutui_action, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static const char *
gbp_shortcutui_action_get_search_text (GbpShortcutuiAction *self)
{
  g_assert (GBP_IS_SHORTCUTUI_ACTION (self));

  if (self->search_text == NULL)
    {
      GString *str = g_string_new (NULL);

      if (self->page != NULL)
        g_string_append_printf (str, "%s ", self->page);

      if (self->group != NULL)
        g_string_append_printf (str, "%s ", self->group);

      if (self->title != NULL)
        g_string_append_printf (str, "%s ", self->title);

      if (self->subtitle != NULL)
        g_string_append_printf (str, "%s ", self->subtitle);

      self->search_text = g_string_free (str, FALSE);
    }

  return self->search_text;
}

static void
gbp_shortcutui_action_dispose (GObject *object)
{
  GbpShortcutuiAction *self = (GbpShortcutuiAction *)object;

  g_clear_pointer (&self->accelerator, g_free);
  g_clear_pointer (&self->action_name, g_free);
  g_clear_pointer (&self->action_target, g_variant_unref);
  g_clear_pointer (&self->group, g_free);
  g_clear_pointer (&self->page, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->search_text, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (gbp_shortcutui_action_parent_class)->dispose (object);
}

static void
gbp_shortcutui_action_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpShortcutuiAction *self = GBP_SHORTCUTUI_ACTION (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, self->accelerator);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, self->action_name);
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, self->action_target);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, self->subtitle);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_GROUP:
      g_value_set_string (value, self->group);
      break;

    case PROP_PAGE:
      g_value_set_string (value, self->page);
      break;

    case PROP_SEARCH_TEXT:
      g_value_set_string (value, gbp_shortcutui_action_get_search_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_action_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpShortcutuiAction *self = GBP_SHORTCUTUI_ACTION (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      self->accelerator = g_value_dup_string (value);
      break;

    case PROP_ACTION_NAME:
      self->action_name = g_value_dup_string (value);
      break;

    case PROP_ACTION_TARGET:
      self->action_target = g_value_dup_variant (value);
      break;

    case PROP_SUBTITLE:
      self->subtitle = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    case PROP_GROUP:
      self->group = g_value_dup_string (value);
      break;

    case PROP_PAGE:
      self->page = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_action_class_init (GbpShortcutuiActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_shortcutui_action_dispose;
  object_class->get_property = gbp_shortcutui_action_get_property;
  object_class->set_property = gbp_shortcutui_action_set_property;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACTION_NAME] =
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACTION_TARGET] =
    g_param_spec_variant ("action-target", NULL, NULL,
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_GROUP] =
    g_param_spec_string ("group", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PAGE] =
    g_param_spec_string ("page", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shortcutui_action_init (GbpShortcutuiAction *self)
{
}

static inline int
utf8_collate0 (const char *a,
               const char *b)
{
  if (a == b)
    return 0;

  if (a == NULL)
    return 1;

  if (b == NULL)
    return -1;

  return g_utf8_collate (a, b);
}

int
gbp_shortcutui_action_compare (const GbpShortcutuiAction *a,
                               const GbpShortcutuiAction *b)
{
  int ret;

  if ((ret = utf8_collate0 (a->page, b->page)))
    return ret;

  if ((ret = utf8_collate0 (a->group, b->group)))
    return ret;

  if ((ret = utf8_collate0 (a->title, b->title)))
    return ret;

  return 0;
}

const char *
gbp_shortcutui_action_get_accelerator (const GbpShortcutuiAction *self)
{
  return self->accelerator;
}

gboolean
gbp_shortcutui_action_is_same_group (const GbpShortcutuiAction *a,
                                     const GbpShortcutuiAction *b)
{
  return utf8_collate0 (a->page, b->page) == 0 &&
         utf8_collate0 (a->group, b->group) == 0;
}

const char *
gbp_shortcutui_action_get_page (const GbpShortcutuiAction *self)
{
  return self->page;
}

const char *
gbp_shortcutui_action_get_group (const GbpShortcutuiAction *self)
{
  return self->group;
}

const char *
gbp_shortcutui_action_get_action_name (const GbpShortcutuiAction *self)
{
  return self->action_name;
}

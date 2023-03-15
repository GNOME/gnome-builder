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

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "ide-application-private.h"
#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-manager-private.h"

#include "gbp-shortcutui-shortcut.h"

#define GET_INFO(o) ((IdeShortcut*)g_object_get_data(G_OBJECT(GTK_SHORTCUT(o)), "IDE_SHORTCUT"))

struct _GbpShortcutuiShortcut
{
  GObject      parent_instance;

  GtkShortcut *shortcut;
  char        *search_text;
  char        *title;

  const char  *id;
  const char  *group;
  const char  *page;
  const char  *subtitle;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiShortcut, gbp_shortcutui_shortcut, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_GROUP,
  PROP_HAS_OVERRIDE,
  PROP_PAGE,
  PROP_SEARCH_TEXT,
  PROP_SHORTCUT,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_shortcutui_shortcut_notify_trigger_cb (GbpShortcutuiShortcut *self,
                                           GParamSpec            *pspec,
                                           GtkShortcut           *shortcut)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_SHORTCUT (self));
  g_assert (GTK_IS_SHORTCUT (shortcut));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_OVERRIDE]);
}

static char *
strip_underline (const char *str)
{
  GString *gstr = g_string_new (str);
  g_string_replace (gstr, "_", "", 0);
  return g_string_free (gstr, FALSE);
}

static void
gbp_shortcutui_shortcut_constructed (GObject *object)
{
  GbpShortcutuiShortcut *self = (GbpShortcutuiShortcut *)object;
  g_autofree char *description = NULL;
  g_autofree char *label = NULL;
  IdeMenuManager *menu_manager;
  const char *id;
  GMenu *menu;
  guint position;

  G_OBJECT_CLASS (gbp_shortcutui_shortcut_parent_class)->constructed (object);

  if (self->shortcut == NULL)
    {
      g_critical ("Attempt to create %s without a shortcut!",
                  G_OBJECT_TYPE_NAME (self));
      return;
    }

  id = GET_INFO (self->shortcut)->id;
  menu_manager = IDE_APPLICATION_DEFAULT->menu_manager;
  menu = ide_menu_manager_find_item_by_id (menu_manager, id, &position);

  if (menu == NULL)
    {
      g_warning ("No menu information found for shortcut id \"%s\". "
                 "Add to menu-search.", id);
      return;
    }

  g_menu_model_get_item_attribute (G_MENU_MODEL (menu), position, "label", "s", &label);
  g_menu_model_get_item_attribute (G_MENU_MODEL (menu), position, "description", "s", &description);

  self->title = strip_underline (label);
  self->subtitle = g_intern_string (description);
  self->id = g_intern_string (id);
  self->search_text = g_strdup_printf ("%s %s %s %s",
                                       self->page ? self->page : "",
                                       self->group ? self->group : "",
                                       self->title ? self->title : "",
                                       self->subtitle ? self->subtitle : "");

  g_signal_connect_object (self->shortcut,
                           "notify::trigger",
                           G_CALLBACK (gbp_shortcutui_shortcut_notify_trigger_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_shortcutui_shortcut_dispose (GObject *object)
{
  GbpShortcutuiShortcut *self = (GbpShortcutuiShortcut *)object;

  g_clear_object (&self->shortcut);
  g_clear_pointer (&self->search_text, g_free);
  g_clear_pointer (&self->title, g_free);

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

    case PROP_GROUP:
      g_value_set_string (value, gbp_shortcutui_shortcut_get_group (self));
      break;

    case PROP_HAS_OVERRIDE:
      g_value_set_boolean (value, gbp_shortcutui_shortcut_has_override (self));
      break;

    case PROP_PAGE:
      g_value_set_string (value, gbp_shortcutui_shortcut_get_page (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gbp_shortcutui_shortcut_get_title (self));
      break;

    case PROP_SEARCH_TEXT:
      g_value_set_string (value, self->search_text);
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
    case PROP_GROUP:
      self->group = g_intern_string (g_value_get_string (value));
      break;

    case PROP_PAGE:
      self->page = g_intern_string (g_value_get_string (value));
      break;

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

  object_class->constructed = gbp_shortcutui_shortcut_constructed;
  object_class->dispose = gbp_shortcutui_shortcut_dispose;
  object_class->get_property = gbp_shortcutui_shortcut_get_property;
  object_class->set_property = gbp_shortcutui_shortcut_set_property;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORTCUT] =
    g_param_spec_object ("shortcut", NULL, NULL,
                         GTK_TYPE_SHORTCUT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_OVERRIDE] =
    g_param_spec_boolean ("has-override", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                        NULL,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PAGE] =
    g_param_spec_string ("page", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_GROUP] =
    g_param_spec_string ("group", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shortcutui_shortcut_init (GbpShortcutuiShortcut *self)
{
}

GbpShortcutuiShortcut *
gbp_shortcutui_shortcut_new (GtkShortcut *shortcut,
                             const char  *page,
                             const char  *group)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (shortcut), NULL);
  g_return_val_if_fail (GET_INFO (shortcut) != NULL, NULL);

  return g_object_new (GBP_TYPE_SHORTCUTUI_SHORTCUT,
                       "group", group,
                       "page", page,
                       "shortcut", shortcut,
                       NULL);
}

const char *
gbp_shortcutui_shortcut_get_title (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  return self->title;
}

const char *
gbp_shortcutui_shortcut_get_subtitle (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  return self->subtitle;
}

static GtkShortcutTrigger *
get_trigger (GbpShortcutuiShortcut *self)
{
  GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (self->shortcut);

  if (GTK_IS_NEVER_TRIGGER (trigger))
    return NULL;

  return trigger;
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
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), FALSE);

  return gtk_shortcut_get_trigger (self->shortcut) != GET_INFO (self->shortcut)->trigger;
}

gboolean
gbp_shortcutui_shortcut_override (GbpShortcutuiShortcut  *self,
                                  const char             *accelerator,
                                  GError                **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), FALSE);

  ret = ide_shortcut_bundle_override (ide_shortcut_manager_get_user_bundle (),
                                      GET_INFO (self->shortcut)->id,
                                      accelerator,
                                      error);

  IDE_RETURN (ret);
}

const char *
gbp_shortcutui_shortcut_get_page (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  return self->page ? self->page : _("Other");
}

const char *
gbp_shortcutui_shortcut_get_group (GbpShortcutuiShortcut *self)
{
  g_return_val_if_fail (GBP_IS_SHORTCUTUI_SHORTCUT (self), NULL);

  return self->group ? self->group : _("Other");
}

int
gbp_shortcutui_shortcut_compare (const GbpShortcutuiShortcut *a,
                                 const GbpShortcutuiShortcut *b)
{
  const char *page_a = a->page ? a->page : _("Other");
  const char *page_b = b->page ? b->page : _("Other");
  const char *group_a = a->group ? a->group : _("Other");
  const char *group_b = b->group ? b->group : _("Other");
  int r;

  r = g_strcmp0 (page_a, page_b);

  if (r == 0)
    r = g_strcmp0 (group_a, group_b);

  if (ide_str_equal0 (a->id, b->id))
    return 0;

  if (r == 0)
    r = g_strcmp0 (a->title, b->title);

  return r;
}

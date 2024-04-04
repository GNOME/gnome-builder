/*
 * manuals-sdk.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include "manuals-book.h"
#include "manuals-repository.h"
#include "manuals-sdk.h"
#include "manuals-utils.h"

struct _ManualsSdk
{
  GomResource parent_instance;
  gint64 id;
  char *icon_name;
  char *kind;
  char *name;
  char *online_uri;
  char *uri;
  char *version;
};

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_KIND,
  PROP_NAME,
  PROP_ONLINE_URI,
  PROP_TITLE,
  PROP_URI,
  PROP_VERSION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (ManualsSdk, manuals_sdk, GOM_TYPE_RESOURCE)

static GParamSpec *properties[N_PROPS];
static char *system_title;
static char *system_icon_name;

static void
manuals_sdk_finalize (GObject *object)
{
  ManualsSdk *self = (ManualsSdk *)object;

  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->online_uri, g_free);
  g_clear_pointer (&self->kind, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->version, g_free);

  G_OBJECT_CLASS (manuals_sdk_parent_class)->finalize (object);
}

static void
manuals_sdk_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ManualsSdk *self = MANUALS_SDK (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, manuals_sdk_get_icon_name (self));
      break;

    case PROP_ID:
      g_value_set_int64 (value, manuals_sdk_get_id (self));
      break;

    case PROP_KIND:
      g_value_set_string (value, manuals_sdk_get_kind (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, manuals_sdk_get_name (self));
      break;

    case PROP_ONLINE_URI:
      g_value_set_string (value, manuals_sdk_get_online_uri (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, manuals_sdk_dup_title (self));
      break;

    case PROP_URI:
      g_value_set_string (value, manuals_sdk_get_uri (self));
      break;

    case PROP_VERSION:
      g_value_set_string (value, manuals_sdk_get_version (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_sdk_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ManualsSdk *self = MANUALS_SDK (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      manuals_sdk_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ID:
      manuals_sdk_set_id (self, g_value_get_int64 (value));
      break;

    case PROP_KIND:
      manuals_sdk_set_kind (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      manuals_sdk_set_name (self, g_value_get_string (value));
      break;

    case PROP_ONLINE_URI:
      manuals_sdk_set_online_uri (self, g_value_get_string (value));
      break;

    case PROP_URI:
      manuals_sdk_set_uri (self, g_value_get_string (value));
      break;

    case PROP_VERSION:
      manuals_sdk_set_version (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_sdk_class_init (ManualsSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomResourceClass *resource_class = GOM_RESOURCE_CLASS (klass);

  object_class->finalize = manuals_sdk_finalize;
  object_class->get_property = manuals_sdk_get_property;
  object_class->set_property = manuals_sdk_set_property;

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLINE_URI] =
    g_param_spec_string ("online-uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_VERSION] =
    g_param_spec_string ("version", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_resource_class_set_table (resource_class, "sdks");
  gom_resource_class_set_primary_key (resource_class, "id");
  gom_resource_class_set_unique (resource_class, "uri");
  gom_resource_class_set_property_set_mapped (resource_class, "title", FALSE);

  system_icon_name = manuals_get_os_info ("LOGO");
  system_title = manuals_get_os_info (G_OS_INFO_KEY_NAME);
}

static void
manuals_sdk_init (ManualsSdk *self)
{
}

const char *
manuals_sdk_get_online_uri (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  return self->online_uri;
}

void
manuals_sdk_set_online_uri (ManualsSdk *self,
                            const char *online_uri)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->online_uri, online_uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ONLINE_URI]);
}

char *
manuals_sdk_dup_title (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  if (g_strcmp0 (self->kind, "host") == 0)
    {
      if (g_strcmp0 (self->name, "GNOME") == 0)
        return g_strdup ("GNOME OS");

      return g_strdup (system_title);
    }

  if (g_strcmp0 (self->kind, "flatpak") == 0)
    {
      if (g_strcmp0 (self->name, "org.gnome.Sdk.Docs") == 0)
        {
          if (g_strcmp0 (self->version, "master") == 0)
            return g_strdup (_("GNOME Nightly"));

          return g_strdup_printf ("GNOME %s", self->version);
        }
    }

  return g_strdup (self->name);
}

const char *
manuals_sdk_get_uri (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  return self->uri;
}

void
manuals_sdk_set_uri (ManualsSdk *self,
                     const char *uri)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->uri, uri))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

const char *
manuals_sdk_get_version (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  return self->version;
}

void
manuals_sdk_set_version (ManualsSdk *self,
                         const char *version)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->version, version))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VERSION]);
}

gint64
manuals_sdk_get_id (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), 0);

  return self->id;
}

void
manuals_sdk_set_id (ManualsSdk *self,
                    gint64      id)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (id != self->id)
    {
      self->id = id;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
    }
}

const char *
manuals_sdk_get_icon_name (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  if (self->icon_name == NULL)
    {
      if (g_strcmp0 (self->kind, "host") == 0)
        return system_icon_name;

      if (g_strcmp0 (self->kind, "jhbuild") == 0)
        return "utilities-terminal-symbolic";
    }

  if (g_strcmp0 (self->name, "org.gnome.Sdk.Docs") == 0)
    return "org.gnome.Sdk-symbolic";

  return self->icon_name;
}

void
manuals_sdk_set_icon_name (ManualsSdk *self,
                           const char *icon_name)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->icon_name, icon_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

const char *
manuals_sdk_get_kind (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  return self->kind;
}

void
manuals_sdk_set_kind (ManualsSdk *self,
                      const char *kind)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->kind, kind))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KIND]);
}

const char *
manuals_sdk_get_name (ManualsSdk *self)
{
  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  return self->name;
}

void
manuals_sdk_set_name (ManualsSdk *self,
                      const char *name)
{
  g_return_if_fail (MANUALS_IS_SDK (self));

  if (g_set_str (&self->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

DexFuture *
manuals_sdk_list_books (ManualsSdk *self)
{
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GomFilter) filter = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  GomSorting *sorting;
  DexFuture *future;

  g_return_val_if_fail (MANUALS_IS_SDK (self), NULL);

  g_object_get (self, "repository", &repository, NULL);

  if (repository == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "No repository to query");

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, self->id);
  filter = gom_filter_new_eq (MANUALS_TYPE_BOOK, "sdk-id", &value);

  sorting = gom_sorting_new (MANUALS_TYPE_BOOK, "title", GOM_SORTING_ASCENDING,
                             G_TYPE_INVALID);
  future = manuals_repository_list_sorted (repository, MANUALS_TYPE_BOOK, filter, sorting);
  g_clear_object (&sorting);

  return future;
}

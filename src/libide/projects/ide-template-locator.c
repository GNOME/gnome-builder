/* ide-template-locator.c
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

#define G_LOG_DOMAIN "ide-template-locator"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include <libide-code.h>

#include "ide-template-locator.h"

typedef struct
{
  char *license_text;
} IdeTemplateLocatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTemplateLocator, ide_template_locator, TMPL_TYPE_TEMPLATE_LOCATOR)

enum {
  PROP_0,
  PROP_LICENSE_TEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GInputStream *
ide_template_locator_locate (TmplTemplateLocator  *locator,
                             const char           *path,
                             GError              **error)
{
  IdeTemplateLocator *self = (IdeTemplateLocator *)locator;
  IdeTemplateLocatorPrivate *priv = ide_template_locator_get_instance_private (self);

  g_assert (IDE_IS_TEMPLATE_LOCATOR (self));
  g_assert (path != NULL);
  g_assert (priv->license_text == NULL || g_utf8_validate (priv->license_text, -1, NULL));

  if (g_str_has_prefix (path, "license."))
    {
      GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default ();
      GtkSourceLanguage *language = gtk_source_language_manager_guess_language (manager, path, NULL);

      if (priv->license_text != NULL && language != NULL)
        {
          g_autofree char *header = ide_language_format_header (language, priv->license_text);
          gsize len = strlen (header);

          g_assert (g_utf8_validate (header, -1, NULL));

          return g_memory_input_stream_new_from_data (g_steal_pointer (&header), len, g_free);
        }

      /* We don't want to fail here just because we didn't have any
       * license text to expand into a header.
       */
      return g_memory_input_stream_new ();
    }

  return TMPL_TEMPLATE_LOCATOR_CLASS (ide_template_locator_parent_class)->locate (locator, path, error);
}

static void
ide_template_locator_dispose (GObject *object)
{
  IdeTemplateLocator *self = (IdeTemplateLocator *)object;
  IdeTemplateLocatorPrivate *priv = ide_template_locator_get_instance_private (self);

  g_clear_pointer (&priv->license_text, g_free);

  G_OBJECT_CLASS (ide_template_locator_parent_class)->dispose (object);
}

static void
ide_template_locator_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeTemplateLocator *self = IDE_TEMPLATE_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_LICENSE_TEXT:
      g_value_set_string (value, ide_template_locator_get_license_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_template_locator_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeTemplateLocator *self = IDE_TEMPLATE_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_LICENSE_TEXT:
      ide_template_locator_set_license_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_template_locator_class_init (IdeTemplateLocatorClass *klass)
{
  TmplTemplateLocatorClass *locator_class = TMPL_TEMPLATE_LOCATOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_template_locator_dispose;
  object_class->get_property = ide_template_locator_get_property;
  object_class->set_property = ide_template_locator_set_property;

  locator_class->locate = ide_template_locator_locate;

  properties [PROP_LICENSE_TEXT] =
    g_param_spec_string ("license-text",
                         "License Text",
                         "The text of the license to include in headers",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_template_locator_init (IdeTemplateLocator *self)
{
}

const char *
ide_template_locator_get_license_text (IdeTemplateLocator *self)
{
  IdeTemplateLocatorPrivate *priv = ide_template_locator_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEMPLATE_LOCATOR (self), NULL);

  return priv->license_text;
}

void
ide_template_locator_set_license_text (IdeTemplateLocator *self,
                                       const char         *license_text)
{
  IdeTemplateLocatorPrivate *priv = ide_template_locator_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEMPLATE_LOCATOR (self));

  if (g_set_str (&priv->license_text, license_text))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LICENSE_TEXT]);
    }
}

IdeTemplateLocator *
ide_template_locator_new (void)
{
  return g_object_new (IDE_TYPE_TEMPLATE_LOCATOR, NULL);
}

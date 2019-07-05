/* ide-docs-query.c
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-docs-query"

#include "config.h"

#include "ide-docs-query.h"

struct _IdeDocsQuery
{
  GObject parent_instance;
  gchar *keyword;
  gchar *fuzzy;
  gchar *sdk;
  gchar *language;
};

G_DEFINE_TYPE (IdeDocsQuery, ide_docs_query, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_KEYWORD,
  PROP_SDK,
  PROP_LANGUAGE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_docs_query_new:
 *
 * Create a new #IdeDocsQuery.
 *
 * Returns: (transfer full): a newly created #IdeDocsQuery
 */
IdeDocsQuery *
ide_docs_query_new (void)
{
  return g_object_new (IDE_TYPE_DOCS_QUERY, NULL);
}

static void
ide_docs_query_finalize (GObject *object)
{
  IdeDocsQuery *self = (IdeDocsQuery *)object;

  g_clear_pointer (&self->keyword, g_free);
  g_clear_pointer (&self->fuzzy, g_free);
  g_clear_pointer (&self->sdk, g_free);
  g_clear_pointer (&self->language, g_free);

  G_OBJECT_CLASS (ide_docs_query_parent_class)->finalize (object);
}

static void
ide_docs_query_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeDocsQuery *self = IDE_DOCS_QUERY (object);

  switch (prop_id)
    {
    case PROP_KEYWORD:
      g_value_set_string (value, ide_docs_query_get_keyword (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, ide_docs_query_get_language (self));
      break;

    case PROP_SDK:
      g_value_set_string (value, ide_docs_query_get_sdk (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_query_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeDocsQuery *self = IDE_DOCS_QUERY (object);

  switch (prop_id)
    {
    case PROP_KEYWORD:
      ide_docs_query_set_keyword (self, g_value_get_string (value));
      break;

    case PROP_LANGUAGE:
      ide_docs_query_set_language (self, g_value_get_string (value));
      break;

    case PROP_SDK:
      ide_docs_query_set_sdk (self, g_value_get_string (value));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_query_class_init (IdeDocsQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_docs_query_finalize;
  object_class->get_property = ide_docs_query_get_property;
  object_class->set_property = ide_docs_query_set_property;
  
  properties [PROP_KEYWORD] =
    g_param_spec_string ("keyword",
                         "Keyword",
                         "Keyword",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  
  properties [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         "Language",
                         "Language",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  
  properties [PROP_SDK] =
    g_param_spec_string ("sdk",
                         "SDK",
                         "SDK",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_docs_query_init (IdeDocsQuery *self)
{
}

const gchar *
ide_docs_query_get_keyword (IdeDocsQuery *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_QUERY (self), NULL);

  return self->keyword;
}

void
ide_docs_query_set_keyword (IdeDocsQuery *self,
                            const gchar  *keyword)
{
  g_return_if_fail (IDE_IS_DOCS_QUERY (self));

  if (g_strcmp0 (keyword, self->keyword) != 0)
    {
      g_free (self->keyword);
      self->keyword = g_strdup (keyword);

      if (keyword != NULL)
        {
          GString *str = g_string_new (NULL);

          for (; *keyword; keyword = g_utf8_next_char (keyword))
            {
              gunichar ch = g_utf8_get_char (keyword);

              if (!g_unichar_isspace (ch))
                g_string_append_unichar (str, ch);
            }

          g_free (self->fuzzy);
          self->fuzzy = g_string_free (str, FALSE);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEYWORD]);
    }
}

const gchar *
ide_docs_query_get_sdk (IdeDocsQuery *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_QUERY (self), NULL);

  return self->sdk;
}

void
ide_docs_query_set_sdk (IdeDocsQuery *self,
                        const gchar  *sdk)
{
  g_return_if_fail (IDE_IS_DOCS_QUERY (self));

  if (g_strcmp0 (sdk, self->sdk) != 0)
    {
      g_free (self->sdk);
      self->sdk = g_strdup (sdk);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
    }
}

const gchar *
ide_docs_query_get_language (IdeDocsQuery *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_QUERY (self), NULL);

  return self->language;
}

void
ide_docs_query_set_language (IdeDocsQuery *self,
                             const gchar  *language)
{
  g_return_if_fail (IDE_IS_DOCS_QUERY (self));

  if (g_strcmp0 (language, self->language) != 0)
    {
      g_free (self->language);
      self->language = g_strdup (language);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

const gchar *
ide_docs_query_get_fuzzy (IdeDocsQuery *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_QUERY (self), NULL);

  return self->fuzzy;
}

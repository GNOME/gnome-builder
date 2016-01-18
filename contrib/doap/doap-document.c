/* doap-document.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "doap"

#include <glib/gi18n.h>

#include "doap-document.h"

#include "xml-reader.h"

/*
 * TODO: We don't do any XMLNS checking or anything here.
 */

struct _DoapDocument
{
  GObject parent_instance;

  gchar     *bug_database;
  gchar     *category;
  gchar     *description;
  gchar     *download_page;
  gchar     *homepage;;
  gchar     *name;
  gchar     *shortdesc;

  GPtrArray *languages;
  GList     *maintainers;
};

G_DEFINE_QUARK (doap_document_error, doap_document_error)
G_DEFINE_TYPE (DoapDocument, doap_document, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUG_DATABASE,
  PROP_CATEGORY,
  PROP_DESCRIPTION,
  PROP_DOWNLOAD_PAGE,
  PROP_HOMEPAGE,
  PROP_LANGUAGES,
  PROP_NAME,
  PROP_SHORTDESC,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

DoapDocument *
doap_document_new (void)
{
  return g_object_new (DOAP_TYPE_DOCUMENT, NULL);
}

const gchar *
doap_document_get_name (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->name;
}

const gchar *
doap_document_get_shortdesc (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->shortdesc;
}

const gchar *
doap_document_get_description (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->description;
}

const gchar *
doap_document_get_bug_database (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->bug_database;
}

const gchar *
doap_document_get_download_page (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->download_page;
}

const gchar *
doap_document_get_homepage (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->homepage;
}

const gchar *
doap_document_get_category (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->category;
}

/**
 * doap_document_get_languages:
 *
 * Returns: (transfer none): A #GStrv.
 */
gchar **
doap_document_get_languages (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  if (self->languages != NULL)
    return (gchar **)self->languages->pdata;

  return NULL;
}

static void
doap_document_set_bug_database (DoapDocument *self,
                                const gchar  *bug_database)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->bug_database, bug_database) != 0)
    {
      g_free (self->bug_database);
      self->bug_database = g_strdup (bug_database);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUG_DATABASE]);
    }
}

static void
doap_document_set_category (DoapDocument *self,
                            const gchar  *category)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->category, category) != 0)
    {
      g_free (self->category);
      self->category = g_strdup (category);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CATEGORY]);
    }
}

static void
doap_document_set_description (DoapDocument *self,
                               const gchar  *description)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->description, description) != 0)
    {
      g_free (self->description);
      self->description = g_strdup (description);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DESCRIPTION]);
    }
}

static void
doap_document_set_download_page (DoapDocument *self,
                                 const gchar  *download_page)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->download_page, download_page) != 0)
    {
      g_free (self->download_page);
      self->download_page = g_strdup (download_page);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DOWNLOAD_PAGE]);
    }
}

static void
doap_document_set_homepage (DoapDocument *self,
                            const gchar  *homepage)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->homepage, homepage) != 0)
    {
      g_free (self->homepage);
      self->homepage = g_strdup (homepage);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HOMEPAGE]);
    }
}

static void
doap_document_set_name (DoapDocument *self,
                        const gchar  *name)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->name, name) != 0)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

static void
doap_document_set_shortdesc (DoapDocument *self,
                             const gchar  *shortdesc)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if (g_strcmp0 (self->shortdesc, shortdesc) != 0)
    {
      g_free (self->shortdesc);
      self->shortdesc = g_strdelimit (g_strdup (shortdesc), "\n", ' ');
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHORTDESC]);
    }
}

/**
 * doap_document_get_maintainers:
 *
 *
 *
 * Returns: (transfer none) (element-type DoapDocumentPerson*): A #GList of #DoapDocumentPerson.
 */
GList *
doap_document_get_maintainers (DoapDocument *self)
{
  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), NULL);

  return self->maintainers;
}

static void
doap_document_add_language (DoapDocument *self,
                            const gchar  *language)
{
  g_return_if_fail (DOAP_IS_DOCUMENT (self));
  g_return_if_fail (language != NULL);

  if (self->languages == NULL)
    {
      self->languages = g_ptr_array_new_with_free_func (g_free);
      g_ptr_array_add (self->languages, NULL);
    }

  g_assert (self->languages->len > 0);

  g_ptr_array_index (self->languages, self->languages->len - 1) = g_strdup (language);
  g_ptr_array_add (self->languages, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGES]);
}

static void
doap_document_set_languages (DoapDocument  *self,
                             gchar        **languages)
{
  gsize i;

  g_return_if_fail (DOAP_IS_DOCUMENT (self));

  if ((self->languages != NULL) && (self->languages->len > 0))
    g_ptr_array_remove_range (self->languages, 0, self->languages->len);

  g_object_freeze_notify (G_OBJECT (self));
  for (i = 0; languages [i]; i++)
    doap_document_add_language (self, languages [i]);
  g_object_thaw_notify (G_OBJECT (self));
}

static void
doap_document_finalize (GObject *object)
{
  DoapDocument *self = (DoapDocument *)object;

  g_clear_pointer (&self->bug_database, g_free);
  g_clear_pointer (&self->category, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->download_page, g_free);
  g_clear_pointer (&self->homepage, g_free);
  g_clear_pointer (&self->languages, g_ptr_array_unref);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->shortdesc, g_free);

  g_list_free_full (self->maintainers, g_object_unref);
  self->maintainers = NULL;

  G_OBJECT_CLASS (doap_document_parent_class)->finalize (object);
}

static void
doap_document_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  DoapDocument *self = DOAP_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUG_DATABASE:
      g_value_set_string (value, doap_document_get_bug_database (self));
      break;

    case PROP_CATEGORY:
      g_value_set_string (value, doap_document_get_category (self));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, doap_document_get_description (self));
      break;

    case PROP_DOWNLOAD_PAGE:
      g_value_set_string (value, doap_document_get_download_page (self));
      break;

    case PROP_HOMEPAGE:
      g_value_set_string (value, doap_document_get_homepage (self));
      break;

    case PROP_LANGUAGES:
      g_value_set_boxed (value, doap_document_get_languages (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, doap_document_get_name (self));
      break;

    case PROP_SHORTDESC:
      g_value_set_string (value, doap_document_get_shortdesc (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
doap_document_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  DoapDocument *self = DOAP_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_BUG_DATABASE:
      doap_document_set_bug_database (self, g_value_get_string (value));
      break;

    case PROP_CATEGORY:
      doap_document_set_category (self, g_value_get_string (value));
      break;

    case PROP_DESCRIPTION:
      doap_document_set_description (self, g_value_get_string (value));
      break;

    case PROP_DOWNLOAD_PAGE:
      doap_document_set_download_page (self, g_value_get_string (value));
      break;

    case PROP_HOMEPAGE:
      doap_document_set_homepage (self, g_value_get_string (value));
      break;

    case PROP_LANGUAGES:
      doap_document_set_languages (self, g_value_get_boxed (value));
      break;

    case PROP_NAME:
      doap_document_set_name (self, g_value_get_string (value));
      break;

    case PROP_SHORTDESC:
      doap_document_set_shortdesc (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
doap_document_class_init (DoapDocumentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = doap_document_finalize;
  object_class->get_property = doap_document_get_property;
  object_class->set_property = doap_document_set_property;

  properties [PROP_BUG_DATABASE] =
    g_param_spec_string ("bug-database",
                         "Bug Database",
                         "Bug Database",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CATEGORY] =
    g_param_spec_string ("category",
                         "Category",
                         "Category",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "Description",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DOWNLOAD_PAGE] =
    g_param_spec_string ("download-page",
                         "Download Page",
                         "Download Page",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOMEPAGE] =
    g_param_spec_string ("homepage",
                         "Homepage",
                         "Homepage",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGES] =
    g_param_spec_string ("languages",
                         "Languages",
                         "Languages",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORTDESC] =
    g_param_spec_string ("shortdesc",
                         "Shortdesc",
                         "Shortdesc",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
doap_document_init (DoapDocument *self)
{
}

static gboolean
doap_document_parse_maintainer (DoapDocument *self,
                                XmlReader    *reader)
{
  g_assert (DOAP_IS_DOCUMENT (self));
  g_assert (XML_IS_READER (reader));

  if (!xml_reader_read (reader))
    return FALSE;

  do
    {
      if (xml_reader_is_a_local (reader, "Person") && xml_reader_read (reader))
        {
          g_autoptr(DoapPerson) person = doap_person_new ();

          do
            {
              if (xml_reader_is_a_local (reader, "name"))
                {
                  doap_person_set_name (person, xml_reader_read_string (reader));
                }
              else if (xml_reader_is_a_local (reader, "mbox"))
                {
                  gchar *str;

                  str = xml_reader_get_attribute (reader, "rdf:resource");
                  if (str != NULL && str[0] != '\0' && g_str_has_prefix (str, "mailto:"))
                    doap_person_set_email (person, str + strlen ("mailto:"));
                  g_free (str);
                }
            }
          while (xml_reader_read_to_next (reader));

          if (doap_person_get_name (person) || doap_person_get_email (person))
            self->maintainers = g_list_append (self->maintainers, g_object_ref (person));
        }
    }
  while (xml_reader_read_to_next (reader));

  return TRUE;
}

gboolean
doap_document_load_from_file (DoapDocument  *self,
                              GFile         *file,
                              GCancellable  *cancellable,
                              GError       **error)
{
  g_autoptr(XmlReader) reader = NULL;

  g_return_val_if_fail (DOAP_IS_DOCUMENT (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  reader = xml_reader_new ();

  if (!xml_reader_load_from_file (reader, file, cancellable, error))
    return FALSE;

  if (!xml_reader_read_start_element (reader, "Project"))
    {
      g_set_error (error,
                   DOAP_DOCUMENT_ERROR,
                   DOAP_DOCUMENT_ERROR_INVALID_FORMAT,
                   "Project element is missing from doap.");
      return FALSE;
    }

  g_object_freeze_notify (G_OBJECT (self));

  xml_reader_read (reader);

  do
    {
      const gchar *element_name;

      element_name = xml_reader_get_local_name (reader);

      if (g_strcmp0 (element_name, "name") == 0 ||
          g_strcmp0 (element_name, "shortdesc") == 0 ||
          g_strcmp0 (element_name, "description") == 0)
        {
          gchar *str;

          str = xml_reader_read_string (reader);
          if (str != NULL)
            g_object_set (self, element_name, g_strstrip (str), NULL);
          g_free (str);
        }
      else if (g_strcmp0 (element_name, "category") == 0 ||
               g_strcmp0 (element_name, "homepage") == 0 ||
               g_strcmp0 (element_name, "download-page") == 0 ||
               g_strcmp0 (element_name, "bug-database") == 0)
        {
          gchar *str;

          str = xml_reader_get_attribute (reader, "rdf:resource");
          if (str != NULL)
            g_object_set (self, element_name, g_strstrip (str), NULL);
          g_free (str);
        }
      else if (g_strcmp0 (element_name, "programming-language") == 0)
        {
          gchar *str;

          str = xml_reader_read_string (reader);
          if (str != NULL && str[0] != '\0')
            doap_document_add_language (self, g_strstrip (str));
          g_free (str);
        }
      else if (g_strcmp0 (element_name, "maintainer") == 0)
        {
          if (!doap_document_parse_maintainer (self, reader))
            break;
        }
    }
  while (xml_reader_read_to_next (reader));

  g_object_thaw_notify (G_OBJECT (self));

  return TRUE;
}

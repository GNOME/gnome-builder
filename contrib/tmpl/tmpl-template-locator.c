/* tmpl-template-locator.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "tmpl-error.h"
#include "tmpl-template-locator.h"

typedef struct
{
  GQueue  *search_path;
} TmplTemplateLocatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (TmplTemplateLocator, tmpl_template_locator, G_TYPE_OBJECT)

static GInputStream *
tmpl_template_locator_locate_in_path (TmplTemplateLocator *self,
                                      const gchar         *path_base,
                                      const gchar         *path)
{
  GInputStream *ret = NULL;
  gchar *full_path;

  g_assert (TMPL_IS_TEMPLATE_LOCATOR (self));
  g_assert (path_base != NULL);
  g_assert (path != NULL);

  full_path = g_build_path (path_base, path, NULL);

  if (g_str_has_prefix (full_path, "resource://"))
    {
      /*
       * A mediocre attempt to prevent escapes using ../
       */
      if (strstr (full_path, "..") == NULL)
        ret = g_resources_open_stream (full_path + strlen ("resource://"), 0, NULL);
    }
  else
    {
      GFile *parent = g_file_new_for_path (path_base);
      GFile *file = g_file_new_for_path (full_path);
      gchar *relative;

      /*
       * If the path tries to escape the search path, using ../../ or
       * something clever, we will get an invalid path here.
       */
      if ((relative = g_file_get_relative_path (parent, file)))
        {
          g_free (relative);
          ret = (GInputStream *)g_file_read (file, NULL, NULL);
        }

      g_object_unref (parent);
      g_object_unref (file);
    }

  g_free (full_path);

  return ret;
}

static GInputStream *
tmpl_template_locator_real_locate (TmplTemplateLocator  *self,
                                   const gchar          *path,
                                   GError              **error)
{
  TmplTemplateLocatorPrivate *priv = tmpl_template_locator_get_instance_private (self);
  GInputStream *ret = NULL;
  const GList *iter;
  const GList *search_path;

  g_assert (TMPL_IS_TEMPLATE_LOCATOR (self));
  g_assert (path != NULL);

  search_path = priv->search_path->head;

  for (iter = search_path; ret == NULL && iter != NULL; iter = iter->next)
    {
      const gchar *path_base = iter->data;

      ret = tmpl_template_locator_locate_in_path (self, path_base, path);
    }

  if (ret == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_TEMPLATE_NOT_FOUND,
                   _("Failed to locate template \"%s\""),
                   path);
      return NULL;
    }

  return ret;
}

static void
tmpl_template_locator_class_init (TmplTemplateLocatorClass *klass)
{
  klass->locate = tmpl_template_locator_real_locate;
}

static void
tmpl_template_locator_init (TmplTemplateLocator *self)
{
  TmplTemplateLocatorPrivate *priv = tmpl_template_locator_get_instance_private (self);

  priv->search_path = g_queue_new ();
}

void
tmpl_template_locator_append_search_path (TmplTemplateLocator *self,
                                          const gchar         *path)
{
  TmplTemplateLocatorPrivate *priv = tmpl_template_locator_get_instance_private (self);

  g_return_if_fail (TMPL_IS_TEMPLATE_LOCATOR (self));
  g_return_if_fail (path != NULL);

  g_queue_push_tail (priv->search_path, g_strdup (path));
}

void
tmpl_template_locator_prepend_search_path (TmplTemplateLocator *self,
                                           const gchar         *path)
{
  TmplTemplateLocatorPrivate *priv = tmpl_template_locator_get_instance_private (self);

  g_return_if_fail (TMPL_IS_TEMPLATE_LOCATOR (self));
  g_return_if_fail (path != NULL);

  g_queue_push_head (priv->search_path, g_strdup (path));
}

/**
 * tmpl_template_locator_get_search_path:
 * @self: A #TmplTemplateLocator
 *
 * Gets the current search path used by the template locator.
 *
 * Returns: (transfer full): A %NULL-terminated array of strings.
 */
gchar **
tmpl_template_locator_get_search_path (TmplTemplateLocator *self)
{
  TmplTemplateLocatorPrivate *priv = tmpl_template_locator_get_instance_private (self);
  GPtrArray *ar;
  const GList *iter;

  g_return_val_if_fail (TMPL_IS_TEMPLATE_LOCATOR (self), NULL);

  ar = g_ptr_array_new ();

  for (iter = priv->search_path->head; iter != NULL; iter = iter->next)
    {
      const gchar *path = iter->data;

      g_ptr_array_add (ar, g_strdup (path));
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

TmplTemplateLocator *
tmpl_template_locator_new (void)
{
  return g_object_new (TMPL_TYPE_TEMPLATE_LOCATOR, NULL);
}

/**
 * tmpl_template_locator_locate:
 * @self: A #TmplTemplateLocator.
 * @path: a relative path to the file
 *
 * This will resolve the relative path using the search paths found within
 * the template loader.
 *
 * Returns: (transfer full): A #GInputStream or %NULL and @error is set.
 */
GInputStream *
tmpl_template_locator_locate (TmplTemplateLocator  *self,
                              const gchar          *path,
                              GError              **error)
{
  g_return_val_if_fail (TMPL_IS_TEMPLATE_LOCATOR (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return TMPL_TEMPLATE_LOCATOR_GET_CLASS (self)->locate (self, path, error);
}

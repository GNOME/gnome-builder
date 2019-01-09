/* gbp-test-path.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-test-path"

#include "config.h"

#include "gbp-test-path.h"

struct _GbpTestPath
{
  GObject         parent_instance;
  IdeTestManager *test_manager;
  gchar          *path;
  gchar          *name;
};

enum {
  PROP_0,
  PROP_PATH,
  PROP_TEST_MANAGER,
  N_PROPS
};

G_DEFINE_TYPE (GbpTestPath, gbp_test_path, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
gbp_test_path_dispose (GObject *object)
{
  GbpTestPath *self = (GbpTestPath *)object;

  g_clear_object (&self->test_manager);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_test_path_parent_class)->dispose (object);
}

static void
gbp_test_path_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbpTestPath *self = GBP_TEST_PATH (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_TEST_MANAGER:
      g_value_set_object (value, self->test_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_test_path_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbpTestPath *self = GBP_TEST_PATH (object);

  switch (prop_id)
    {
    case PROP_PATH:
      if ((self->path = g_value_dup_string (value)))
        self->name = g_path_get_basename (self->path);
      break;

    case PROP_TEST_MANAGER:
      self->test_manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_test_path_class_init (GbpTestPathClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_test_path_dispose;
  object_class->get_property = gbp_test_path_get_property;
  object_class->set_property = gbp_test_path_set_property;

  properties [PROP_PATH] =
    g_param_spec_string ("path", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEST_MANAGER] =
    g_param_spec_object ("test-manager", NULL, NULL, IDE_TYPE_TEST_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_test_path_init (GbpTestPath *self)
{
}

GbpTestPath *
gbp_test_path_new (IdeTestManager *test_manager,
                   const gchar    *path)
{
  return g_object_new (GBP_TYPE_TEST_PATH,
                       "test-manager", test_manager,
                       "path", path,
                       NULL);
}

const gchar *
gbp_test_path_get_name (GbpTestPath *self)
{
  g_return_val_if_fail (GBP_IS_TEST_PATH (self), NULL);

  return self->name;
}

GPtrArray *
gbp_test_path_get_folders (GbpTestPath *self)
{
  GPtrArray *folders;
  g_auto(GStrv) dirs = NULL;

  g_return_val_if_fail (GBP_IS_TEST_PATH (self), NULL);

  folders = g_ptr_array_new ();

  dirs = ide_test_manager_get_folders (self->test_manager, self->path);

  for (guint i = 0; dirs[i]; i++)
    {
      g_autofree gchar *subdir = NULL;

      if (self->path == NULL)
        subdir = g_strdup (dirs[i]);
      else
        subdir = g_strjoin ("/", self->path, dirs[i], NULL);

      g_ptr_array_add (folders, gbp_test_path_new (self->test_manager, subdir));
    }

  return g_steal_pointer (&folders);
}

GPtrArray *
gbp_test_path_get_tests (GbpTestPath *self)
{
  g_return_val_if_fail (GBP_IS_TEST_PATH (self), NULL);

  return ide_test_manager_get_tests (self->test_manager, self->path);
}

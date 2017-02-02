/* ide-file-monitor.c
 *
 * Copyright (C) 2017 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "ide-file-monitor"

#include "ide-context.h"
#include "ide-debug.h"

#include "files/ide-file.h"

struct _IdeFileMonitor
{
  IdeObject          parent_instance;

  IdeFile           *file;
};

enum {
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

G_DEFINE_TYPE (IdeFileMonitor, ide_file_monitor, IDE_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];

/**
 * ide_file_monitor_get_file:
 *
 * Retrieves the underlying #GFile represented by @self.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_file_monitor_get_file (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  return self->file;
}

static void
ide_file_monitor_finalize (GObject *object)
{
  IdeFile *self = (IdeFile *)object;

  IDE_ENTRY;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_file_monitor_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_file_monitor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeFile *self = (IdeFile *)object;

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_file_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_monitor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeFile *self = (IdeFile *)object;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_monitor_class_init (IdeFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_file_monitor_finalize;
  object_class->get_property = ide_file_monitor_get_property;
  object_class->set_property = ide_file_monitor_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The path to the underlying file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_file_monitor_init (IdeFile *file)
{
  EGG_COUNTER_INC (instances);
}

/**
 * ide_file_monitor_new:
 * @context: (allow-none): An #IdeContext or %NULL.
 * @file: a #GFile.
 *
 * Creates a new file.
 *
 * Returns: (transfer full): An #IdeFile.
 */
IdeFile *
ide_file_monitor_new (IdeContext *context,
              GFile      *file)
{
  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (IDE_TYPE_FILE,
                       "context", context,
                       "file", file,
                       NULL);
}

IdeFile *
ide_file_monitor_new_for_path (IdeContext  *context,
                       const gchar *path)
{
  g_autoptr(GFile) file = NULL;
  IdeFile *ret;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  file = g_file_new_for_path (path);
  ret = g_object_new (IDE_TYPE_FILE,
                      "context", context,
                      "file", file,
                      NULL);

  return ret;
}

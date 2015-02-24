/* ide-buffer.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-buffer"

#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-context.h"
#include "ide-file.h"

struct _IdeBufferClass
{
  GtkSourceBufferClass parent_class;
};

struct _IdeBuffer
{
  GtkSourceBuffer  parent_instance;

  IdeContext      *context;
  IdeFile         *file;
};

G_DEFINE_TYPE (IdeBuffer, ide_buffer, GTK_SOURCE_TYPE_BUFFER)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_FILE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_buffer_get_context:
 *
 * Gets the #IdeBuffer:context property. This is the #IdeContext that owns the buffer.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
ide_buffer_get_context (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->context;
}

static void
ide_buffer_set_context (IdeBuffer  *self,
                        IdeContext *context)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (self->context == NULL);

  ide_set_weak_pointer (&self->context, context);
}

/**
 * ide_buffer_get_file:
 *
 * Gets the underlying file behind the buffer.
 *
 * Returns: (transfer none): An #IdeFile.
 */
IdeFile *
ide_buffer_get_file (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->file;
}

/**
 * ide_buffer_set_file:
 *
 * Sets the underlying file to use when saving and loading @self to and and from storage.
 */
void
ide_buffer_set_file (IdeBuffer *self,
                     IdeFile   *file)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
}

static void
ide_buffer_finalize (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_buffer_parent_class)->finalize (object);
}

static void
ide_buffer_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_buffer_get_context (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_buffer_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_buffer_set_context (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      ide_buffer_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_class_init (IdeBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_buffer_finalize;
  object_class->get_property = ide_buffer_get_property;
  object_class->set_property = ide_buffer_set_property;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The IdeContext for the buffer."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file represented by the buffer."),
                         IDE_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE, gParamSpecs [PROP_FILE]);
}

static void
ide_buffer_init (IdeBuffer *self)
{
}

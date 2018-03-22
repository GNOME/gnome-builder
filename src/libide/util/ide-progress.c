/* ide-progress.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-progress"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-object.h"
#include "util/ide-progress.h"

struct _IdeProgress
{
  GObject  parent_instance;

  GMutex   mutex;
  gchar   *message;
  gdouble  fraction;
  guint    completed : 1;
};

G_DEFINE_TYPE (IdeProgress, ide_progress, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COMPLETED,
  PROP_FRACTION,
  PROP_MESSAGE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

gboolean
ide_progress_get_completed (IdeProgress *self)
{
  g_return_val_if_fail (IDE_IS_PROGRESS (self), FALSE);

  return self->completed;
}

void
ide_progress_set_completed (IdeProgress *self,
                            gboolean     completed)
{
  g_return_if_fail (IDE_IS_PROGRESS (self));

  g_mutex_lock (&self->mutex);
  if (self->completed != completed)
    self->completed = completed;
  g_mutex_unlock (&self->mutex);

  ide_object_notify_in_main (G_OBJECT (self), properties [PROP_COMPLETED]);
}

gdouble
ide_progress_get_fraction (IdeProgress *self)
{
  gdouble ret;

  g_return_val_if_fail (IDE_IS_PROGRESS (self), 0.0);

  g_mutex_lock (&self->mutex);
  ret = self->fraction;
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
ide_progress_set_fraction (IdeProgress *self,
                           gdouble      fraction)
{
  gboolean do_completed = FALSE;
  gboolean do_notify = FALSE;

  g_return_if_fail (IDE_IS_PROGRESS (self));

  fraction = CLAMP (fraction, 0.0, 1.0);

  g_mutex_lock (&self->mutex);
  if (self->fraction != fraction)
    {
      self->fraction = fraction;
      if (fraction == 1.0)
        do_completed = TRUE;
      do_notify = TRUE;
    }
  g_mutex_unlock (&self->mutex);

  if (do_completed)
    ide_progress_set_completed (self, TRUE);

  if (do_notify)
    ide_object_notify_in_main (G_OBJECT (self), properties [PROP_FRACTION]);
}

gchar *
ide_progress_get_message (IdeProgress *self)
{
  gchar *ret;

  g_return_val_if_fail (IDE_IS_PROGRESS (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = g_strdup (self->message);
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
ide_progress_set_message (IdeProgress *self,
                          const gchar *message)
{
  g_return_if_fail (IDE_IS_PROGRESS (self));

  g_mutex_lock (&self->mutex);
  if (g_strcmp0 (self->message, message) != 0)
    {
      g_free (self->message);
      self->message = g_strdup (message);
      ide_object_notify_in_main (G_OBJECT (self), properties [PROP_MESSAGE]);
    }
  g_mutex_unlock (&self->mutex);
}

void
ide_progress_flatpak_progress_callback (const char *status,
                                        guint       progress,
                                        gboolean    estimating,
                                        gpointer    user_data)
{
  IdeProgress *self = user_data;

  g_return_if_fail (IDE_IS_PROGRESS (self));

  ide_progress_set_message (self, status);
  ide_progress_set_fraction (self, (gdouble)progress / 100.0);
}

/**
 * ide_progress_file_progress_callback:
 *
 * This function is a #GFileProgressCallback helper that will update the
 * #IdeProgress:fraction property. @user_data must be an #IdeProgress.
 *
 * Remember to make sure to unref the #IdeProgress instance with
 * g_object_unref() during the #GDestroyNotify.
 */
void
ide_progress_file_progress_callback (goffset  current_num_bytes,
                                     goffset  total_num_bytes,
                                     gpointer user_data)
{
  IdeProgress *self = user_data;
  gdouble fraction = 0.0;

  g_return_if_fail (IDE_IS_PROGRESS (self));

  if (total_num_bytes)
    fraction = (gdouble)current_num_bytes / (gdouble)total_num_bytes;

  ide_progress_set_fraction (self, fraction);
}

static void
ide_progress_finalize (GObject *object)
{
  IdeProgress *self = (IdeProgress *)object;

  g_clear_pointer (&self->message, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (ide_progress_parent_class)->finalize (object);
}

static void
ide_progress_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeProgress *self = IDE_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_progress_get_completed (self));
      break;

    case PROP_FRACTION:
      g_value_set_double (value, ide_progress_get_fraction (self));
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, ide_progress_get_message (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_progress_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeProgress *self = IDE_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      ide_progress_set_fraction (self, g_value_get_double (value));
      break;

    case PROP_MESSAGE:
      ide_progress_set_message (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_progress_class_init (IdeProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_progress_finalize;
  object_class->get_property = ide_progress_get_property;
  object_class->set_property = ide_progress_set_property;

  properties [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          "Completed",
                          "If the progress has completed.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FRACTION] =
    g_param_spec_double ("fraction",
                         "Fraction",
                         "The fraction of the progress.",
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "A short message for the progress.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_progress_init (IdeProgress *self)
{
  g_mutex_init (&self->mutex);
}

IdeProgress *
ide_progress_new (void)
{
  return g_object_new (IDE_TYPE_PROGRESS, NULL);
}

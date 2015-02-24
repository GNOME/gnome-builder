/* ide-progress.c
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

#define G_LOG_DOMAIN "ide-progress"

#include <glib/gi18n.h>

#include "ide-progress.h"

struct _IdeProgress
{
  IdeObject  parent_instance;

  gchar     *message;
  gdouble    fraction;
  guint      completed : 1;
};

G_DEFINE_TYPE (IdeProgress, ide_progress, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COMPLETED,
  PROP_FRACTION,
  PROP_MESSAGE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

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

  if (self->completed != completed)
    {
      self->completed = completed;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_COMPLETED]);
    }
}

gdouble
ide_progress_get_fraction (IdeProgress *self)
{
  g_return_val_if_fail (IDE_IS_PROGRESS (self), 0.0);

  return self->fraction;
}

void
ide_progress_set_fraction (IdeProgress *self,
                           gdouble      fraction)
{
  g_return_if_fail (IDE_IS_PROGRESS (self));
  g_return_if_fail (fraction >= 0.0);
  g_return_if_fail (fraction <= 1.0);

  if (self->fraction != fraction)
    {
      self->fraction = fraction;
      if (fraction == 1.0)
        ide_progress_set_completed (self, TRUE);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FRACTION]);
    }
}

const gchar *
ide_progress_get_message (IdeProgress *self)
{
  g_return_val_if_fail (IDE_IS_PROGRESS (self), NULL);

  return self->message;
}

void
ide_progress_set_message (IdeProgress *self,
                          const gchar *message)
{
  g_return_if_fail (IDE_IS_PROGRESS (self));

  if (self->message != message)
    {
      g_free (self->message);
      self->message = g_strdup (message);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_MESSAGE]);
    }
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
      g_value_set_string (value, ide_progress_get_message (self));
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

  gParamSpecs [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          _("Completed"),
                          _("If the progress has completed."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COMPLETED,
                                   gParamSpecs [PROP_COMPLETED]);

  gParamSpecs [PROP_FRACTION] =
    g_param_spec_double ("fraction",
                         _("Fraction"),
                         _("The fraction of the progress."),
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FRACTION,
                                   gParamSpecs [PROP_FRACTION]);

  gParamSpecs [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         _("Message"),
                         _("A short message for the progress."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MESSAGE,
                                   gParamSpecs [PROP_MESSAGE]);
}

static void
ide_progress_init (IdeProgress *self)
{
}

/*
 * manuals-job.c
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

#include <libdex.h>

#include "manuals-job.h"

struct _ManualsJob
{
  GObject parent_instance;
  GMutex mutex;
  char *title;
  char *subtitle;
  double fraction;
  guint has_completed : 1;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_FRACTION,
  N_PROPS
};

enum {
  COMPLETED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (ManualsJob, manuals_job, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

typedef struct _NotifyInMain
{
  ManualsJob *self;
  GParamSpec *pspec;
} NotifyInMain;

static void
notify_in_main_cb (gpointer user_data)
{
  NotifyInMain *state = user_data;

  g_assert (state != NULL);
  g_assert (MANUALS_IS_JOB (state->self));
  g_assert (state->pspec != NULL);

  g_object_notify_by_pspec (G_OBJECT (state->self), state->pspec);

  g_clear_object (&state->self);
  g_clear_pointer (&state->pspec, g_param_spec_unref);
  g_free (state);
}

static void
notify_in_main (ManualsJob *self,
                GParamSpec *pspec)
{
  NotifyInMain *state;

  g_assert (MANUALS_IS_JOB (self));
  g_assert (pspec != NULL);

  state = g_new0 (NotifyInMain, 1);
  state->self = g_object_ref (self);
  state->pspec = g_param_spec_ref (pspec);

  dex_scheduler_push (dex_scheduler_get_default (),
                      notify_in_main_cb,
                      state);
}

static void
manuals_job_finalize (GObject *object)
{
  ManualsJob *self = (ManualsJob *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (manuals_job_parent_class)->finalize (object);
}

static void
manuals_job_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ManualsJob *self = MANUALS_JOB (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      g_value_set_double (value, manuals_job_get_fraction (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, manuals_job_dup_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, manuals_job_dup_subtitle (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_job_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ManualsJob *self = MANUALS_JOB (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      manuals_job_set_fraction (self, g_value_get_double (value));
      break;

    case PROP_TITLE:
      manuals_job_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      manuals_job_set_subtitle (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_job_class_init (ManualsJobClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_job_finalize;
  object_class->get_property = manuals_job_get_property;
  object_class->set_property = manuals_job_set_property;

  signals[COMPLETED] =
    g_signal_new ("completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FRACTION] =
    g_param_spec_double ("fraction", NULL, NULL,
                         0, 1, 0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_job_init (ManualsJob *self)
{
  g_mutex_init (&self->mutex);
}

ManualsJob *
manuals_job_new (void)
{
  return g_object_new (MANUALS_TYPE_JOB, NULL);
}

char *
manuals_job_dup_title (ManualsJob *self)
{
  char *ret;

  g_return_val_if_fail (MANUALS_IS_JOB (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = g_strdup (self->title);
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
manuals_job_set_title (ManualsJob *self,
                       const char *title)
{
  g_return_if_fail (MANUALS_IS_JOB (self));

  g_mutex_lock (&self->mutex);
  if (g_set_str (&self->title, title))
    notify_in_main (self, properties[PROP_TITLE]);
  g_mutex_unlock (&self->mutex);
}

char *
manuals_job_dup_subtitle (ManualsJob *self)
{
  char *ret;

  g_return_val_if_fail (MANUALS_IS_JOB (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = g_strdup (self->subtitle);
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
manuals_job_set_subtitle (ManualsJob *self,
                          const char *subtitle)
{
  g_return_if_fail (MANUALS_IS_JOB (self));

  g_mutex_lock (&self->mutex);
  if (g_set_str (&self->subtitle, subtitle))
    notify_in_main (self, properties[PROP_SUBTITLE]);
  g_mutex_unlock (&self->mutex);
}

double
manuals_job_get_fraction (ManualsJob *self)
{
  g_return_val_if_fail (MANUALS_IS_JOB (self), 0);

  return self->fraction;
}

void
manuals_job_set_fraction (ManualsJob *self,
                          double      fraction)
{
  g_return_if_fail (MANUALS_IS_JOB (self));

  g_mutex_lock (&self->mutex);
  if (fraction != self->fraction)
    {
      self->fraction = fraction;
      notify_in_main (self, properties[PROP_FRACTION]);
    }
  g_mutex_unlock (&self->mutex);
}

void
manuals_job_complete (ManualsJob *self)
{
  gboolean can_emit;

  g_return_if_fail (MANUALS_IS_JOB (self));

  g_mutex_lock (&self->mutex);
  can_emit = self->has_completed == FALSE;
  self->has_completed = TRUE;
  self->fraction = 1;
  g_mutex_unlock (&self->mutex);

  if (can_emit)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FRACTION]);
      g_signal_emit (self, signals[COMPLETED], 0);
    }
}

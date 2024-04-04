/*
 * manuals-job.h
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define MANUALS_TYPE_JOB (manuals_job_get_type())

G_DECLARE_FINAL_TYPE (ManualsJob, manuals_job, MANUALS, JOB, GObject)

ManualsJob *manuals_job_new          (void);
char       *manuals_job_dup_title    (ManualsJob *self);
void        manuals_job_set_title    (ManualsJob *self,
                                      const char *title);
char       *manuals_job_dup_subtitle (ManualsJob *self);
void        manuals_job_set_subtitle (ManualsJob *self,
                                      const char *subtitle);
double      manuals_job_get_fraction (ManualsJob *self);
void        manuals_job_set_fraction (ManualsJob *self,
                                      double      fraction);
void        manuals_job_complete     (ManualsJob *self);

typedef struct _ManualsJob ManualsJobMonitor;

static inline void
_manuals_job_monitor_cleanup_func (ManualsJobMonitor *monitor)
{
  manuals_job_complete (monitor);
  g_object_unref (monitor);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ManualsJobMonitor, _manuals_job_monitor_cleanup_func)

G_END_DECLS

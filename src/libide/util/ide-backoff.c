/* ide-backoff.c
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

#include "config.h"

#define G_LOG_DOMAIN "ide-backoff"

#include "util/ide-backoff.h"

/**
 * ide_backoff_init:
 * @self: (out): an #IdeBackoff
 * @min_delay: minimum delay to apply for exponential backoff
 * @max_delay: maximum delay to apply for exponential backoff
 *
 * Initializes an @IdeBackoff struct.
 *
 * This is useful for implementing exponential backoff.
 *
 * Since: 3.32
 */
void
ide_backoff_init (IdeBackoff *self,
                  guint       min_delay,
                  guint       max_delay)
{
  g_return_if_fail (self != NULL);

  if (max_delay < 2)
    max_delay = G_MAXUINT;

  self->min_delay = MAX (1, min_delay);
  self->max_delay = MAX (min_delay, max_delay);
  self->cur_delay = self->min_delay;
  self->n_failures = 0;

  g_return_if_fail (self->min_delay > 0);
  g_return_if_fail (self->cur_delay > 0);
  g_return_if_fail (self->max_delay >= self->min_delay);
}

/**
 * ide_backoff_failed:
 * @self: an #IdeBackoff
 * @next_delay: (optional) (out): location for the next delay
 *
 * Marks the backoff as failed, so that the next delay timeout will be used.
 * You can access the value using @next_delay to determine how long to sleep
 * before retrying the operation.
 *
 * Since: 3.32
 */
void
ide_backoff_failed (IdeBackoff *self,
                    guint      *next_delay)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->min_delay > 0);
  g_return_if_fail (self->cur_delay > 0);
  g_return_if_fail (self->max_delay >= self->min_delay);

  self->n_failures++;

  /* Special case overflow for correctness */
  if (self->cur_delay > (self->max_delay / 2))
    self->cur_delay = self->max_delay;
  else
    self->cur_delay *= 2;

  if (next_delay != NULL)
    {
      guint adjustment;

      /*
       * Generate small random adjustment to the delay time so that we avoid
       * coordinating components from racing together to failures.
       *
       * We don't set this on cur_delay, because we want things to be more
       * testable and to not drift based on the adjustment.
       */
      adjustment = g_random_int_range (0, MIN (self->min_delay,
                                               MIN (G_MAXINT, self->max_delay) / 4));

      *next_delay = self->cur_delay;

      if (*next_delay == self->max_delay)
        *next_delay -= adjustment;
      else
        *next_delay += adjustment;
    }
}

/**
 * ide_backoff_succeeded:
 * @self: an #IdeBackoff
 *
 * Mark the backoff as succeeded and reset the counters.
 *
 * Since: 3.32
 */
void
ide_backoff_succeeded (IdeBackoff *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->min_delay > 0);
  g_return_if_fail (self->cur_delay > 0);
  g_return_if_fail (self->max_delay >= self->min_delay);

  self->n_failures = 0;
  self->cur_delay = self->min_delay;

  g_return_if_fail (self->min_delay > 0);
  g_return_if_fail (self->cur_delay > 0);
  g_return_if_fail (self->max_delay >= self->min_delay);
}

/* gbp-maven-test.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-maven-test"

#include "config.h"

#include "gbp-maven-test.h"

struct _GbpMavenTest
{
  IdeTest parent_instance;
  char *suite_name;
};

G_DEFINE_FINAL_TYPE (GbpMavenTest, gbp_maven_test, IDE_TYPE_TEST)

static void
gbp_maven_test_finalize (GObject *object)
{
  GbpMavenTest *self = (GbpMavenTest *)object;

  g_clear_pointer (&self->suite_name, g_free);

  G_OBJECT_CLASS (gbp_maven_test_parent_class)->finalize (object);
}

static void
gbp_maven_test_class_init (GbpMavenTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_maven_test_finalize;
}

static void
gbp_maven_test_init (GbpMavenTest *self)
{
}

GbpMavenTest *
gbp_maven_test_new (const char *suite_name)
{
  GbpMavenTest *self = g_object_new (GBP_TYPE_MAVEN_TEST, NULL);
  self->suite_name = g_strdup (suite_name);
  return self;
}

const char *
gbp_maven_test_get_suite_name (GbpMavenTest *self)
{
  g_return_val_if_fail (GBP_IS_MAVEN_TEST (self), NULL);

  return self->suite_name;
}

/* gbp-meson-test.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-test"

#include "gbp-meson-test.h"

struct _GbpMesonTest
{
  IdeTest     parent_instance;
  gchar     **environ;
  gchar     **command;
  GFile      *workdir;
  guint       timeout;
};

enum {
  PROP_0,
  PROP_COMMAND,
  PROP_ENVIRON,
  PROP_TIMEOUT,
  PROP_WORKDIR,
  N_PROPS
};

G_DEFINE_TYPE (GbpMesonTest, gbp_meson_test, IDE_TYPE_TEST)

static GParamSpec *properties [N_PROPS];

static void
gbp_meson_test_finalize (GObject *object)
{
  GbpMesonTest *self = (GbpMesonTest *)object;

  g_clear_pointer (&self->command, g_strfreev);
  g_clear_pointer (&self->environ, g_strfreev);
  g_clear_object (&self->workdir);

  G_OBJECT_CLASS (gbp_meson_test_parent_class)->finalize (object);
}

static void
gbp_meson_test_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpMesonTest *self = GBP_MESON_TEST (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_boxed (value, self->command);
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, self->environ);
      break;

    case PROP_TIMEOUT:
      g_value_set_uint (value, self->timeout);
      break;

    case PROP_WORKDIR:
      g_value_set_object (value, self->workdir);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_test_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpMesonTest *self = GBP_MESON_TEST (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      self->command = g_value_dup_boxed (value);
      break;

    case PROP_ENVIRON:
      self->environ = g_value_dup_boxed (value);
      break;

    case PROP_TIMEOUT:
      self->timeout = g_value_get_uint (value);
      break;

    case PROP_WORKDIR:
      self->workdir = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_meson_test_class_init (GbpMesonTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_meson_test_finalize;
  object_class->get_property = gbp_meson_test_get_property;
  object_class->set_property = gbp_meson_test_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_boxed ("command",
                        "Command",
                        "The command to execute for the test",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environment",
                        "The environment for the test",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TIMEOUT] =
    g_param_spec_uint ("timeout",
                       "Timeout",
                       "The timeout in seconds, or 0 for none",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_WORKDIR] =
    g_param_spec_object ("workdir",
                         "Workdir",
                         "The working directory to run the test",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_meson_test_init (GbpMesonTest *self)
{
}

const gchar * const *
gbp_meson_test_get_command (GbpMesonTest *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TEST (self), NULL);

  return (const gchar * const *)self->command;
}

GFile *
gbp_meson_test_get_workdir (GbpMesonTest *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TEST (self), NULL);

  return self->workdir;
}

guint
gbp_meson_test_get_timeout (GbpMesonTest *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TEST (self), 0);

  return self->timeout;
}

const gchar * const *
gbp_meson_test_get_environ (GbpMesonTest *self)
{
  g_return_val_if_fail (GBP_IS_MESON_TEST (self), NULL);

  return (const gchar * const *)self->environ;
}

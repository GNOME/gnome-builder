/* gbp-make-template.c
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

#define G_LOG_DOMAIN "gbp-make-template"

#include "config.h"

#include "gbp-make-template.h"

struct _GbpMakeTemplate
{
  IdeProjectTemplate parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpMakeTemplate, gbp_make_template, IDE_TYPE_PROJECT_TEMPLATE)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_make_template_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_make_template_parent_class)->finalize (object);
}

static void
gbp_make_template_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_make_template_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_make_template_class_init (GbpMakeTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_make_template_finalize;
  object_class->get_property = gbp_make_template_get_property;
  object_class->set_property = gbp_make_template_set_property;
}

static void
gbp_make_template_init (GbpMakeTemplate *self)
{
}

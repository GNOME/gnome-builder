/* ide-docs-pane.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-docs-pane"

#include "config.h"

#include <dazzle.h>

#include "ide-docs-library.h"
#include "ide-docs-pane.h"

struct _IdeDocsPane
{
  IdePane         parent_instance;

  IdeDocsLibrary *library;

  /* Template widgets */
  DzlStackList   *stack_list;
};

enum {
  PROP_0,
  PROP_LIBRARY,
  N_PROPS
};

G_DEFINE_TYPE (IdeDocsPane, ide_docs_pane, IDE_TYPE_PANE)

static GParamSpec *properties [N_PROPS];

static void
ide_docs_pane_finalize (GObject *object)
{
  IdeDocsPane *self = (IdeDocsPane *)object;

  g_clear_object (&self->library);

  G_OBJECT_CLASS (ide_docs_pane_parent_class)->finalize (object);
}

static void
ide_docs_pane_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeDocsPane *self = IDE_DOCS_PANE (object);

  switch (prop_id)
    {
    case PROP_LIBRARY:
      g_value_set_object (value, self->library);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_pane_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeDocsPane *self = IDE_DOCS_PANE (object);

  switch (prop_id)
    {
    case PROP_LIBRARY:
      self->library = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_pane_class_init (IdeDocsPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_docs_pane_finalize;
  object_class->get_property = ide_docs_pane_get_property;
  object_class->set_property = ide_docs_pane_set_property;

  properties [PROP_LIBRARY] =
    g_param_spec_object ("library",
                         "Library",
                         "The library for the documentation pane",
                         IDE_TYPE_DOCS_LIBRARY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsPane, stack_list);

  g_type_ensure (DZL_TYPE_STACK_LIST);
}

static void
ide_docs_pane_init (IdeDocsPane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * ide_docs_pane_get_library:
 * @self: an #IdeDocsPane
 *
 * Get the library for the pane.
 *
 * Returns: (transfer none): an #IdeDocsLibrary
 *
 * Since: 3.34
 */
IdeDocsLibrary *
ide_docs_pane_get_library (IdeDocsPane *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_PANE (self), NULL);

  return self->library;
}

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

#include <glib/gi18n.h>
#include <dazzle.h>

#include "ide-docs-library.h"
#include "ide-docs-pane.h"
#include "ide-docs-pane-row.h"

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
      ide_docs_pane_set_library (self, g_value_get_object (value));
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
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsPane, stack_list);

  g_type_ensure (DZL_TYPE_STACK_LIST);
}

static GtkWidget *
create_pane_row_cb (gpointer item_,
                    gpointer user_data)
{
  IdeDocsItem *item = item_;

  g_assert (IDE_IS_DOCS_ITEM (item));

  return g_object_new (IDE_TYPE_DOCS_PANE_ROW,
                       "item", item,
                       "visible", TRUE,
                       NULL);
}

static void
ide_docs_pane_activate_populate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  IdeDocsPane *self;
  IdeDocsItem *item;

  g_assert (IDE_IS_DOCS_LIBRARY (object));
  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = ide_task_get_source_object (task);
  item = ide_task_get_task_data (task);

  dzl_stack_list_push (self->stack_list,
                       create_pane_row_cb (item, self),
                       G_LIST_MODEL (item),
                       create_pane_row_cb,
                       self, NULL);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_docs_pane_row_activated_cb (IdeDocsPane    *self,
                                IdeDocsPaneRow *row,
                                DzlStackList   *stack_list)
{
  g_autoptr(IdeTask) task = NULL;
  IdeDocsLibrary *library;
  IdeDocsItem *item;
  IdeContext *context;

  g_assert (IDE_IS_DOCS_PANE (self));
  g_assert (IDE_IS_DOCS_PANE_ROW (row));
  g_assert (DZL_IS_STACK_LIST (stack_list));

  item = ide_docs_pane_row_get_item (row);
  context = ide_widget_get_context (GTK_WIDGET (self));
  library = ide_docs_library_from_context (context);

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, ide_docs_pane_row_activated_cb);
  ide_task_set_task_data (task, g_object_ref (item), g_object_unref);

  ide_docs_library_populate_async (library,
                                   item,
                                   NULL,
                                   ide_docs_pane_activate_populate_cb,
                                   g_steal_pointer (&task));
}

static void
ide_docs_pane_init (IdeDocsPane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->stack_list,
                           "row-activated",
                           G_CALLBACK (ide_docs_pane_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_docs_pane_populate_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeDocsLibrary *library = (IdeDocsLibrary *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeDocsPane *self;
  IdeDocsItem *item;

  g_assert (G_IS_OBJECT (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  item = ide_task_get_task_data (task);

  g_assert (IDE_IS_DOCS_PANE (self));
  g_assert (IDE_IS_DOCS_ITEM (item));

  if (!ide_docs_library_populate_finish (library, result, &error))
    {
      g_warning ("Failed to populate documentation: %s",
                 error->message);
      return;
    }

  dzl_stack_list_clear (self->stack_list);
  dzl_stack_list_push (self->stack_list,
                       create_pane_row_cb (item, self),
                       G_LIST_MODEL (item),
                       create_pane_row_cb,
                       self, NULL);

  ide_task_return_boolean (task, TRUE);
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

void
ide_docs_pane_set_library (IdeDocsPane    *self,
                           IdeDocsLibrary *library)
{
  g_return_if_fail (IDE_IS_DOCS_PANE (self));
  g_return_if_fail (!library || IDE_IS_DOCS_LIBRARY (library));

  if (g_set_object (&self->library, library))
    {
      g_autoptr(IdeDocsItem) root = NULL;
      g_autoptr(IdeTask) task = NULL;

      if (library == NULL)
        {
          dzl_stack_list_clear (self->stack_list);
          return;
        }

      root = ide_docs_item_new ();
      ide_docs_item_set_title (root, _("Library"));
      ide_docs_item_set_kind (root, IDE_DOCS_ITEM_KIND_COLLECTION);

      task = ide_task_new (self, NULL, NULL, NULL);
      ide_task_set_task_data (task, g_object_ref (root), g_object_unref);

      ide_docs_library_populate_async (library,
                                       root,
                                       NULL,
                                       ide_docs_pane_populate_cb,
                                       g_steal_pointer (&task));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LIBRARY]);
    }
}

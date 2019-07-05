/* ide-docs-search-section.c
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

#define G_LOG_DOMAIN "ide-docs-search-section"

#include "config.h"

#include <dazzle.h>

#include "ide-docs-search-group.h"
#include "ide-docs-search-section.h"

struct _IdeDocsSearchSection
{
  GtkBin          parent_instance;

  DzlPriorityBox *groups;

  gchar          *title;

  gint            priority;
};

G_DEFINE_TYPE (IdeDocsSearchSection, ide_docs_search_section, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_PRIORITY,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_docs_search_section_add (GtkContainer *container,
                             GtkWidget    *child)
{
  IdeDocsSearchSection *self = (IdeDocsSearchSection *)container;

  g_assert (IDE_IS_DOCS_SEARCH_SECTION (self));
  g_assert (GTK_IS_WIDGET (child));

  if (IDE_IS_DOCS_SEARCH_GROUP (child))
    {
      gint priority = ide_docs_search_group_get_priority (IDE_DOCS_SEARCH_GROUP (child));
      gtk_container_add_with_properties (GTK_CONTAINER (self->groups), child,
                                         "priority", priority,
                                         NULL);
      return;
    }

  GTK_CONTAINER_CLASS (ide_docs_search_section_parent_class)->add (container, child);
}

static void
ide_docs_search_section_finalize (GObject *object)
{
  IdeDocsSearchSection *self = (IdeDocsSearchSection *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_docs_search_section_parent_class)->finalize (object);
}

static void
ide_docs_search_section_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeDocsSearchSection *self = IDE_DOCS_SEARCH_SECTION (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, ide_docs_search_section_get_priority (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_section_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeDocsSearchSection *self = IDE_DOCS_SEARCH_SECTION (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      ide_docs_search_section_set_priority (self, g_value_get_int (value));
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_section_class_init (IdeDocsSearchSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_docs_search_section_finalize;
  object_class->get_property = ide_docs_search_section_get_property;
  object_class->set_property = ide_docs_search_section_set_property;

  container_class->add = ide_docs_search_section_add;

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "THe priority of the section",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the section",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "IdeDocsSearchSection");
}

static void
ide_docs_search_section_init (IdeDocsSearchSection *self)
{
  self->groups = g_object_new (DZL_TYPE_PRIORITY_BOX,
                               "orientation", GTK_ORIENTATION_VERTICAL,
                               "spacing", 14,
                               "visible", TRUE,
                               NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->groups));
}

GtkWidget *
ide_docs_search_section_new (const gchar *title)
{
  return g_object_new (IDE_TYPE_DOCS_SEARCH_SECTION,
                       "title", title,
                       NULL);
}

const gchar *
ide_docs_search_section_get_title (IdeDocsSearchSection *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self), NULL);

  return self->title;
}

gint
ide_docs_search_section_get_priority (IdeDocsSearchSection *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self), 0);

  return self->priority;
}

void
ide_docs_search_section_set_priority (IdeDocsSearchSection *self,
                                      gint                  priority)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self));

  if (priority != self->priority)
    {
      self->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

void
ide_docs_search_section_add_groups (IdeDocsSearchSection *self,
                                    IdeDocsItem          *parent)
{
  gboolean show_more_items = FALSE;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (parent));

  /* If there is a single group within the section, we want to show more
   * items than we otherwise would.
   */
  if (ide_docs_item_get_n_children (parent) == 1)
    show_more_items = TRUE;

  for (const GList *iter = ide_docs_item_get_children (parent);
       iter != NULL;
       iter = iter->next)
    {
      IdeDocsItem *child = iter->data;
      IdeDocsSearchGroup *group;
      const gchar *title;
      gint priority;

      g_assert (IDE_IS_DOCS_ITEM (child));

      title = ide_docs_item_get_title (child);
      priority = ide_docs_item_get_priority (child);
      group = g_object_new (IDE_TYPE_DOCS_SEARCH_GROUP,
                            "title", title,
                            "priority", priority,
                            "visible", TRUE,
                            NULL);
      if (show_more_items)
        ide_docs_search_group_set_max_items (group, 25);
      ide_docs_search_group_add_items (group, child);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (group));
    }
}

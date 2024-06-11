/*
 * gbp-git-staged-item.c
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

#include <glib/gi18n.h>

#include "gbp-git-staged-item.h"
#include "gbp-git-staged-row.h"

struct _GbpGitStagedItem
{
  GbpGitCommitItem parent_instance;
  GFile *file;
  char *title;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_ICON,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitStagedItem, gbp_git_staged_item, GBP_TYPE_GIT_COMMIT_ITEM)

static GParamSpec *properties[N_PROPS];

static const char *
gbp_git_staged_item_get_section_title (GbpGitCommitItem *item)
{
  return _("Staged Files");
}

static void
gbp_git_staged_item_bind (GbpGitCommitItem *item,
                          GtkListItem      *list_item)
{
  GbpGitStagedItem *self = (GbpGitStagedItem *)item;
  GtkWidget *child;

  g_assert (GBP_IS_GIT_STAGED_ITEM (self));
  g_assert (GTK_IS_LIST_ITEM (list_item));

  child = gtk_list_item_get_child (list_item);

  if (!GBP_IS_GIT_STAGED_ROW (child))
    {
      child = gbp_git_staged_row_new ();
      gtk_list_item_set_child (list_item, child);
    }

  gbp_git_staged_row_set_item (GBP_GIT_STAGED_ROW (child), self);
}

static void
gbp_git_staged_item_dispose (GObject *object)
{
  GbpGitStagedItem *self = (GbpGitStagedItem *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gbp_git_staged_item_parent_class)->dispose (object);
}

static void
gbp_git_staged_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpGitStagedItem *self = GBP_GIT_STAGED_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gbp_git_staged_item_get_file (self));
      break;

    case PROP_ICON:
      g_value_take_object (value, gbp_git_staged_item_dup_icon (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gbp_git_staged_item_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpGitStagedItem *self = GBP_GIT_STAGED_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_item_class_init (GbpGitStagedItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbpGitCommitItemClass *item_class = GBP_GIT_COMMIT_ITEM_CLASS (klass);

  object_class->dispose = gbp_git_staged_item_dispose;
  object_class->get_property = gbp_git_staged_item_get_property;
  object_class->set_property = gbp_git_staged_item_set_property;

  item_class->get_section_title = gbp_git_staged_item_get_section_title;
  item_class->bind = gbp_git_staged_item_bind;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_staged_item_init (GbpGitStagedItem *self)
{
}

GFile *
gbp_git_staged_item_get_file (GbpGitStagedItem *self)
{
  g_return_val_if_fail (GBP_IS_GIT_STAGED_ITEM (self), NULL);

  return self->file;
}

const char *
gbp_git_staged_item_get_title (GbpGitStagedItem *self)
{
  g_return_val_if_fail (GBP_IS_GIT_STAGED_ITEM (self), NULL);

  return self->title;
}

GIcon *
gbp_git_staged_item_dup_icon (GbpGitStagedItem *self)
{
  g_autofree char *content_type = NULL;
  const char *filename = NULL;
  gboolean uncertan;

  g_return_val_if_fail (GBP_IS_GIT_STAGED_ITEM (self), NULL);
  g_return_val_if_fail (G_IS_FILE (self->file), NULL);
  g_return_val_if_fail (g_file_is_native (self->file), NULL);

  filename = g_file_peek_path (self->file);

  if ((content_type = g_content_type_guess (filename, NULL, 0, &uncertan)))
    return g_content_type_get_symbolic_icon (content_type);

  return NULL;
}

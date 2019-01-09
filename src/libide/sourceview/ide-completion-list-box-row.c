/* ide-completion-list-box-row.c
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

#define G_LOG_DOMAIN "ide-completion-list-box-row"

#include "config.h"

#include "ide-completion-list-box-row.h"
#include "ide-completion-private.h"

struct _IdeCompletionListBoxRow
{
  GtkListBoxRow          parent_instance;

  IdeCompletionProposal *proposal;

  GtkBox                *box;
  GtkImage              *image;
  GtkLabel              *left;
  GtkLabel              *center;
  GtkLabel              *right;
};

enum {
  PROP_0,
  PROP_PROPOSAL,
  N_PROPS
};

G_DEFINE_TYPE (IdeCompletionListBoxRow, ide_completion_list_box_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];

static void
ide_completion_list_box_row_finalize (GObject *object)
{
  IdeCompletionListBoxRow *self = (IdeCompletionListBoxRow *)object;

  g_clear_object (&self->proposal);

  G_OBJECT_CLASS (ide_completion_list_box_row_parent_class)->finalize (object);
}

static void
ide_completion_list_box_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeCompletionListBoxRow *self = IDE_COMPLETION_LIST_BOX_ROW (object);

  switch (prop_id)
    {
    case PROP_PROPOSAL:
      g_value_set_object (value, ide_completion_list_box_row_get_proposal (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_list_box_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeCompletionListBoxRow *self = IDE_COMPLETION_LIST_BOX_ROW (object);

  switch (prop_id)
    {
    case PROP_PROPOSAL:
      ide_completion_list_box_row_set_proposal (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_list_box_row_class_init (IdeCompletionListBoxRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_completion_list_box_row_finalize;
  object_class->get_property = ide_completion_list_box_row_get_property;
  object_class->set_property = ide_completion_list_box_row_set_property;

  /**
   * IdeCompletionListBoxRow:proposal:
   *
   * The proposal to display in the list box row.
   *
   * Since: 3.32
   */
  properties [PROP_PROPOSAL] =
    g_param_spec_object ("proposal",
                         "Proposal",
                         "The proposal to be displayed",
                         IDE_TYPE_COMPLETION_PROPOSAL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-sourceview/ui/ide-completion-list-box-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionListBoxRow, box);
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionListBoxRow, image);
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionListBoxRow, left);
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionListBoxRow, center);
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionListBoxRow, right);
}

static void
ide_completion_list_box_row_init (IdeCompletionListBoxRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_completion_list_box_row_new (void)
{
  return g_object_new (IDE_TYPE_COMPLETION_LIST_BOX_ROW, NULL);
}

/**
 * ide_completion_list_box_row_get_proposal:
 * @self: a #IdeCompletionListBoxRow
 *
 * Gets the proposal viewed by the row.
 *
 * Returns: (transfer none) (nullable): an #IdeCompletionProposal or %NULL
 *
 * Since: 3.32
 */
IdeCompletionProposal *
ide_completion_list_box_row_get_proposal (IdeCompletionListBoxRow *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self), NULL);

  return self->proposal;
}

/**
 * ide_completion_list_box_row_set_proposal:
 * @self: a #IdeCompletionListBoxRow
 * @proposal: an #IdeCompletionProposal
 *
 * Sets the proposal to display in the row.
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_proposal (IdeCompletionListBoxRow *self,
                                          IdeCompletionProposal   *proposal)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));
  g_return_if_fail (!proposal || IDE_IS_COMPLETION_PROPOSAL (proposal));

  if (g_set_object (&self->proposal, proposal))
    {
      if (proposal == NULL)
        {
          gtk_label_set_label (self->left, NULL);
          gtk_label_set_label (self->center, NULL);
          gtk_label_set_label (self->right, NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROPOSAL]);
    }
}

/**
 * ide_completion_list_box_row_set_left:
 * @self: a #IdeCompletionListBoxRow
 * @left: (nullable): text for the left column
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_left (IdeCompletionListBoxRow *self,
                                      const gchar             *left)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  gtk_label_set_label (self->left, left);
}

/**
 * ide_completion_list_box_row_set_left_markup:
 * @self: a #IdeCompletionListBoxRow
 * @left_markup: (nullable): markup for the left column
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_left_markup (IdeCompletionListBoxRow *self,
                                             const gchar             *left_markup)
{
  g_autofree gchar *adjusted = NULL;

  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  /*
   * HACK: For some reason labels ending in a <span fgalpha=xxx> span
   *       cause fgalpha to effect external pango contexts and i have
   *       no idea how/why that is happening.
   */
  if (left_markup != NULL && g_str_has_suffix (left_markup, "</span>"))
    left_markup = adjusted = g_strdup_printf ("%s ", left_markup);

  gtk_label_set_label (self->left, left_markup);
  gtk_label_set_use_markup (self->left, TRUE);
}

/**
 * ide_completion_list_box_row_set_center:
 * @self: a #IdeCompletionListBoxRow
 * @center: (nullable): text for the center column
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_center (IdeCompletionListBoxRow *self,
                                        const gchar             *center)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  gtk_label_set_use_markup (self->center, FALSE);
  gtk_label_set_label (self->center, center);
}

/**
 * ide_completion_list_box_row_set_center_markup:
 * @self: a #IdeCompletionListBoxRow
 * @center_markup: (nullable): markup for the center column
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_center_markup (IdeCompletionListBoxRow *self,
                                               const gchar             *center_markup)
{
  g_autofree gchar *adjusted = NULL;

  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  /*
   * HACK: For some reason labels ending in a <span fgalpha=xxx> span
   *       cause fgalpha to effect external pango contexts and i have
   *       no idea how/why that is happening.
   */
  if (center_markup != NULL && g_str_has_suffix (center_markup, "</span>"))
    center_markup = adjusted = g_strdup_printf ("%s ", center_markup);

  gtk_label_set_label (self->center, center_markup);
  gtk_label_set_use_markup (self->center, TRUE);
}

/**
 * ide_completion_list_box_row_set_right:
 * @self: a #IdeCompletionListBoxRow
 * @right: (nullable): text for the right column
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_right (IdeCompletionListBoxRow *self,
                                       const gchar             *right)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  gtk_label_set_label (self->right, right);
}

/**
 * ide_completion_list_box_row_set_icon_name:
 * @self: a #IdeCompletionListBoxRow
 * @icon_name: (nullable): an icon-name or %NULL
 *
 *
 * Since: 3.32
 */
void
ide_completion_list_box_row_set_icon_name (IdeCompletionListBoxRow *self,
                                           const gchar             *icon_name)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  g_object_set (self->image,
                "icon-name", icon_name,
                NULL);
}

void
_ide_completion_list_box_row_attach (IdeCompletionListBoxRow *self,
                                     GtkSizeGroup            *left,
                                     GtkSizeGroup            *center,
                                     GtkSizeGroup            *right)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  gtk_size_group_add_widget (left, GTK_WIDGET (self->left));
  gtk_size_group_add_widget (center, GTK_WIDGET (self->center));
  gtk_size_group_add_widget (right, GTK_WIDGET (self->right));
}

gint
_ide_completion_list_box_row_get_x_offset (IdeCompletionListBoxRow *self,
                                           GtkWidget               *toplevel)
{
  GtkStyleContext *style_context;
  GtkBorder margin;
  GtkStateFlags flags;
  gint min, nat;
  gint x = 0;

  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX_ROW (self), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (toplevel), 0);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->image));
  flags = gtk_style_context_get_state (style_context);
  gtk_style_context_get_margin (style_context, flags, &margin);
  gtk_widget_get_preferred_width (GTK_WIDGET (self->image), &min, &nat);
  x += nat + margin.left + margin.right;

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->left));
  flags = gtk_style_context_get_state (style_context);
  gtk_style_context_get_margin (style_context, flags, &margin);
  gtk_widget_get_preferred_width (GTK_WIDGET (self->left), &min, &nat);
  x += nat + margin.left + margin.right;

  return x;
}

void
_ide_completion_list_box_row_set_attrs (IdeCompletionListBoxRow *self,
                                        PangoAttrList           *attrs)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (self));

  gtk_label_set_attributes (self->left, attrs);
  gtk_label_set_attributes (self->center, attrs);
  gtk_label_set_attributes (self->right, attrs);
}

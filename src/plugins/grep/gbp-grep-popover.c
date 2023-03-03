/* gbp-grep-popover.c
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

#define G_LOG_DOMAIN "gbp-grep-popover"

#include "config.h"

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-editor.h>

#include "gbp-grep-model.h"
#include "gbp-grep-panel.h"
#include "gbp-grep-popover.h"
#include "gbp-grep-workspace-addin.h"

struct _GbpGrepPopover
{
  GtkPopover      parent_instance;

  GFile          *file;

  GtkEntry       *entry;
  GtkButton      *button;
  GtkCheckButton *regex_button;
  GtkCheckButton *whole_button;
  GtkCheckButton *case_button;
  GtkCheckButton *recursive_button;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_IS_DIRECTORY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGrepPopover, gbp_grep_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
gbp_grep_popover_button_clicked_cb (GbpGrepPopover *self,
                                    GtkButton      *button)
{
  g_autoptr(GbpGrepModel) model = NULL;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspaceAddin *addin;
  IdeWorkspace *workspace;
  GbpGrepPanel *panel;
  IdeContext *context;
  gboolean use_regex;
  gboolean at_word_boundaries;
  gboolean case_sensitive;
  gboolean recursive;

  g_assert (GBP_IS_GREP_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  context = ide_widget_get_context (GTK_WIDGET (workspace));

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_BOTTOM);

  use_regex = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->regex_button));
  at_word_boundaries = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->whole_button));
  case_sensitive = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->case_button));
  recursive = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->recursive_button));

  model = gbp_grep_model_new (context);
  gbp_grep_model_set_directory (model, self->file);
  gbp_grep_model_set_use_regex (model, use_regex);
  gbp_grep_model_set_at_word_boundaries (model, at_word_boundaries);
  gbp_grep_model_set_case_sensitive (model, case_sensitive);
  gbp_grep_model_set_query (model, gtk_editable_get_text (GTK_EDITABLE (self->entry)));

  if (gtk_widget_get_visible (GTK_WIDGET (self->recursive_button)))
    gbp_grep_model_set_recursive (model, recursive);
  else
    gbp_grep_model_set_recursive (model, FALSE);

  addin = ide_workspace_addin_find_by_module_name (workspace, "grep");
  panel = gbp_grep_workspace_addin_get_panel (GBP_GREP_WORKSPACE_ADDIN (addin));

  gbp_grep_panel_set_model (panel, model);
  panel_widget_raise (PANEL_WIDGET (panel));

  gtk_popover_popdown (GTK_POPOVER (self));

  gbp_grep_panel_launch_search (panel);
}

static void
gbp_grep_popover_entry_activate_cb (GbpGrepPopover *self,
                                    GtkEntry       *entry)
{
  g_assert (GBP_IS_GREP_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  gtk_widget_activate (GTK_WIDGET (self->button));
}

static void
gbp_grep_popover_finalize (GObject *object)
{
  GbpGrepPopover *self = (GbpGrepPopover *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gbp_grep_popover_parent_class)->finalize (object);
}

static void
gbp_grep_popover_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpGrepPopover *self = GBP_GREP_POPOVER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_IS_DIRECTORY:
      g_value_set_boolean (value, gtk_widget_get_visible (GTK_WIDGET (self->recursive_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_popover_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpGrepPopover *self = GBP_GREP_POPOVER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      break;

    case PROP_IS_DIRECTORY:
      gtk_widget_set_visible (GTK_WIDGET (self->recursive_button), g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_popover_class_init (GbpGrepPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_grep_popover_finalize;
  object_class->get_property = gbp_grep_popover_get_property;
  object_class->set_property = gbp_grep_popover_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_DIRECTORY] =
    g_param_spec_boolean ("is-directory", NULL, NULL, FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/grep/gbp-grep-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, regex_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, whole_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, case_button);
  gtk_widget_class_bind_template_child (widget_class, GbpGrepPopover, recursive_button);
}

static void
gbp_grep_popover_init (GbpGrepPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gbp_grep_popover_entry_activate_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gbp_grep_popover_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

/* gbp-spell-language-popover.c
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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
 * Adaptation of GspellLanguageChooserButton to show a popover
 * https://wiki.gnome.org/Projects/gspell
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-spell-language-popover"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-spell-language-popover.h"

struct _GbpSpellLanguagePopover
{
  GtkButton             parent_instance;

  GtkPopover           *popover;
  GtkTreeView          *treeview;
  GtkTreeSelection     *selection;
  GtkTreeModel         *store;
  GtkScrolledWindow    *scrolled_window;
  const GspellLanguage *language;

  guint                 default_language : 1;
};

static void language_chooser_iface_init (GspellLanguageChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpSpellLanguagePopover, gbp_spell_language_popover, GTK_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (GSPELL_TYPE_LANGUAGE_CHOOSER, language_chooser_iface_init))

enum {
  PROP_0,
  N_PROPS,

  /* override properties */
  PROP_LANGUAGE,
  PROP_LANGUAGE_CODE,
};

enum
{
  COLUMN_LANGUAGE_NAME,
  COLUMN_LANGUAGE_POINTER,
  N_COLUMNS
};

GbpSpellLanguagePopover *
gbp_spell_language_popover_new (const GspellLanguage *language)
{
  return g_object_new (GBP_TYPE_SPELL_LANGUAGE_POPOVER,
                       "language", language,
                       NULL);
}

static void
scroll_to_selected (GbpSpellLanguagePopover *self)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  if (gtk_tree_selection_get_selected (self->selection, NULL, &iter) &&
      NULL != (path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->store), &iter)))
    {
      gtk_tree_view_scroll_to_cell (self->treeview, path, NULL, TRUE, .5, 0.0);
      gtk_tree_path_free (path);
    }
}

static void
populate_popover (GbpSpellLanguagePopover *self)
{
  const GList *available_langs;
  const gchar *name;
  GtkTreeIter iter;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  available_langs = gspell_language_get_available ();
  for (const GList *l = available_langs; l != NULL; l = l->next)
    {
      name = gspell_language_get_name (l->data);
      gtk_list_store_append (GTK_LIST_STORE (self->store), &iter);
      gtk_list_store_set (GTK_LIST_STORE (self->store), &iter,
                          COLUMN_LANGUAGE_NAME, name,
                          COLUMN_LANGUAGE_POINTER, l->data,
                          -1);
    }
}

static void
treeview_row_activated_cb (GbpSpellLanguagePopover *self,
                           GtkTreePath             *path,
                           GtkTreeViewColumn       *column,
                           GtkTreeView             *treeview)
{
  GspellLanguage *lang;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));
  g_assert (GTK_IS_TREE_VIEW (treeview));

  if (gtk_tree_selection_get_selected (self->selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, COLUMN_LANGUAGE_POINTER, &lang, -1);
      gspell_language_chooser_set_language (GSPELL_LANGUAGE_CHOOSER (self), lang);
      gtk_popover_popdown (self->popover);
    }
}

static GtkPopover *
create_popover (GbpSpellLanguagePopover *self)
{
  GtkPopover *popover;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  self->treeview = g_object_new (GTK_TYPE_TREE_VIEW,
                                 "headers-visible", FALSE,
                                 "visible", TRUE,
                                 "expand", TRUE,
                                 NULL);

  self->selection = gtk_tree_view_get_selection (self->treeview);
  gtk_tree_selection_set_mode (self->selection, GTK_SELECTION_BROWSE);

  self->store = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, GSPELL_TYPE_LANGUAGE));
  gtk_tree_view_set_model (self->treeview, GTK_TREE_MODEL (self->store));
  gtk_tree_view_insert_column_with_attributes (self->treeview, -1, NULL,
                                               gtk_cell_renderer_text_new (),
                                               "text", COLUMN_LANGUAGE_NAME,
                                               NULL);

  self->scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                        "visible", TRUE,
                                        "expand", TRUE,
                                        "hscrollbar-policy", GTK_POLICY_NEVER,
                                        "max-content-height", 400,
                                        "propagate-natural-height", TRUE,
                                        NULL);
  popover = g_object_new (GTK_TYPE_POPOVER,
                          "relative-to", self,
                          "position", GTK_POS_TOP,
                          NULL);

  gtk_container_add (GTK_CONTAINER (self->scrolled_window), GTK_WIDGET (self->treeview));
  gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (self->scrolled_window));

  g_signal_connect_object (self->treeview,
                           "row-activated",
                           G_CALLBACK (treeview_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return popover;
}

static void
select_language (GbpSpellLanguagePopover *self,
                 const GspellLanguage    *language)
{
  GtkTreeModel *model = (GtkTreeModel *)self->store;
  const GspellLanguage *lang;
  GtkTreeIter iter;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  if (self->treeview == NULL ||
      language == NULL ||
      !gtk_tree_model_get_iter_first (model, &iter))
    return;

  g_assert (GTK_IS_TREE_VIEW (self->treeview));
  g_assert (GTK_IS_TREE_MODEL (self->store));

  do
    {
      gtk_tree_model_get (model, &iter, COLUMN_LANGUAGE_POINTER, &lang, -1);
      if (self->language == lang)
        gtk_tree_selection_select_iter (self->selection, &iter);
    }
  while (gtk_tree_model_iter_next (model, &iter));
}

static void
gbp_spell_language_popover_button_clicked (GtkButton *button)
{
  GbpSpellLanguagePopover *self = (GbpSpellLanguagePopover *)button;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  gspell_language_chooser_set_language (GSPELL_LANGUAGE_CHOOSER (self),
                                        self->default_language ? NULL : self->language);

  if (self->popover == NULL)
    {
      self->popover = g_object_ref (create_popover (self));
      populate_popover (self);
    }

  gtk_popover_popup (self->popover);
  select_language (self, self->language);
  scroll_to_selected (self);
}

static void
update_button_label (GbpSpellLanguagePopover *self)
{
  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  if (self->language != NULL)
    gtk_button_set_label (GTK_BUTTON (self), gspell_language_get_name (self->language));
  else
    gtk_button_set_label (GTK_BUTTON (self), _("No language selected"));
}

static void
gbp_spell_language_popover_set_language (GspellLanguageChooser *chooser,
                                                const GspellLanguage  *language)
{
  GbpSpellLanguagePopover *self = (GbpSpellLanguagePopover *)chooser;
  gboolean notify_language_code = FALSE;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  if (self->default_language != (language == NULL))
    {
      self->default_language = (language == NULL);
      notify_language_code = TRUE;
    }

  if (language == NULL && NULL == (language = gspell_language_get_default ()) && self->selection != NULL)
    gtk_tree_selection_unselect_all (self->selection);

  if (self->language != language)
    {
      self->language = language;
      update_button_label (self);

      g_object_notify (G_OBJECT (chooser), "language");
      notify_language_code = TRUE;
    }

  if (notify_language_code)
    g_object_notify (G_OBJECT (chooser), "language-code");

  select_language (self, self->language);
}

static const GspellLanguage *
gbp_spell_language_popover_get_language_full (GspellLanguageChooser *chooser,
                                              gboolean              *default_language)
{
  GbpSpellLanguagePopover *self = (GbpSpellLanguagePopover *)chooser;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  if (default_language != NULL)
    *default_language = self->default_language;

  return self->language;
}

static void
gbp_spell_language_popover_finalize (GObject *object)
{
  GbpSpellLanguagePopover *self = (GbpSpellLanguagePopover *)object;

  g_clear_object (&self->popover);

  G_OBJECT_CLASS (gbp_spell_language_popover_parent_class)->finalize (object);
}

static void
gbp_spell_language_popover_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GspellLanguageChooser *chooser = GSPELL_LANGUAGE_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_value_set_boxed (value, gspell_language_chooser_get_language (chooser));
      break;

    case PROP_LANGUAGE_CODE:
      g_value_set_string (value, gspell_language_chooser_get_language_code (chooser));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_language_popover_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GspellLanguageChooser *chooser = GSPELL_LANGUAGE_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      gspell_language_chooser_set_language (chooser, g_value_get_boxed (value));
      break;

    case PROP_LANGUAGE_CODE:
      gspell_language_chooser_set_language_code (chooser, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_language_popover_constructed (GObject *object)
{
  GbpSpellLanguagePopover *self = (GbpSpellLanguagePopover *)object;

  g_assert (GBP_IS_SPELL_LANGUAGE_POPOVER (self));

  G_OBJECT_CLASS (gbp_spell_language_popover_parent_class)->constructed (object);

  update_button_label (self);
}

static void
gbp_spell_language_popover_class_init (GbpSpellLanguagePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  object_class->finalize = gbp_spell_language_popover_finalize;
  object_class->get_property = gbp_spell_language_popover_get_property;
  object_class->set_property = gbp_spell_language_popover_set_property;
  object_class->constructed = gbp_spell_language_popover_constructed;

  button_class->clicked = gbp_spell_language_popover_button_clicked;

  g_object_class_override_property (object_class, PROP_LANGUAGE, "language");
  g_object_class_override_property (object_class, PROP_LANGUAGE_CODE, "language-code");
}

static void
gbp_spell_language_popover_init (GbpSpellLanguagePopover *self)
{
}

static void
language_chooser_iface_init (GspellLanguageChooserInterface *iface)
{
  iface->get_language_full = gbp_spell_language_popover_get_language_full;
  iface->set_language = gbp_spell_language_popover_set_language;
}

/*
 * gbp-git-commit-entry.c
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

#include <libide-gui.h>

#include "gbp-git-commit-entry.h"

struct _GbpGitCommitEntry
{
  GtkSourceView parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitCommitEntry, gbp_git_commit_entry, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPS];

static gboolean
style_scheme_name_to_object (GBinding     *binding,
                             const GValue *value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  const char *name = g_value_get_string (value);

  if (name != NULL)
    {
      GtkSourceStyleSchemeManager *m = gtk_source_style_scheme_manager_get_default ();
      g_value_set_object (to_value, gtk_source_style_scheme_manager_get_scheme (m, name));
    }

  return TRUE;
}

static void
gbp_git_commit_entry_dispose (GObject *object)
{
  GbpGitCommitEntry *self = (GbpGitCommitEntry *)object;

  G_OBJECT_CLASS (gbp_git_commit_entry_parent_class)->dispose (object);
}

static void
gbp_git_commit_entry_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGitCommitEntry *self = GBP_GIT_COMMIT_ENTRY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_entry_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGitCommitEntry *self = GBP_GIT_COMMIT_ENTRY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_entry_class_init (GbpGitCommitEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_git_commit_entry_dispose;
  object_class->get_property = gbp_git_commit_entry_get_property;
  object_class->set_property = gbp_git_commit_entry_set_property;

  //widget_class->measure = gbp_git_commit_entry_measure;
}

static void
gbp_git_commit_entry_init (GbpGitCommitEntry *self)
{
  GtkSourceLanguageManager *lm;
  GtkSourceLanguage *lang;
  GtkTextBuffer *buffer;

  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self), 12);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self), TRUE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  lm = gtk_source_language_manager_get_default ();
  lang = gtk_source_language_manager_get_language (lm, "git-commit");
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);

  g_object_bind_property_full (IDE_APPLICATION_DEFAULT, "style-scheme",
                               buffer, "style-scheme",
                               G_BINDING_SYNC_CREATE,
                               style_scheme_name_to_object, NULL, NULL, NULL);
}

/* ide-source-view-addins.c
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

#define G_LOG_DOMAIN "ide-source-view-addins"

#include "config.h"

#include "ide-source-view-private.h"

#define DISABLED_LANGUAGE_ID "plain"

static void
ide_source_view_completion_provider_added_cb (IdeExtensionSetAdapter *adapter,
                                              PeasPluginInfo         *plugin_info,
                                              PeasExtension          *exten,
                                              gpointer                user_data)
{
  GtkSourceCompletionProvider *provider = (GtkSourceCompletionProvider *)exten;
  IdeSourceView *self = user_data;
  GtkSourceCompletion *completion;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Adding completion provider %s from module %s",
           G_OBJECT_TYPE_NAME (provider),
           peas_plugin_info_get_module_name (plugin_info));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_add_provider (completion, provider);

  IDE_EXIT;
}

static void
ide_source_view_completion_provider_removed_cb (IdeExtensionSetAdapter *adapter,
                                                PeasPluginInfo         *plugin_info,
                                                PeasExtension          *exten,
                                                gpointer                user_data)
{
  GtkSourceCompletionProvider *provider = (GtkSourceCompletionProvider *)exten;
  IdeSourceView *self = user_data;
  GtkSourceCompletion *completion;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Removing completion provider %s from module %s",
           G_OBJECT_TYPE_NAME (provider),
           peas_plugin_info_get_module_name (plugin_info));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_remove_provider (completion, provider);

  IDE_EXIT;
}

static void
ide_source_view_hover_provider_added_cb (IdeExtensionSetAdapter *adapter,
                                         PeasPluginInfo         *plugin_info,
                                         PeasExtension          *exten,
                                         gpointer                user_data)
{
  GtkSourceHoverProvider *provider = (GtkSourceHoverProvider *)exten;
  IdeSourceView *self = user_data;
  GtkSourceHover *hover;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Adding hover provider %s from module %s",
           G_OBJECT_TYPE_NAME (provider),
           peas_plugin_info_get_module_name (plugin_info));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));
  gtk_source_hover_add_provider (hover, provider);

  IDE_EXIT;
}

static void
ide_source_view_hover_provider_removed_cb (IdeExtensionSetAdapter *adapter,
                                           PeasPluginInfo         *plugin_info,
                                           PeasExtension          *exten,
                                           gpointer                user_data)
{
  GtkSourceHoverProvider *provider = (GtkSourceHoverProvider *)exten;
  IdeSourceView *self = user_data;
  GtkSourceHover *hover;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Removing hover provider %s from module %s\n",
           G_OBJECT_TYPE_NAME (provider),
           peas_plugin_info_get_module_name (plugin_info));

  hover = gtk_source_view_get_hover (GTK_SOURCE_VIEW (self));
  gtk_source_hover_remove_provider (hover, provider);

  IDE_EXIT;
}

static void
ide_source_view_indenter_added_cb (IdeExtensionSetAdapter *adapter,
                                   PeasPluginInfo         *plugin_info,
                                   PeasExtension          *exten,
                                   gpointer                user_data)
{
  GtkSourceIndenter *indenter = (GtkSourceIndenter *)exten;
  IdeSourceView *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_INDENTER (indenter));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Adding indenter %s from module %s",
           G_OBJECT_TYPE_NAME (indenter),
           peas_plugin_info_get_module_name (plugin_info));

  // TODO: how to handle multiple indenters?
  gtk_source_view_set_indenter (GTK_SOURCE_VIEW (self), indenter);

  IDE_EXIT;
}

static void
ide_source_view_indenter_removed_cb (IdeExtensionSetAdapter *adapter,
                                     PeasPluginInfo         *plugin_info,
                                     PeasExtension          *exten,
                                     gpointer                user_data)
{
  GtkSourceIndenter *indenter = (GtkSourceIndenter *)exten;
  IdeSourceView *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (GTK_SOURCE_IS_INDENTER (indenter));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_debug ("Removing indenter %s from module %s",
           G_OBJECT_TYPE_NAME (indenter),
           peas_plugin_info_get_module_name (plugin_info));

  // TODO: how to handle multiple indenters?
  if (indenter == gtk_source_view_get_indenter (GTK_SOURCE_VIEW (self)))
    gtk_source_view_set_indenter (GTK_SOURCE_VIEW (self), NULL);

  IDE_EXIT;
}

void
_ide_source_view_addins_init (IdeSourceView     *self,
                              GtkSourceLanguage *language)
{
  const char *language_id;
  IdeObjectBox *parent;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));
  g_return_if_fail (self->completion_providers == NULL);
  g_return_if_fail (self->hover_providers == NULL);

  if (language != NULL)
    language_id = gtk_source_language_get_id (language);
  else
    language_id = DISABLED_LANGUAGE_ID;

  /* Get a handle to the buffers "Box" on the object tree */
  parent = ide_object_box_from_object (G_OBJECT (self->buffer));

  /* Create our completion providers and attach them */
  self->completion_providers =
    ide_extension_set_adapter_new (IDE_OBJECT (parent),
                                   peas_engine_get_default (),
                                   GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                   "Completion-Provider-Languages",
                                   language_id);
  g_signal_connect (self->completion_providers,
                    "extension-added",
                    G_CALLBACK (ide_source_view_completion_provider_added_cb),
                    self);
  g_signal_connect (self->completion_providers,
                    "extension-removed",
                    G_CALLBACK (ide_source_view_completion_provider_removed_cb),
                    self);
  ide_extension_set_adapter_foreach (self->completion_providers,
                                     ide_source_view_completion_provider_added_cb,
                                     self);

  /* Create our hover providers and attach them */
  self->hover_providers =
    ide_extension_set_adapter_new (IDE_OBJECT (parent),
                                   peas_engine_get_default (),
                                   GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                   "Hover-Provider-Languages",
                                   language_id);
  g_signal_connect (self->hover_providers,
                    "extension-added",
                    G_CALLBACK (ide_source_view_hover_provider_added_cb),
                    self);
  g_signal_connect (self->hover_providers,
                    "extension-removed",
                    G_CALLBACK (ide_source_view_hover_provider_removed_cb),
                    self);
  ide_extension_set_adapter_foreach (self->hover_providers,
                                     ide_source_view_hover_provider_added_cb,
                                     self);

  /* Create our hover providers and attach them */
  self->indenters =
    ide_extension_set_adapter_new (IDE_OBJECT (parent),
                                   peas_engine_get_default (),
                                   GTK_SOURCE_TYPE_INDENTER,
                                   "Indenter-Languages",
                                   language_id);
  g_signal_connect (self->indenters,
                    "extension-added",
                    G_CALLBACK (ide_source_view_indenter_added_cb),
                    self);
  g_signal_connect (self->indenters,
                    "extension-removed",
                    G_CALLBACK (ide_source_view_indenter_removed_cb),
                    self);
  ide_extension_set_adapter_foreach (self->indenters,
                                     ide_source_view_indenter_added_cb,
                                     self);

  IDE_EXIT;
}

void
_ide_source_view_addins_shutdown (IdeSourceView *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  ide_clear_and_destroy_object (&self->completion_providers);
  ide_clear_and_destroy_object (&self->hover_providers);
  ide_clear_and_destroy_object (&self->indenters);

  IDE_EXIT;
}

void
_ide_source_view_addins_set_language (IdeSourceView     *self,
                                      GtkSourceLanguage *language)
{
  const char *language_id;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));
  g_return_if_fail (self->completion_providers != NULL);
  g_return_if_fail (self->hover_providers != NULL);
  g_return_if_fail (self->indenters != NULL);

  if (language != NULL)
    language_id = gtk_source_language_get_id (language);
  else
    language_id = DISABLED_LANGUAGE_ID;

  ide_extension_set_adapter_set_value (self->completion_providers, language_id);
  ide_extension_set_adapter_set_value (self->hover_providers, language_id);
  ide_extension_set_adapter_set_value (self->indenters, language_id);

  IDE_EXIT;
}

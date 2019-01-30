/* gbp-code-index-application-addin.c
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

#define G_LOG_DOMAIN "gbp-code-index-application-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-code-index-application-addin.h"

struct _GbpCodeIndexApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_code_index_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                     IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  g_application_add_main_option (G_APPLICATION (application),
                                 "index",
                                 'i',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_FILENAME,
                                 _("Create or update code-index for project file"),
                                 _("PROJECT_FILE"));
}

static void
gbp_code_index_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                      IdeApplication          *application,
                                                      GApplicationCommandLine *cmdline)
{
  g_autofree gchar *project_path = NULL;
  GVariantDict *options;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!(options = g_application_command_line_get_options_dict (cmdline)) ||
      !g_variant_dict_contains (options, "index") ||
      !g_variant_dict_lookup (options, "index", "^ay", &project_path))
    return;

  ide_application_set_command_line_handled (application, cmdline, TRUE);

  g_print ("Re-index %s\n", project_path);
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->add_option_entries = gbp_code_index_application_addin_add_option_entries;
  iface->handle_command_line = gbp_code_index_application_addin_handle_command_line;
}

G_DEFINE_TYPE_WITH_CODE (GbpCodeIndexApplicationAddin, gbp_code_index_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_code_index_application_addin_class_init (GbpCodeIndexApplicationAddinClass *klass)
{
}

static void
gbp_code_index_application_addin_init (GbpCodeIndexApplicationAddin *self)
{
}

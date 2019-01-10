/* ide-golang-application-addin.h
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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
 */

#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-editor.h>

#include "ide-golang-application-addin.h"

struct _IdeGolangApplicationAddin
{
  GObject parent_instance;
  gchar *golang_version;
};

static void application_addin_iface_init (IdeApplicationAddinInterface *iface);
static IdeGolangApplicationAddin *golang_application_addin = NULL;

G_DEFINE_TYPE_EXTENDED (IdeGolangApplicationAddin,
                        ide_golang_application_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN,
                                               application_addin_iface_init))

static void
ide_golang_application_addin_class_init (IdeGolangApplicationAddinClass *klass)
{
}

static void
ide_golang_application_addin_init (IdeGolangApplicationAddin *addin)
{
  addin->golang_version = g_strdup("unknown");
}

static const gchar *goversion_pattern = "^go version (.*)\n?$";

static void
ide_golang_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *stdoutstr = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GMatchInfo) match_info = NULL;
  IdeGolangApplicationAddin *self = (IdeGolangApplicationAddin *)addin;

  golang_application_addin = self;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  if (launcher == NULL)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "An unknown error ocurred");

      g_error ("%s", error->message);
      g_error_free (error);
      IDE_EXIT;
    }

  ide_subprocess_launcher_push_argv (launcher, "go");
  ide_subprocess_launcher_push_argv (launcher, "version");
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  if (subprocess == NULL)
    {
      g_assert (error != NULL);
      g_error ("%s", error->message);
      g_error_free (error);
      IDE_EXIT;
    }

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdoutstr, NULL, &error))
    {
      g_assert(error != NULL);
      g_error("Unable to communicate with subprocess while fetching golang version: %s", error->message);
      IDE_EXIT;
    }

  if (!ide_subprocess_wait (subprocess, NULL, &error))
    {
      g_assert(error != NULL);
      g_error("Unable to wait when communication with go version: %s", error->message);
      IDE_EXIT;
    }

  if (!(regex = g_regex_new (goversion_pattern, G_REGEX_MULTILINE, 0, &error)))
    {
      g_assert(error != NULL);
      g_error("Unable to create regex when parsing golang version: %s", error->message);
      IDE_EXIT;
    }

  // Search for golang version and return immediately when found.
  g_regex_match (regex, stdoutstr, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      gint begin = 0;
      gint end = 0;

      if (g_match_info_fetch_pos (match_info, 1, &begin, &end))
        {
          g_free (self->golang_version);
          self->golang_version = g_strndup(&stdoutstr[begin], end - begin);
          g_debug("Found golang version: %s", self->golang_version);
          IDE_EXIT;
        }

      g_match_info_next (match_info, &error);
    }

  IDE_EXIT;
}

static void
ide_golang_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
}


static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = ide_golang_application_addin_load;
  iface->unload = ide_golang_application_addin_unload;
}

const gchar *golang_get_go_version(void)
{
  return golang_application_addin->golang_version;
}

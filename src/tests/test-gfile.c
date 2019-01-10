#include <libide-io.h>

static void
test_uncanonical_file (void)
{
  static const struct {
    const gchar *file;
    const gchar *other;
    const gchar *result;
  } tests[] = {
    { "/home/alberto/.var/app/org.gnome.Builder/cache/gnome-builder/projects/gtask-example/builds/org.gnome.Gtask-Example.json-0601fcfb2fbf01231dd228e0b218301c589ae573-local-flatpak-org.gnome.Platform-x86_64-master",
      "/home/alberto/Projects/gtask-example/src/main.c",
      "/home/alberto/.var/app/org.gnome.Builder/cache/gnome-builder/projects/gtask-example/builds/org.gnome.Gtask-Example.json-0601fcfb2fbf01231dd228e0b218301c589ae573-local-flatpak-org.gnome.Platform-x86_64-master/../../../../../../../../../Projects/gtask-example/src/main.c" },
    { "/home/xtian/foo",
      "/home/xtian/foo/bar",
      "/home/xtian/foo/bar" },
    { "/home/xtian/foo",
      "/home/xtian/bar",
      "/home/xtian/foo/../bar" },
    { "/home/xtian/foo",
      "/",
      "/home/xtian/foo/../../../" },
  };

  for (guint i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_autoptr(GFile) file = g_file_new_for_path (tests[i].file);
      g_autoptr(GFile) other = g_file_new_for_path (tests[i].other);
      g_autofree gchar *result = ide_g_file_get_uncanonical_relative_path (file, other);

      g_assert_cmpstr (tests[i].result, ==, result);
    }
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/GLib/uncanonical-file", test_uncanonical_file);
  return g_test_run ();
}

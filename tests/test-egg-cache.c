#include "egg-task-cache.h"

static GMainLoop *main_loop;
static EggTaskCache *cache;
static GObject *foo;

static void
populate_callback (EggTaskCache  *self,
                   gconstpointer  key,
                   GTask         *task)
{
  foo = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_weak_pointer (G_OBJECT (foo), (gpointer *)&foo);
  g_task_return_pointer (task, foo, g_object_unref);
}

static void
get_foo_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  GError *error = NULL;
  GObject *ret;

  ret = egg_task_cache_get_finish (cache, result, &error);
  g_assert_no_error (error);
  g_assert (ret != NULL);
  g_assert (ret == foo);

  g_assert (egg_task_cache_evict (cache, "foo"));
  g_object_unref (ret);

  g_main_loop_quit (main_loop);
}

static void
test_task_cache (void)
{
  main_loop = g_main_loop_new (NULL, FALSE);
  cache = egg_task_cache_new (g_str_hash,
                              g_str_equal,
                              (GBoxedCopyFunc)g_strdup,
                              (GBoxedFreeFunc)g_free,
                              populate_callback,
                              100 /* msec */);

  g_assert (!egg_task_cache_peek (cache, "foo"));
  g_assert (!egg_task_cache_evict (cache, "foo"));

  egg_task_cache_get_async (cache, "foo", NULL, get_foo_cb, NULL);

  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  g_assert (foo == NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Egg/TaskCache/basic", test_task_cache);
  return g_test_run ();
}

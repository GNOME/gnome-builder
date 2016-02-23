# Contributing to GNOME Builder

We would love for you to contribute to GNOME Builder!

GNOME Builder is a new IDE focusing on the GNOME desktop environment. It is
primarily written in C, but allows for plugins in C, C++, Python or Vala.
Additional compiled languages can also be supported with some work.

Remember that the GNOME community is largely made up of part-time contributors
that do this for fun. Be respectful and assume the best of each other.

## Filing a bug

Please file bugs for issues, enhancements and features on
[our Bugzilla](https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-builder)
bug tracker.

To submit a patch, file a bug and attach your patch to the bug.
Someone will review your patch shortly after doing so.

## Asking for Help

You can often find the other contributors to GNOME Builder on our IRC channel at
[irc://irc.gimp.net/#gnome-builder](irc://irc.gimp.net/#gnome-builder).
If you have any questions, we'd be happy to help you.

In particular, if you'd like to start on a new plugin or feature, stop by
our IRC channel and we'd be happy to get you set off in the right direction.

## Portability

GNOME Builder is primarily focused on **GNU/Linux** with the **GNOME desktop**.

However, we do often accept patches for various BSD-based operating systems
and alternate desktops. It is important that you can help keep things working
for your platform, as the Builder team cannot individually test all
configurations.

## Testing

We use autotools for our build system. It provides support for running unit
tests with the `make check` command. We run these tests before releasing
tarballs, so it is important that you check tests and add new tests when it
makes sense.

## Licensing

Contributions should be licensed under the LGPL-2.1+ or GPL-3. Permissive
licensed contributions will also be accepted, but we prefer that original
contributions are either LGPL-2.1+ or GPL-3 licensed.

## Coding Style

Our coding style matches that of Gtk+. We consider the Gtk+ project an
upstream, and often push features into Gtk+. This might feel unfamiliar
at first, but it works well.

We use the recent GNU GCC '11 mode, such as -std=gnu11.

```c
static GtkWidget *
example_object_get_widget (ExampleObject  *object,
                           GError        **error)
{
  g_return_val_if_fail (EXAMPLE_IS_OBJECT (object), NULL);

  if (object->widget == NULL)
    {
      object->widget = g_object_new (GTK_TYPE_LABEL,
                                     "visible", TRUE,
                                     NULL);
    }

  return object->widget;
}
```

Please use new-style Object declarations. Unless you intend to subclass the
object, make the object final.

```c
#define EXAMPLE_TYPE_OBJECT (example_object_get_type())
G_DECLARE_FINAL_TYPE (ExampleObject, example_object, EXAMPLE, OBJECT, GObject)
```

Builder's default C mode matches our style guide.

## Documentation

Most functions should be obvious what they do from the object type, name,
and parameters. Add additional documentation when it makes sense.

If you find you come across something particularly tricky, or are being clever,
please add a comment denoting such.


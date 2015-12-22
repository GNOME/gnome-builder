# IDE Scripting Examples

This directory contains various examples you can take inspiration from when
writing a simple script for GNOME Builder.

Scripting is different from plugins in that scripting allows the user to
write short, one-off extensions to their workflow that does not warrant
a full plugin.

Such an example might be updating ane external resource when a file is saved.

Simply place your `*.py` file in the proper directory, and it will be loaded
when the `Ide.Context` is initialized.

# IDE Scripting Examples

This directory contains various examples you can take inspiration from when
writing your own IDE extensions.

Extensions are placed in ~/.config/gnome-builder/scripts/. Currently,
JavaScript and Python 3 are supported. However, more languages may be
added in the future based on demands.

Simply place a `*.js` or `*.py` file in the proper directory, and it will
be loaded when the `Ide.Context` is initialized.

# BuildConfig

Once upon a time, Builder needed a simple format to track build configurations
so that we could build projects and associate the proper SDK/Runtime, configure
options, prefix, and other tweaks.

The format for this file was called ".buildconfig" following in the footsteps
of ".editorconfig" files.

It looked something like:

```
[default]
id = default
default = true
name = Default Configuration
prefix = /app
runtime = flatpak:org.gnome.Sdk
device = local
```

However, as we grew the configuration engine, we allowed configuration backends
so this format no longer became a requirement. Therefore, it was abstracted
into the buildconfig embedded plugin.

It continues to provide a simple configuration provider based on .buildconfig
files.

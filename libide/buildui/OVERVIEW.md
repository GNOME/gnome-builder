# Build UI

This embedded plugin provides UI for various build components such as the build
panel and the build log panel.

Additionally, it provides the perspective for altering build configurations.
The configuration providers can use IdeBuildConfigurationView directly or with
subclasses to allow altering their build configuration.

The ide-build-tool.c provides a command line tool that can execute a build for a
project when run from the source directory.
 
```sh
gnome-builder-cli build
```

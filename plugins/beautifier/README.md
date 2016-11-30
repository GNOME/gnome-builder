# Beautifier plugin usage:

To be able to trigger a beautify action you need:

- Act on a file with a recognized language (or change the language in Builder interface)
- Have a command configured for this language (or the menu will be empty)
- The command executable need to be reachable from your $PATH
- Some text need to be selected.

Then, two possible actions:
- A default entry is defined: pressing &lt;ctrl&gt;&lt;alt&gt;b trigger the beautify actions.
- You can manually choose an entry in the contextual menu at "Selection-> Beautify" submenu.

# Beautifier plugin configuration :

Every configuration files use the GKeyFile GLib format:
https://developer.gnome.org/glib/unstable/glib-Key-value-file-parser.html

The beautifier plugin has three layers of configuration:
- user wide:

  ~/.config/gnome-builder/beautifier_plugin

- project wide:

  .beautifier folder at project root.

- system wide, installed by gnome-builder:

  $PREFIX/share/gnome-builder/plugins/beautifier_plugin

Each layer has a priority in the same order:
- user
- project
- system

This count for the language id groups, but for the default entry too.

In each layer you will find:
- a global.ini file.
- any number of languages (using GtkSourceView lang_id) or group-of-languages (mapped in global.ini) folders.

In each language or group-of-languages folder, you will find:
- A config.ini
- Any numbers of configuration files used by your beautifier commands.

```
	[layer root]
		global.ini
		[lang_id folder]
			config.ini
			command_config_file.cfg (or any other file extension)
			another_config_file.cfg
		[mapped folder]
			config.ini
			command_config_file.cfg (or any other file extension)
			another_config_file.cfg
```

## global.ini groups and keys:

The global.ini file define options for a specific language.
The group name need be one of the GtkSourveView language id (lang_id).

lang_ids are the same used by the language-specs GtkSourceView .lang files:
https://git.gnome.org/browse/gtksourceview/tree/data/language-specs

```
	[c]
	[java]
	[javascript]

	etc...
```

The possible keys are:
- map = language or group-of-languages folder name for the layer.
- default = default profile to get the accelerator (<ctrl><alt>b)

This permit to reference the same folder from different languages:

```
	[chdr]
	map = c_family
	default = k&r.cfg

	[objc]
	map = c_family
```

## config.ini groups and keys:

The config.ini file define configuration to display and launch a beaufity action.
Groups are named with one of your command configuration file found in the same folder.
The keys are:
- command = the command to launch (currently, uncrustify or clang-format)
- name = the real name to display in the menu

A specific group named [global] allow folder wide configuration.
Currently, there's only one key: default = default configuration file

## Example of configuration:

```
	[layer root]
		global.ini
		[c_family]
			config.ini
			k&r.cfg
			gnu.cfg

		[c]
			config.ini
			my_config.cfg

	global.ini:
		[chdr]
		map = c_family
		default = k&r.cfg

		[objc]
		map = c_family

	[c_family] config.ini:
		[global]
		default = k&r.cfg

		[k&r.cfg]
		command = uncrustify
		name = Kernighan and Ritchie

		[gnu.cfg]
		command = uncrustify
		name = Gnu Style

	[c] config.ini:
		[global]
		default = my_config.cfg

		[my_config.cfg]
		command = uncrustify
		name = my C style
```

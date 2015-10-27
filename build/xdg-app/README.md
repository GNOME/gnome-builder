# Building an Xdg-App

## Get latest xdg-app

Install xdg-app from `git://anongit.freedesktop.org/xdg-app/xdg-app`.
It contains a few fixes required for Builder.

```sh
jhbuild build xdg-app
```

## Install the GNOME SDK GPG key

```sh
wget http://sdk.gnome.org/keys/gnome-sdk.gpg
gpg --import gnome-sdk.gpg
rm gnome-sdk.gpg
```

## Get the GNOME SDK Runtime

We need the SDK instead of the normal application runtime since it includes
things like clang for us to use to compile.

```sh
xdg-app add-remote --user gnome-sdk http://sdk.gnome.org/repo/
xdg-app install-runtime --user gnome-sdk org.gnome.Sdk 3.18
```

## Build the Xdg-App

```sh
make dist
cd build/xdg-app
./build.sh gnome-builder.def
```

## Install the Xdg-App

We need to add the location where we built the application to our xdg-app
repos.  Alternatively, you could export a single file and import that into your
user xdg-app tree.

I'm almost positive there is a better way to do this, but this will get you
started.

```sh
xdg-app add-remote --user gnome-builder ./repo
xdg-app install-app gnome-builder org.gnome.Builder
```

If you've already done the above, and you made a new build, you'll need to
update the app in your xdg-app tree instead.

```sh
xdg-app update-app --user org.gnome.Builder
```

## Run the Xdg-App

```sh
xdg-app run org.gnome.Builder
```

If you want to look around in the runtime environment, you might consider
using the following.

```sh
xdg-app run --devel --command=bash org.gnome.Builder
```

This will give you a shell as if you replaced the `gnome-builder` executable
with `bash`.

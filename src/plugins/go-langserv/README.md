# a quick langserver plugin for go

Meant to wrap [Sourcegraph's](https://github.com/sourcegraph/go-langserver)
`go-langserver`. It's a straight copy & paste of the rust-langserver
implementation, with some sketchy additional `bash` magic to try and better
support flatpak-packaged Builder.

## Installing and testing

1. Install `go`. Its tooling is stable, 'apt install' or 'yum install' will be fine.
2. Set up your go environment-
```
export GOPATH=$HOME/go
export PATH="$GOPATH/bin:$PATH"
go get github.com/sourcegraph/go-langserver
go install github.com/sourcegraph/go-langserver
```
3. Launch Builder, open ~/go/src/github.com/sourcegraph as a project
4. Right click on a method and click 'go to definition'

## Runtime configuration
Go has a simple environment convention for managing source code. All user code
imports come from $GOPATH/src, so to manage separate projects, you can change
your $GOPATH- think of it as a one-variable equivalent to Python's
`virtualenv`. Developing inside of $GOPATH/src/$PROJECT is the encouraged way
to do things, but you don't have to. Newer versions of Go define a default
'global scrum' $GOPATH in $HOME/go.

The standard library is in $GOROOT/src. If you manually install `go` to a place
that isn't `/usr/local/go`, you'll need to set this, but normally you don't have
to worry about it.

## Bugs:
* I have to disable the rust-langserver extension, or else it seems to take
  precedence
* `go-langserver` claims to support formatting, but I don't know how to ask
  Builder to ask it to do that
* `go-langserver` has recently merged completion support, but it seems crashy
  from Builder

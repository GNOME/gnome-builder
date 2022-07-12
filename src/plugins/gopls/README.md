# A Language server plugin for go

This plugin provides an implementation of the language server support for Go. `gopls` is the new standard language server implementation for Go which this plugin uses.

## Installing and testing

1. Install `go`. Its tooling is stable, 'apt install' or 'yum install' will be fine.
2. Set up your go environment-
```
export GOPATH=$HOME/go
export PATH="$GOPATH/bin:$PATH"
```
3. Install `gopls`, refer https://github.com/golang/tools/blob/master/gopls/README.md
Either
```
go get golang.org/x/tools/gopls@latest
```
or if it fails
```
GO111MODULE=on go get golang.org/x/tools/gopls@latest
```
4. Launch Builder, open any Go source file
5. Right click on a method and click 'go to definition'


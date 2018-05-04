## Ubuntu

It works.

## Debian(stretch) 

It works. 

Todo
 [] add llvm-5 repo for all supported versions (only latest for now)

## Arch Linux 

It works!

## Fedora

It works!

## CentOS

* Vagrant box: https://app.vagrantup.com/centos/boxes/7
* Same package as Fedora: https://copr.fedorainfracloud.org/coprs/alonid/llvm-5.0.0/
* Same issue, distro not recognised by script

## Alpine Linux

* Vagrant box: https://app.vagrantup.com/generic/boxes/alpine36
* Package to be added: https://pkgs.alpinelinux.org/package/edge/main/x86_64/llvm5-dev
* Same issue, distro not recognised by script


## OSX

* Package to be installed is To install: http://formulae.brew.sh/formula/llvm@5

* Trying using Homebrew, but getting

```
Installing solidity dependencies on macOS 10.13 High Sierra.
Error: /usr/local/Homebrew is not writable. You should change the
ownership and permissions of /usr/local/Homebrew back to your
user account:
  sudo chown -R $(whoami) /usr/local/Homebrew
```

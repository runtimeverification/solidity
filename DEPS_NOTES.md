## Ubuntu

It works.

## Debian(stretch) 

It works. 

Todo
 [] add llvm-5 repo for all supported versions (only latest for now)

## Fedora

* Vagrant box: https://app.vagrantup.com/generic/boxes/fedora27
* Package to be installed seems to be https://copr.fedorainfracloud.org/coprs/alonid/llvm-5.0.0/ (shared with CentOS)

Looks like `install_deps.sh` fails to recognise the distro:

```
ERROR - Unsupported or unidentified Linux distro.
See http://solidity.readthedocs.io/en/latest/installing-solidity.html for manual instructions.
If you would like to get your distro working, that would be fantastic.
Drop us a message at https://gitter.im/ethereum/solidity-dev.
```

(this is also true for everything else apart from Ubuntu, Debian and Darwin)

## CentOS

* Vagrant box: https://app.vagrantup.com/centos/boxes/7
* Same package as Fedora: https://copr.fedorainfracloud.org/coprs/alonid/llvm-5.0.0/
* Same issue, distro not recognised by script

## Alpine Linux

* Vagrant box: https://app.vagrantup.com/generic/boxes/alpine36
* Package to be added: https://pkgs.alpinelinux.org/package/edge/main/x86_64/llvm5-dev
* Same issue, distro not recognised by script

## Arch Linux 

* Vagrant box: https://app.vagrantup.com/generic/boxes/arch
* As usual, distro not recognised by script
* Looks like there's only llvm-6, no 5!
    * tried (manually), but `clang` still wants version 5

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

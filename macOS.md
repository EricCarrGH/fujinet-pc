# macOS build environment setup

If running macOS jump directly to the [Homebrew](##Homebrew) section.

If not running macOS natively there is an option to run macOS as a virtual machine.
## macOS on Linux

### KVM

Note: macOS inside KVM is slow (especially GUI), but for small project building over SSH it works fine.

Detailed instructions can be found for example here: [How To run macOS on KVM / QEMU](https://computingforgeeks.com/how-to-run-macos-on-kvm-qemu/) 

Quick steps:

```sh
git clone https://github.com/foxlet/macOS-Simple-KVM.git
```

Check `README.md` inside `macOS-Simple-KVM` directory.

### Install OS updates

## Homebrew

Detailed instructions can be found for example here: [How To Install and Use Homebrew on macOS](https://www.digitalocean.com/community/tutorials/how-to-install-and-use-homebrew-on-macos)

Quick steps below.

### Install Xcode Command Line Tools

From terminal:

```sh
xcode-select --install
```

### Install Homebrew

Download install script:

```sh
curl -fsSL -o install.sh https://raw.githubusercontent.com/Homebrew/install/master/install.sh
```

Check the downloaded `install.sh` script. If happy with it, run it:

```sh
/bin/bash install.sh
```

## Build tools

```sh
# install CMake
brew install cmake
```

CMake with all dependencies should be installed.

## SSH

Optionally, to enable SSH navigate to System Preferences > Sharing. Enable Remote Login.

## FTDI VCP Drivers

To use SIO2USB cable or adapter based on Future Technologies chip you need to install MacOS driver for it:

https://ftdichip.com/Drivers/vcp-drivers/

VCP = virtual COM port


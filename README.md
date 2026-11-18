# caffeine

caffeine is an early Windows kernel-mode virtualization experiment. It contains a KMDF driver and a small loader; the driver currently logs startup and checks whether the CPU reports VMX support.

## Build

Open `caffeine.sln` in Visual Studio with the Windows Driver Kit installed and build the x64 configuration.

## Status

The repository is a driver scaffold, not a functioning hypervisor. It does not yet enter or manage VMX operation. The loader starts a kernel driver, so this project is intended for controlled development environments and is not a general-purpose application.

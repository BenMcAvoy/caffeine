#include <ntddk.h>

#include <intrin.h>

#include "helpers/logging.h"

extern "C" VOID DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    DriverObject->DriverUnload = DriverUnload;
    
    Caffeine::Logging::Info("Hello, world!");
    
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    const bool vmxSupported = (cpuInfo[2] & (1 << 5)) != 0;
    Caffeine::Logging::Info("vmxSupported=%d", vmxSupported);

    return STATUS_SUCCESS;
}

extern "C" VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
}
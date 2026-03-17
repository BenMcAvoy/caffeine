#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <string>
#include <string_view>
#include <algorithm>
#include <memory>

#include "logging.h"

namespace Caffeine::Driver {
    class Manager {
    public:
        explicit Manager(const std::wstring_view driverName, const std::wstring_view driverPath)
            : name_(driverName), path_(driverPath), hSCM_(nullptr), hService_(nullptr) {}

        ~Manager() {
            // close service handle first
            if (hService_) {
                CloseServiceHandle(hService_);
                hService_ = nullptr;
            }

            if (hSCM_) {
                CloseServiceHandle(hSCM_);
                hSCM_ = nullptr;
            }
        }
        
        bool RegisterDriver() {
            OpenScm();
            
            hService_ = CreateServiceW(
                hSCM_,
                name_.c_str(),
                name_.c_str(),
                SERVICE_ALL_ACCESS,
                SERVICE_KERNEL_DRIVER,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_NORMAL,
                path_.c_str(),
                nullptr, nullptr, nullptr, nullptr, nullptr
            );
            
            if (!hService_) {
                const auto err = GetLastError();
                if (err == ERROR_SERVICE_EXISTS) {
                    Logging::Info("Service '%ls' already exists. Attempting to open existing service.", name_.c_str());
                    return OpenSysService();
                }
                
                Logging::Error("Failed to create service '%ls': %u", name_.c_str(), err);
                return false;
            }
            
            Logging::Success("Service '%ls' registered successfully.", name_.c_str());
            return true;
        }
        
        bool UnregisterDriver() {
            if (!OpenSysService()) {
                return false;
            }
            
            if (!DeleteService(hService_)) {
                Logging::Error("Failed to delete service '%ls': %u", name_.c_str(), GetLastError());
                return false;
            }
            
            Logging::Success("Service '%ls' unregistered successfully.", name_.c_str());
            return true;
        }
        
        bool StartDriver() {
            if (!OpenSysService()) {
                return false;
            }

            // If already running, treat as success
            if (IsRunning()) {
                Logging::Info("Service '%ls' is already running.", name_.c_str());
                return true;
            }

            // If service is pending start/stop, wait briefly first
            WaitWhilePending(5000);

            if (!StartServiceW(hService_, 0, nullptr)) {
                const auto err = GetLastError();

                if (err == ERROR_SERVICE_ALREADY_RUNNING) {
                    Logging::Info("Service '%ls' already running.", name_.c_str());
                    return true;
                }

                if (err == ERROR_SERVICE_DISABLED) {
                    Logging::warn("Service '%ls' is disabled (error %u). Attempting to change start type to SERVICE_DEMAND_START.", name_.c_str(), err);

                    // Try to change service configuration to demand start and retry once.
                    if (!ChangeServiceConfigW(hService_, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
                        Logging::Error("Failed to change start type for '%ls': %u", name_.c_str(), GetLastError());
                        DumpServiceStatusAndConfig();
                        return false;
                    }

                    Logging::Info("Changed service '%ls' start type to DEMAND_START, retrying StartService.", name_.c_str());
                    if (!StartServiceW(hService_, 0, nullptr)) {
                        Logging::Error("Retry StartService for '%ls' failed: %u", name_.c_str(), GetLastError());
                        DumpServiceStatusAndConfig();
                        return false;
                    }
                } else {
                    Logging::Error("Failed to start service '%ls': %u", name_.c_str(), err);
                    DumpServiceStatusAndConfig();
                    return false;
                }
            }

            // Wait for the service to actually enter the running state
            if (!WaitForServiceState(SERVICE_RUNNING, 15000)) {
                Logging::Error("Service '%ls' did not reach RUNNING state within timeout.", name_.c_str());
                DumpServiceStatusAndConfig();
                return false;
            }

            Logging::Success("Service '%ls' started successfully.", name_.c_str());
            return true;
        }

        bool StopDriver() {
            if (!OpenSysService()) {
                return false;
            }

            SERVICE_STATUS_PROCESS ssp{};
            if (!GetServiceStatusProcess(ssp)) {
                return false;
            }

            if (ssp.dwCurrentState == SERVICE_STOPPED) {
                Logging::Info("Service '%ls' is already stopped.", name_.c_str());
                return true;
            }

            // If pending, wait until pending clears to avoid 1052
            if (ssp.dwCurrentState == SERVICE_START_PENDING || ssp.dwCurrentState == SERVICE_STOP_PENDING) {
                Logging::Info("Service '%ls' is in a pending state, waiting briefly before attempting stop.", name_.c_str());
                if (!WaitWhilePending(5000)) {
                    Logging::warn("Service '%ls' remained pending; proceeding to attempt stop anyway.", name_.c_str());
                }
            }

            // New: check whether the service actually accepts STOP control before calling ControlService
            SERVICE_STATUS statusQuery{};
            if (!QueryServiceStatus(hService_, &statusQuery)) {
                Logging::Error("Failed to query service controls for '%ls': %u", name_.c_str(), GetLastError());
                // fall through and attempt ControlService anyway as a best-effort
            } else {
                // If the service does not accept STOP control, avoid calling ControlService which will return 1052.
                if ((statusQuery.dwControlsAccepted & SERVICE_ACCEPT_STOP) == 0) {
                    Logging::warn("Service '%ls' does not accept SERVICE_ACCEPT_STOP (dwControlsAccepted=%u). It may be a driver without unload support. Treating stop as successful no-op.", name_.c_str(), statusQuery.dwControlsAccepted);
                    DumpServiceStatusAndConfig();
                    return true;
                }
            }

            SERVICE_STATUS status{};
            const int maxRetries = 5;
            for (int attempt = 0; attempt < maxRetries; ++attempt) {
                if (ControlService(hService_, SERVICE_CONTROL_STOP, &status)) {
                    break; // requested stop successfully
                }

                const auto err = GetLastError();
                if (err == ERROR_SERVICE_NOT_ACTIVE) {
                    Logging::Info("Service '%ls' is not active (treated as stopped).", name_.c_str());
                    return true;
                }

                if (err == ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
                    Logging::warn("Service '%ls' cannot accept control right now (attempt %d/%d). Waiting then retrying.", name_.c_str(), attempt + 1, maxRetries);
                    // wait for pending to clear a bit then retry
                    if (!WaitWhilePending(2000)) {
                        Sleep(200);
                    }
                    continue; // retry
                }

                Logging::Error("Failed to stop service '%ls' (attempt %d): %u", name_.c_str(), attempt + 1, err);
                DumpServiceStatusAndConfig();
                // small backoff before next attempt
                Sleep(200);
            }

            // Wait for service to reach stopped state
            if (!WaitForServiceState(SERVICE_STOPPED, 15000)) {
                Logging::Error("Service '%ls' did not reach STOPPED state within timeout.", name_.c_str());
                DumpServiceStatusAndConfig();
                return false;
            }

            Logging::Success("Service '%ls' stopped successfully.", name_.c_str());
            return true;
        }
        
        bool IsRunning() {
            if (!OpenSysService()) {
                return false;
            }
            
            SERVICE_STATUS status;
            if (!QueryServiceStatus(hService_, &status)) {
                Logging::Error("Failed to query service '%ls' status: %u", name_.c_str(), GetLastError());
                return false;
            }
            
            return status.dwCurrentState == SERVICE_RUNNING;
        }
        
        bool IsRegistered() {
            if (!OpenScm()) {
                return false;
            }
            
            SC_HANDLE hTempService = OpenServiceW(hSCM_, name_.c_str(), SERVICE_QUERY_STATUS);
            if (hTempService) {
                CloseServiceHandle(hTempService);
                return true;
            }
            
            if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
                Logging::Error("Failed to check if service '%ls' is registered: %u", name_.c_str(), GetLastError());
            }
            
            return false;
        }
        
    private:
        std::wstring name_;
        std::wstring path_;
        
        SC_HANDLE hSCM_;
        SC_HANDLE hService_;

        // Query service status process helper
        bool GetServiceStatusProcess(SERVICE_STATUS_PROCESS &outSsp) {
            DWORD bytesNeeded = 0;
            if (!QueryServiceStatusEx(hService_, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&outSsp), sizeof(outSsp), &bytesNeeded)) {
                Logging::Error("QueryServiceStatusEx failed for '%ls': %u", name_.c_str(), GetLastError());
                return false;
            }
            return true;
        }

        // Diagnostic helper: log current service state and configured start type (if available)
        void DumpServiceStatusAndConfig() {
            if (!hService_) {
                Logging::Error("DumpServiceStatusAndConfig: service handle not open for '%ls'", name_.c_str());
                return;
            }
 
             SERVICE_STATUS_PROCESS ssp{};
             if (GetServiceStatusProcess(ssp)) {
                 Logging::Debug("Service '%ls' state: %u (chkpt %u, waitHint %u)", name_.c_str(), ssp.dwCurrentState, ssp.dwCheckPoint, ssp.dwWaitHint);
             }
 
             // QueryServiceConfig to read start type
             DWORD bytesNeeded = 0;
             QueryServiceConfigW(hService_, nullptr, 0, &bytesNeeded);
             if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytesNeeded > 0) {
                 std::unique_ptr<BYTE[]> buffer(new BYTE[bytesNeeded]);
                 LPQUERY_SERVICE_CONFIGW qsc = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.get());
                 if (QueryServiceConfigW(hService_, qsc, bytesNeeded, &bytesNeeded)) {
                     DWORD startType = qsc->dwStartType;
                     Logging::Debug("Service '%ls' configured start type: %u", name_.c_str(), startType);
                 } else {
                     Logging::Debug("QueryServiceConfigW failed for '%ls': %u", name_.c_str(), GetLastError());
                 }
             } else {
                 Logging::Debug("QueryServiceConfigW: no config or insufficient buffer for '%ls' (err %u)", name_.c_str(), GetLastError());
             }
         }
 
         // Wait for the service to reach the desired state within timeoutMs (milliseconds)
         bool WaitForServiceState(DWORD desiredState, DWORD timeoutMs) {
             if (!hService_) {
                 Logging::Error("WaitForServiceState: service handle not open for '%ls'", name_.c_str());
                 return false;
             }

             const ULONGLONG start = GetTickCount64();
             SERVICE_STATUS_PROCESS ssp{};

             while (true) {
                 if (!GetServiceStatusProcess(ssp)) {
                     return false;
                 }

                 if (ssp.dwCurrentState == desiredState) {
                     return true;
                 }

                 // If service is in a pending state, wait a bit based on dwWaitHint
                 if (ssp.dwCurrentState == SERVICE_START_PENDING || ssp.dwCurrentState == SERVICE_STOP_PENDING) {
                     DWORD wait = 100;
                     if (ssp.dwWaitHint > 0) {
                         wait = std::min<DWORD>(ssp.dwWaitHint / 10, 1000);
                     }
                     Sleep(wait);
                 } else {
                     // Not in a pending state and not desired state -> cannot reach desired state
                     // Keep trying until timeout, but avoid tight loop
                     Sleep(200);
                 }

                 if ((GetTickCount64() - start) > timeoutMs) {
                     return false;
                 }
             }

             // unreachable
             return false;
         }

         // Wait while the service reports a pending state (start/stop pending). Returns true when the service
         // is not in a pending state anymore or when timeout expires.
         bool WaitWhilePending(DWORD timeoutMs) {
             if (!hService_) {
                 Logging::Error("WaitWhilePending: service handle not open for '%ls'", name_.c_str());
                 return false;
             }
             
             const ULONGLONG start = GetTickCount64();
             SERVICE_STATUS_PROCESS ssp{};
             
             while (true) {
                 if (!GetServiceStatusProcess(ssp)) {
                     return false;
                 }
                 
                 if (ssp.dwCurrentState != SERVICE_START_PENDING && ssp.dwCurrentState != SERVICE_STOP_PENDING) {
                     return true; // no longer pending
                 }
                 
                 DWORD wait = 100;
                 if (ssp.dwWaitHint > 0) {
                     wait = std::min<DWORD>(ssp.dwWaitHint / 10, 1000);
                 }
                 Sleep(wait);
                 
                 if ((GetTickCount64() - start) > timeoutMs) {
                     return false;
                 }
             }
             
             return false;
         }
        
        bool OpenScm() {
            if (!hSCM_) {
                hSCM_ = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
                
                if (!hSCM_) {
                    Logging::Error("Failed to open Service Control Manager: %u", GetLastError());
                    return false;
                }
            }
            
            return true;
        }
        
        bool OpenSysService() {
            if (!OpenScm()) {
                return false;
            }
            
            if (!hService_) {
                hService_ = OpenServiceW(hSCM_, name_.c_str(), SERVICE_ALL_ACCESS);
                
                if (!hService_) {
                    Logging::Error("Failed to open service '%ls': %u", name_.c_str(), GetLastError());
                    return false;
                }
            }
            
            return true;
        }
    };
}


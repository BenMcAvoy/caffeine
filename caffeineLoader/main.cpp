#include "helpers/driver.h"
#include "helpers/elevation.h"
#include "helpers/logging.h"

int main() {
    Caffeine::EnsureElevation();
    Caffeine::Logging::Info("Application started with elevated privileges.");
    
    Caffeine::Driver::Manager drv(L"caffeineHV", SYS_PATH);

    if (!drv.IsRegistered()) {
        drv.RegisterDriver();
        Caffeine::Logging::Info("Driver caffeineHV registered.");
    }
    
    if (drv.IsRunning()) {
        drv.StopDriver();
    }
    
    drv.StartDriver();
    
    Caffeine::Logging::WaitForInteract("RETURN to exit");
}
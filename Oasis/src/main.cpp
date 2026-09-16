#include <iostream>
#include "Oryx/core/log.h"
#include "Oryx/version.h"

int main() {
    oryx::Log::Init();

    std::cout << "Oasis — built on Oryx v"
              << oryx::VERSION_MAJOR << "."
              << oryx::VERSION_MINOR << "."
              << oryx::VERSION_PATCH << std::endl;

    ORYX_INFO("Oasis started");

    return 0;
}

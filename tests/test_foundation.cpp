#include <iostream>
#include "Oryx/version.h"

int main() {
    std::cout << "Oryx v" 
              << oryx::VERSION_MAJOR << "."
              << oryx::VERSION_MINOR << "."
              << oryx::VERSION_PATCH << std::endl;
    
    std::cout << "✓ Oryx foundation is working!" << std::endl;
    
    return 0;
}
#include <iostream>
#include "gateway-master.hpp"


int main(int argc, char *argv[], char *envp[])
{
    std::string config_file = std::string(argv[1]);

    MasterServer ms(config_file);
    ms.start();

    return 0;
}
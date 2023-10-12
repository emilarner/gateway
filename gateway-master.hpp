#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <fstream>

#include <cstring>

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "gateway-relay.hpp"
#include "config.hpp"
#include "protocol.hpp"

class MasterServer
{
private:
    std::string configuration_file_path;
    std::vector<Relay> relays;

    int master_sock_fd;
    IPDatabase ip_database;

public:
    MasterServer(std::string config_path);
    
    /* Set an expiration that is the maximum expiration time for all clients through the Gateway Launcher interface. */
    /* This does not include clients who are hardcoded into the whitelist via the gateway-config file. */
    void set_forced_expiration(time_t seconds);

    /* Load the mappings between inbound and outbound services (relays) into memory. Also read hardcoded whitelisted IPs. */
    void parse_config();

    /* Accept information via UNIX socket about dynamic IP whitelisting and other management features.*/
    void handle();

    /* Start the relays and the UNIX socket server.*/
    void start();
};  

#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <functional>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>

#include "config.hpp"
#include "ip-database.hpp"

class Relay
{
private:
    uint16_t outbound_port;
    uint16_t inbound_port;
    in_addr_t inbound_addr;

    IPDatabase *ip_database;

    int outbound_fd;
public:
    Relay(uint16_t op, in_addr_t iaddr, uint16_t ip, IPDatabase *id);

    /* Glue the client from the outbound port to a new connection to the inbound service port, using select. */
    void glue(int cfd); 

    /* Accept connections from the outbound port, so we can glue them to the inbound port, if they're authorized. */
    void handle();
};
#pragma once

#include <cstdint>
#include <sys/types.h>

enum class Commands
{
    Authenticate
};

struct __attribute__((__packed__)) Command 
{
    enum Commands command : 8;
};

struct __attribute__((__packed__)) AuthenticateCommand
{
    in_addr_t ip;
    uint32_t expiration_seconds;
};

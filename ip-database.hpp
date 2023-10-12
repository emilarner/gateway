#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <thread>
#include <mutex>
#include <optional>
#include <algorithm>
#include <fstream>
#include <utility>

#include <ctime>

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>

#include "config.hpp"

struct __attribute__((__packed__)) ip_file_header {
    in_addr_t ip;
    time_t duration; // -1 if non-existent
};

class IPDatabase
{
private:
    /* a map provides faster searching, insertion, and removal than an array. */
    /* ... theoretically? */
    std::map<in_addr_t, std::optional<time_t>> ips;
    std::vector<std::pair<in_addr_t, std::optional<time_t>>> new_ips;

    std::mutex ip_mutex;
    const std::string database_path;
    time_t forced;

public:
    IPDatabase(std::string database_location);

    void set_forced_expiration(time_t seconds);

    /* Read stored IP database information from disk. */
    void read_disk_database();

    /* Write newly added IP database information to disk. */
    void write_disk_database();

    /* Add an IP address--even if it already exists--to the database of currently allowed IP addresses. */
    /* If it already exists, it won't be added to the database on disk again. */
    void add_ip(in_addr_t ip, std::optional<time_t> expiration);

    /* Check if an IPv4 address is already in the database.*/
    bool check_ip(in_addr_t ip);
};
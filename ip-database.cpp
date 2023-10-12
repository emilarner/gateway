#include "ip-database.hpp"


IPDatabase::IPDatabase(std::string database_location) : database_path(database_location)
{
    read_disk_database();
    forced = 0;
}

void IPDatabase::set_forced_expiration(time_t seconds)
{
    forced = seconds;
}

void IPDatabase::read_disk_database()
{
    std::ifstream disk(database_path, std::ios::binary);
    while (!disk.eof())
    {
        struct ip_file_header ip;
        disk.read((char*)&ip, sizeof(ip));

        /* it's expired, so don't even think about adding it. */
        if (ip.duration <= std::time(nullptr))
            continue;

        if (ip.duration == -1)
            ips[ip.ip] = std::nullopt;
        else
            ips[ip.ip] = (time_t)ip.duration;
    }

    disk.close();
}

void IPDatabase::write_disk_database()
{
    std::ofstream disk(database_path, std::ios::app | std::ios::binary);

    for (auto &[ip, expiration] : new_ips)
    {
        struct ip_file_header ip_hdr;
        ip_hdr.ip = ip;
        ip_hdr.duration = !expiration ? -1 : expiration.value();

        disk.write((char*)&ip_hdr, sizeof(ip_hdr));
    }

    disk.close();
    new_ips.clear();
}

void IPDatabase::add_ip(in_addr_t ip, std::optional<time_t> expiration)
{
    ip_mutex.lock();
    
    ips[ip] = forced != 0 ? forced : expiration;
    new_ips.push_back({ip, forced != 0 ? forced : expiration});
    write_disk_database();

    ip_mutex.unlock();
}


bool IPDatabase::check_ip(in_addr_t ip)
{
    auto it = ips.find(ip);

    if (it == ips.end())
        return false;

    if (it->second == std::nullopt)
        return true;

    if (it->second <= std::time(nullptr))
    {
        /* handle expired IP. */
        return false;
    }

    return true;
}
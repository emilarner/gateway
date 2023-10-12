#include "gateway-master.hpp"

MasterServer::MasterServer(std::string config_path) : ip_database(DEFAULT_IP_DATABASE_PATH)
{
    configuration_file_path = config_path;
    master_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    parse_config();
}

void MasterServer::set_forced_expiration(time_t seconds)
{
    ip_database.set_forced_expiration(seconds);
}

void MasterServer::parse_config()
{
    std::ifstream configfp(configuration_file_path);

    if (!configfp)
    {
        std::cerr << "The configuration file at " << configuration_file_path << " does not exist." << std::endl;
        abort();
    }

    /* C++ has confusing documentation regarding how to check if the stream has reached end of file. */
    /* the eof() method is the most explicit, but a stream's boolean casting operator is said to work the same way, */
    /* but it doesn't. */
    while (!configfp.eof())
    {
        /* note: this code is thrown together and uses (practically unsafe) C-string handling functions. */

        std::string line;
        std::getline(configfp, line);
        
        /* ignore comments and lines beginning with a newline/space. */
        /* bug: this code doesn't correctly handle empty lines, for whatever reason that may be, I have no clue.*/
        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ' || line[0] == '\t' || line[0] == '\r')
            continue;

        /* handle expiration clauses. */
        if (line.size() > (sizeof("expiration") - 1))
        {
            if (!std::memcmp(line.c_str(), "expiration", sizeof("expiration") - 1))
            {
                const char *after = std::strchr(line.c_str(), ' ');
                if (after == nullptr)
                {
                    std::cerr << "The duration after the 'expiration' keyword is not present." << std::endl;
                    abort();
                }

                try 
                {
                    int forced_duration = std::stoi(std::string(after + 1));
                    ip_database.set_forced_expiration(forced_duration);
                }
                catch (std::exception &ex)
                {
                    std::cerr << "The duration for the 'expiration' keyword is invalid: " << ex.what() << std::endl;
                    abort();
                }

                continue;
            }
        }

        /* it's an IP address to always allow, regardless of an expiration. */
        /* this is because the IP address is always added directly, instead of from the IP store. */
        if (std::strchr(line.c_str(), ' ') == nullptr)
        {
            in_addr_t ip_addr = inet_addr(line.c_str());
            if (ip_addr == -1)
            {
                std::cerr << line << std::endl;
                std::cerr << "Error parsing IP address " << line.c_str() << std::endl;
                abort();
            }

            ip_database.add_ip(ip_addr, std::nullopt);
            continue;
        }

        /* it's a map between outbound and inbound. */
        char outbound[6];
        std::memset(outbound, 0, sizeof(outbound));

        /* check the length before we copy it over. */
        if (std::strchr(line.c_str(), ' ') - line.c_str() > 5)
        {
            std::cerr << "The outbound port's length is way too big. Don't try to cause a SEGFAULT!!!" << std::endl;
            abort();
        }

        std::memcpy(outbound, line.c_str(), std::strchr(line.c_str(), ' ') - line.c_str());

        char inbound_addr[16];
        std::memset(inbound_addr, 0, sizeof(inbound_addr));
        
        const char *inbound_info = std::strchr(line.c_str(), ' ') + 1;
        const char *inbound_port = std::strchr(inbound_info, ':') + 1;

        /* ipv4 in format xxx.xxx.xxx.xxx = 15 characters, 16 including null terminator */
        
        if (inbound_port - 1 - inbound_info > sizeof(inbound_addr))
        {
            std::cout << "The address is too big to fit within the buffer." << std::endl;
            abort();
        }


        std::memcpy(inbound_addr, inbound_info, inbound_port - 1 - inbound_info);


        std::string outbound_cpp(outbound);
        std::string inbound_port_cpp(inbound_port);

        try 
        {
            in_addr_t resolution = inet_addr(inbound_addr);
            if (resolution < 0)
                throw std::out_of_range("j");

            relays.push_back(Relay(std::stoi(outbound_cpp), resolution, std::stoi(inbound_port), &ip_database));
            std::cout << "Successfully created relay mapping outbound " << outbound_cpp << " to inbound " 
                    << inbound_addr << ":" << inbound_port_cpp << std::endl;
        }
        catch (std::exception &ex)
        {
            std::cerr << "Error processing config file: " << ex.what() << std::endl;
            std::cerr << "Are you sure you wrote it in the right format?" << std::endl;
            abort();
        }
    }

    configfp.close();
    std::cout << "Configuration file successfully read." << std::endl;
}

void MasterServer::handle()
{
    std::cout << "Handling master socket messages." << std::endl;

    while (true)
    {
        struct sockaddr_in client;
        socklen_t length = sizeof(client);

        int cfd = accept(master_sock_fd, (struct sockaddr*)&client, &length);

        while (true)
        {
            struct Command cmd;
            if (recv(cfd, &cmd, sizeof(struct Command), MSG_WAITALL) <= 0)
            {
                std::cerr << "Master server socket connection died, allowing for another connection." << std::endl;
                break;
            }

            switch (cmd.command)
            {
                case Commands::Authenticate:
                {
                    struct AuthenticateCommand ac;
                    recv(cfd, &ac, sizeof(ac), MSG_WAITALL);

                    ip_database.add_ip(ac.ip, (time_t)ac.expiration_seconds);

                    break;
                }
            }
        }
    }
}

void MasterServer::start()
{
    struct sockaddr_in saddr;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(60102); // 60102 is the hardcoded Gateway master server port. 
    saddr.sin_addr.s_addr = INADDR_ANY;

    if ((bind(master_sock_fd, (struct sockaddr*) &saddr, sizeof(saddr))) < 0)
    {
        std::cerr << "Error binding master socket: " << std::strerror(errno) << std::endl;
        abort();
    }

    /* let the server reclaim zombie ports */
    int val = 1;
    setsockopt(master_sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    if (listen(master_sock_fd, 64) < 0)
    {
        std::cerr << "Error listening master socket: " << std::strerror(errno) << std::endl;
        abort();
    }

    /* start each relay server. */
    for (Relay &r : relays)
    {
        std::thread tmp(&Relay::handle, &r);
        tmp.detach();
    }

    handle();
}
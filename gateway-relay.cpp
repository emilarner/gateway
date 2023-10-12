#include "gateway-relay.hpp"

Relay::Relay(uint16_t op, in_addr_t iaddr, uint16_t ip, IPDatabase *id) 
    : outbound_port(op), inbound_port(ip), ip_database(id), inbound_addr(iaddr)
{

}

void Relay::glue(int cfd)
{
    struct timeval tv;
    tv.tv_sec = 60 * 60;
    tv.tv_usec = 0;

    /* we must create a socket to the designated inbound port for this relay. */
    int ifd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in iaddr;
    iaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    iaddr.sin_port = htons(inbound_port);
    iaddr.sin_family = AF_INET;

    if (connect(ifd, (struct sockaddr*)&iaddr, sizeof(iaddr)) < 0)
    {
        std::cerr << "Connection to inbound service failed, for outbound port " << outbound_port << std::endl;
        return;
    }

    while (true)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(cfd, &fds);
        FD_SET(ifd, &fds);


        /* select blocks until one of the file descriptors has something for us to read. */
        int retval = select(std::max(ifd, cfd) + 1, &fds, nullptr, nullptr, &tv);

        char buffer[DEFAULT_BUFFER_SIZE];

        /* if an fd has something to read, send it to the other fd, and vice versa. */

        if (FD_ISSET(cfd, &fds))
        {
            ssize_t len = recv(cfd, buffer, sizeof(buffer), MSG_DONTWAIT);

            if (len <= 0)
            {
                close(ifd);
                return;
            }

            send(ifd, buffer, len, MSG_WAITALL);
        }

        if (FD_ISSET(ifd, &fds))
        {
            ssize_t len = recv(ifd, buffer, sizeof(buffer), MSG_DONTWAIT);

            if (len <= 0)
            {
                close(cfd);
                return;
            }

            send(cfd, buffer, len, MSG_WAITALL);
        }
    }
}

void Relay::handle()
{
    /* set up the server for the outbound port. */
    outbound_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    std::cout << "Relay handler for outbound port " << outbound_port << " started." << std::endl;


    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(outbound_port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    /* allows the program to bind over existing ports or whatever, it just werkz. */
    int val = 1;
    setsockopt(outbound_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));

    if (bind(outbound_fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0)
    {
        std::cerr << "Failed to bind relay server for outbound port " << outbound_port << std::endl;
        abort();
    }

    listen(outbound_fd, 64);

    while (true)
    {
        struct sockaddr_in client;
        socklen_t client_length = sizeof(client);
        int cfd = 0;

        if ((cfd = accept(outbound_fd, (struct sockaddr*)&client, &client_length)) < 0)
        {
            std::cerr << "Error accepting in relay for outbound port " << outbound_port << std::endl;
            continue;
        }

        /* is the IP address within the database? If not, close the connection. */
        if (!ip_database->check_ip(client.sin_addr.s_addr))
        {
            std::cout << "Failed authentication for " << inet_ntoa(client.sin_addr) << std::endl;

            close(cfd);
            continue;
        }

        /* start the isolated thread where we 'glue' the connections together henceforth. */
        std::thread tmp(&Relay::glue, this, cfd);
        tmp.detach();
    }
}
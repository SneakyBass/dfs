#include <arpa/inet.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int (*connect_t)(int, const struct sockaddr *, socklen_t);

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    struct Handshake
    {
        uint8_t Address[16];
        socklen_t Addrlen;
        in_port_t Port;
    };

    connect_t original_connect = (connect_t)dlsym(RTLD_NEXT, "connect");

    if (addr->sa_family == AF_INET6) {
        // It looks like Dofus uses IPV6 address space with the first bytes set to 0.
        // The second half is actually an IPv4
        struct sockaddr_in6 *target = (struct sockaddr_in6 *)addr;

        if (target->sin6_port == htons(5555) && (target->sin6_addr.s6_addr[0] == 0)
            && (target->sin6_addr.s6_addr[1] == 0) && (target->sin6_addr.s6_addr[2] == 0)
            && (target->sin6_addr.s6_addr[3] == 0) && (target->sin6_addr.s6_addr[4] == 0)
            && (target->sin6_addr.s6_addr[5] == 0) && (target->sin6_addr.s6_addr[6] == 0)
            && (target->sin6_addr.s6_addr[7] == 0) && (target->sin6_addr.s6_addr[8] == 0)
            && (target->sin6_addr.s6_addr[9] == 0) && (target->sin6_addr.s6_addr[10] == 0xFF)
            && (target->sin6_addr.s6_addr[11] == 0xFF)) {

            struct Handshake h;
            memcpy(&h.Address, target->sin6_addr.s6_addr, 16);
            h.Addrlen = addrlen;
            h.Port = target->sin6_port;

            // Reset the IP
            memset(&target->sin6_addr, 0, sizeof(target->sin6_addr));
            target->sin6_addr.s6_addr[10] = 0xFF;
            target->sin6_addr.s6_addr[11] = 0xFF;

            in_addr_t localhost = inet_addr("127.0.0.1");
            memcpy(&target->sin6_addr.s6_addr[12], &localhost, 4);

            // Extract the IPv4 address (last 4 bytes of the IPv6 address)
            struct sockaddr_in sin4;
            memset(&sin4, 0, sizeof(sin4));
            sin4.sin_family = AF_INET;

            // Convert the last 4 bytes of the IPv6 address to an IPv4 address
            memcpy(&sin4.sin_addr.s_addr, &target->sin6_addr.s6_addr[12], 4);

            // Copy the port from the original IPv6 address
            sin4.sin_port = target->sin6_port;

            // Log the modified target address (IPv4)
            char ip4[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sin4.sin_addr), ip4, INET_ADDRSTRLEN);

            // Call the original connect function with the new IPv4 address
            int result = original_connect(sockfd, (struct sockaddr *)target, addrlen);

            // Send the handshake with the destination server information
            write(sockfd, &h, sizeof(struct Handshake));

            return result;
        }
    } else if (addr->sa_family == AF_FILE) {
        // Ignore
    } else {
        printf("Connect called to family: %d", addr->sa_family);
    }

    return original_connect(sockfd, addr, addrlen);
}

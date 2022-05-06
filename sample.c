#include "nl_utilities.h"
#include "mcpcap.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/if_ether.h>     /* ethhdr */


/*
*   $ gcc nl_utilities.c mcpcap.c sample.c -o sample $(pkg-config --cflags --libs libnl-genl-3.0)
*
*   Example test:
*   $ sudo ./sample wlan1 1000
*/

int main(int argc, char** argv) {

    nl_handle nl;
    sk_handle skh;
    packet_buffer pb;
    struct if_info info;
    info.if_name = argv[1];
    int ITER = strtol(argv[2], NULL , 10);
    int if_index = get_if_index(info.if_name);

    nl_init(&nl);
    if (!nl.sk) {
        return -1;
    }

    if (get_if_info(&nl, &info, if_index) < 0) {
        fprintf(stderr, "Could not retrieve interface information.\n");
        return -1;
    }
    printf("Current device interfae data:\n");
    printf("IFNAME: %s\n", info.if_name);
    printf("IFINDEX: %d\n", info.if_index);
    printf("WDEV: %d\n", info.wdev);
    printf("WIPHY: %d\n", info.wiphy);
    printf("IFTYPE: %d\n", info.if_type);
    printf("\n");

    const char *new_iftype = "monitor";
    const char *new_ifname = "mcmesh0";
    int new_if_index;
    if (compare_if_type(info.if_type, new_iftype) == 0) {
        printf("Device not currently in monitor mode.\n");
        //set_if_type(&nl, new_iftype, if_index, if_name);
        //printf("Put device in monitor mode.\n");
        //delete_if(&nl, if_index);
        create_new_if(&nl, new_iftype, info.wiphy, new_ifname);
        new_if_index = get_if_index(new_ifname);
        printf("New ifindex:%d\n", new_if_index);
        printf("Created new monitor mode interface.\n");
    } else {
        printf("Device is already in monitor mode.\n");
    }
    printf("\n");

    create_pack_socket(&skh);
    if (skh.sockfd < 0) {
        return -1;
    }
    
    if (bind_pack_socket(&skh, new_if_index) < 0) {
        return -1;
    }

    allocate_packet_buffer(&pb);

    //Just for testing, remove asap.
    for (int n=0; n<ITER; n++) {
        int n = recvpacket(&skh, &pb);
        printf("Received %d bytes.\n", n);
        struct ethhdr *eth = (struct ethhdr *)(pb.buffer);
        printf("\nReceived Ethernet Header:\n");
        printf("Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
        printf("Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
        printf("Protocol : %d\n",eth->h_proto);
        printf("\n");
    }
    
    printf("Restoring device settings...\n");
    const char *ret_iftype = "managed";
    //set_if_type(&nl, ret_iftype, if_index, if_name);
    //delete_if(&nl, new_if_index);
    //create_new_if(&nl, info.if_type, info.wiphy, info.if_name);
    nl_cleanup(&nl);

    return 0;
}

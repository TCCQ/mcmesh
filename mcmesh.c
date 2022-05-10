#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nl_utilities.h"
#include "handle80211.h"
#include "wpcap.h"

void print_interface(struct if_info *info) {
    printf("Interface: %s\n", info->if_name);
    printf("\tIFINDEX: %d\n", info->if_index);
    printf("\tWDEV: %d\n", info->wdev);
    printf("\tWIPHY: %d\n", info->wiphy);
    printf("\tIFTYPE: %d\n", info->if_type);
    printf("\tIFFREQ: %d\n", info->if_freq);
}

// void init_packet_socket(sk_handle *skh, struct if_info *info) {
//     create_pack_socket(skh);
//     if (!skh->sockfd) {
//         fprintf(stderr, "Could not allocate new packet socket. Are you running this as root/sudo?\n");
//     }
//     if (bind_pack_socket(skh, info->if_index) < 0) {
//         fprintf(stderr, "Could not bind packet socket to %s", info->if_name);
//     }
//     //Optional...?
//     if (set_if_promisc(skh, info->if_index) < 0) {
//         printf("Could not set %s to promiscuous mode.\n", info->if_name);
//     }
// }

// void recv_socket(sk_handle *skh, int ITER) {
//     int fail = 0;
//     int count = 0;
//     skh->buffer = (char *)malloc(65336);     //Going big! Eth MTU is probably fine.
//     memset(skh->buffer, 0, 65536);
//     printf("Waiting to receive packets...\n");
//     while (count < ITER) {
//         count++;
//         if (read_socket(skh) < 0) {
//             fail++;
//             printf("Failed to read %d/%d packets.\n", fail, count);
//         } else {
//             handle_buffer(skh);
//         }
//     }
// }

    /*
    *  Initialize netlink socket.
    */
int start_netlink(nl_handle *nl) {
    nl_init(nl);
    if (!nl->sk) {
        fprintf(stderr, "Failed to initialize netlink socket.\n");
        return -1;
    }
    return 0;
}

    /*
    *  STEP 2: Store existing interface config.
    */
int find_interface(nl_handle *nl, struct if_info *keep_if) {
    if (get_interface_config(nl, keep_if, keep_if->if_index) < 0) {
        fprintf(stderr, "Could not collect configuration for %s.\n", keep_if->if_name);
        return -1;
    }
    //print_interface(&keep_if);
    return 0;
}


    /*
    *  Create new / set existing interface typer monitor mode (+ flags).
    *  Contingent: Delete existing interface from phy.
    */
int get_monitor_interface(nl_handle *nl, struct if_info *new_if, struct if_info *keep_if) {
    printf("Creating new monitor mode interface on phy%d...\n", keep_if->wiphy);
    // strcpy(new_if.if_name, keep_if.if_name);
    // strcat(new_if.if_name, "mon0");     //Need to find the next highest number for phy. Not just 0.
    new_if->if_name = "mcmon";
    if (create_new_interface(nl, new_if->if_name, NL80211_IFTYPE_MONITOR, keep_if->wiphy) < 0) {
        printf("Could not create new monitor interface...\n");
        printf("Attempting to reset existing interface type...\n");
        set_if_down(keep_if->if_name);
        if (set_interface_type(nl, NL80211_IFTYPE_MONITOR, keep_if->if_index) < 0) {
            fprintf(stderr, "Could not set existing interface to monitor mode...\n");
            return -1;
        }
        set_if_up(keep_if->if_name);
        get_interface_config(nl, new_if, keep_if->if_index);   //Overwrite "new" iface info with re-typed iface.i
    } else {
        set_if_up(new_if->if_name);
        new_if->if_index = get_if_index(new_if->if_name);
        printf("Deleting %s...\n", keep_if->if_name);
        if (delete_interface(nl, keep_if->if_index) < 0) {
            fprintf(stderr, "Could not delete %s...proceeding.\n", keep_if->if_name);
        }
    }
    //set_interface_channel(&nl, new_if.if_index, CHANNEL_149);
    get_interface_config(nl, new_if, new_if->if_index);
    printf("New monitor mode interface configuration...\n");
    print_interface(new_if);
    return 0;
}

int main(int argc, char **argv) {
    nl_handle nl;
    //sk_handle skh;
    wifi_pcap_t wpt;
    struct if_info keep_if = {0};
    struct if_info new_if = {0};
    struct bpf_program fp;
    char filter_expr[] = "";//type mgt subtype beacon";

    /*
     *  STEP 0: TODO: Consider adding channel parameter.
     */
    int monitor, ITER;
    switch (argc)
    {
    /* Capture wifi traffic on local network on specified interface. */
    case 2:
        monitor = 0;
        keep_if.if_name = argv[1];
        break;
    
    /* Capture wifi traffic in monitor mode on specified interface. */
    case 3:
        monitor = 1;
        keep_if.if_name = argv[1];
        break;

    /* Limit number of packets captured to ITER */
    case 4:
        monitor = 1;
        keep_if.if_name = argv[1];
        ITER = strtol(argv[3], NULL, 10);
        break;

    /* Need more arguments. */
    default:
        printf("Requires at least one argument: INTERFACE NAME.\n");
        printf("\nsudo ./mcmesh {IFACE} {MONITOR} {No. PACKETS}\n");
        return -1;
        break;
    }
    keep_if.if_index = get_if_index(keep_if.if_name);

    if (monitor) {
        start_netlink(&nl);
        find_interface(&nl, &keep_if);
        get_monitor_interface(&nl, &new_if, &keep_if);
    } else {
        new_if.if_name = keep_if.if_name;
        new_if.if_index = keep_if.if_index;
        new_if.if_type = keep_if.if_type;
        new_if.wdev = keep_if.wdev;
        new_if.wiphy = keep_if.wiphy;
        new_if.if_freq = keep_if.if_freq;
    }

    /*
     * Create & configure packet socket.
     */
    //init_packet_socket(&skh, &new_if);
    int cont = 1;
    if (init_wpcap(&wpt, new_if.if_name, &fp, filter_expr, monitor) < 0) {
        cont = 0;
        printf("Unable to open pcap session on %s.\n", new_if.if_name);
    }

    /* 
     *  Receive and parse data from the socket.
     */
    //recv_socket(&skh, ITER);
    if (cont) {
        view_packets(&wpt, ITER, monitor);
    }

    if (monitor) {
        /*
        *  Recreate network interface setup and free resources used.
        */
        //printf("Recreating prior existing interface...\n");
        if (create_new_interface(&nl, keep_if.if_name, NL80211_IFTYPE_STATION, keep_if.wiphy) < 0) {
            fprintf(stderr, "Could not recreate prior existing network interface.\n");
        }
        printf("Deleting monitor mode interface...\n");
        if (delete_interface(&nl, new_if.if_index) < 0) {
            fprintf(stderr, "Could not delete %s...\n", new_if.if_name);
        }
        set_if_up(keep_if.if_name);
        nl_cleanup(&nl);
    }
    
    //cleanup_mcpap(&skh);
    cleanup_wpcap(&wpt, &fp);

    return 0;
}
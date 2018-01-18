
/*------------------------------------------------------------------------
 *  ttcp.c
 *
 * Feb 8, 2012
 *
 *  Copyright (c) 2011 Ericsson AB.
 *  All rights reserved.
 *
 * Building Block Functions for ttcp Feature
 *------------------------------------------------------------------------
 */

#include "ttcp.h"
#include <error.h>
#define WINDOW_WIDTH 10
/* Prototype declaration of the functions defined in this file */
int run_ttcp_bldg_block(FEAT_CMD_MSG *);
void *ttcp_transmitter_thread(void *);
void *ttcp_reciever_thread(void *);
void pattern( char *cp, int cnt );
void ignore_sigpipe (int);
void send_traffic(char *, int *, struct sockaddr_in* , uint64_t* , FEAT_CMD_MSG*, int);
int  timespec_subtract (struct timespec *, struct timespec *, struct timespec *);
/* Global variables definitions, if any */
static int tc_id = 0, sno = 0;

/* Prototype declaration of functions which are referred here, but defined in
 * other files */
extern void add_entry_wkr_list(WKR_TC_LIST);
extern void del_entry_wkr_list(int, int);
extern int get_run_flag(int, int, int);
extern double get_cur_time(void);
extern void send_log(char *, ...);
extern void print_to_console(char *, ...);
extern void print_with_timestamp(char *, ...);
extern int checkbitpos(uint32_t *, int);

/* Declaration of the global variables, if any, which are referred here, but
 * defined in other files */
extern int slot_no, instance_no, terminate_feature;

extern app_sim_lib_globals_t *app_sim_lg;
extern uint32_t debug_flag;

volatile sig_atomic_t ttcp_thread_state = FALSE;

int run_ttcp_bldg_block(FEAT_CMD_MSG *feat_cmd) {
        pthread_t txrx_thread;
        int ltc_id = 0, l_sno = 0;
        WKR_TC_LIST wkr_tc_entry;
        FEAT_CMD_MSG msg;

        DTTCP_WKR("ASWD-%d-%d run_ttcp_bldg_block start\n", slot_no, instance_no);
        memset(&msg, 0, sizeof(FEAT_CMD_MSG));
        memcpy(&msg, feat_cmd, sizeof(FEAT_CMD_MSG));
        memset(&wkr_tc_entry, 0, sizeof(WKR_TC_LIST));
        wkr_tc_entry.tc_id = msg.tc_id;
        wkr_tc_entry.serial_no = msg.serial_no;
        wkr_tc_entry.next = NULL;
        wkr_tc_entry.run_thread = TRUE;
        if (wkr_tc_entry.tc_id != 0){
                add_entry_wkr_list(wkr_tc_entry);
        }
        ltc_id = tc_id;
        l_sno = sno;
        tc_id = msg.tc_id;
        sno = msg.serial_no;
        DTTCP_WKR("ASWD-%d-%d run_ttcp_bldg_block TC[%d] SEQ[%d]\n", slot_no, instance_no,wkr_tc_entry.tc_id, wkr_tc_entry.serial_no);
        if (ltc_id != 0){
                del_entry_wkr_list(ltc_id, l_sno);
        }
#ifndef GTC
        if ( ( msg.f_args.ttcp_args.rslot == slot_no ) && ( msg.f_args.ttcp_args.rinstance == msg.instance ) ) {
                if ( pthread_create(&txrx_thread, NULL, ttcp_reciever_thread, (void *)&msg) != 0 ) {
                        DBG_WKR("ASWD-%d-%d run_ttcp_bldg_block creating ttcp_reciever_thread failed\n", slot_no, instance_no);
                        DTTCP_WKR("ASWD-%d-%d run_ttcp_bldg_block end fail\n", slot_no, instance_no);
                        return FAILURE_VAL;
                }
                else {
                        app_sim_lg->ttcp_txrx_thread = txrx_thread;
                        sleep(1);
                }
        } else {
                if ( pthread_create(&txrx_thread, NULL, ttcp_transmitter_thread, (void *)&msg) != 0 ) {
                        DBG_WKR("ASWD-%d-%d run_ttcp_bldg_block creating ttcp_transmitter_thread failed\n", slot_no, instance_no);
                        DTTCP_WKR("ASWD-%d-%d run_ttcp_bldg_block end fail\n", slot_no, instance_no);
                        return FAILURE_VAL;
                }
                else {
                        app_sim_lg->ttcp_txrx_thread = txrx_thread;
                        sleep(1);
                }
        }
#endif
        DTTCP_WKR("ASWD-%d-%d run_ttcp_bldg_block end\n", slot_no, instance_no);
        return SUCCESS_VAL;
} /* End of run_ttcp_bldg_block */

void ignore_sigpipe (int signum)
{

}

#ifndef GTC
void *ttcp_transmitter_thread(void *p) {
        struct in_addr ip_addr;
        int ltcid = 0, lsno = 0, sock = 0, ntries = 0, extra_duration = 0, i = 0, no_of_slices = 0, com;
        char *buf = NULL, *host = NULL;
        double timer = 0, rate_tot = 0;
        unsigned long int npkts = 0;
        oai_asp_handle_t asp_handle;
        struct sockaddr_in destaddr, srcaddr;
        FEAT_CMD_MSG msg;
        time_t start = 0, stop = 0;
        uint32_t active_rpslot = 0;
        //uint32_t total_packet_count;
        uint64_t byte_count = 0;

        ttcp_thread_state = TRUE;

        do {
            DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread start\n", slot_no, instance_no);
            memcpy(&msg, (FEAT_CMD_MSG *)p, sizeof(FEAT_CMD_MSG));
            ltcid = tc_id;
            lsno = sno;
            if (ltcid != 0) {
                com = FALSE;
            } else {
                com = TRUE;
            }

            DTTCP_WKR("ASWD-%d-%d slot_no=>%d instance=>%d\n", slot_no, instance_no,slot_no, msg.instance);
            if ( oai_get_active_rpsw_slot(&active_rpslot) == OAI_RC_OK ) {
                active_rpslot++;
            }
            if ( msg.f_args.ttcp_args.rslot == active_rpslot ) {
                DTTCP_WKR("ASWD-%d-%d ****Destination is RP card*****\n", slot_no, instance_no);
                if ( oai_get_active_rpsw_backplane_ip_addr(&ip_addr) != OAI_RC_OK ) {
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread could not retrieve IP address of RP\n", slot_no, instance_no);
                    break;
                }
            } else {
                DTTCP_WKR("ASWD-%d-%d ****Destination is SSC card*****\n", slot_no, instance_no);
                if ( oai_att_get_asp_handle(&asp_handle) != OAI_RC_OK ) {
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread could not retrieve ASP handle\n", slot_no, instance_no);
                    break;
                }
                asp_handle.slot_id = msg.f_args.ttcp_args.rslot - 1;
                if ( oai_att_get_asp_ip_addr(&asp_handle, &ip_addr) != OAI_RC_OK ) {
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread could not retrieve IP address of SSC\n", slot_no, instance_no);
                    break;
                }
                DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread ip_addr %s\n", slot_no, instance_no,inet_ntoa(ip_addr));
            }

            if ( msg.f_args.ttcp_args.rate == 0 ) {
                msg.f_args.ttcp_args.rate = 10;
            }

            if ( msg.f_args.ttcp_args.pkt_len == 0 ) {
                msg.f_args.ttcp_args.pkt_len = DEFAULT_PKT_LEN;
            }

            DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread rate:%d pkt_len:%d\n", slot_no, instance_no, msg.f_args.ttcp_args.rate, msg.f_args.ttcp_args.pkt_len);

            if ( msg.f_args.ttcp_args.port == 0 ) {
                msg.f_args.ttcp_args.port = DEFAULT_TTCP_PORT;
                DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread port %d\n", slot_no, instance_no,msg.f_args.ttcp_args.port);
            }

            bzero((char *)&srcaddr, sizeof(srcaddr));
            host = inet_ntoa(ip_addr);

            if ( atoi(host) > 0 ) {
                srcaddr.sin_family = AF_INET;
                srcaddr.sin_addr.s_addr = inet_addr(host);
            }
            srcaddr.sin_port = htons(msg.f_args.ttcp_args.port);
            memset(&destaddr,0,sizeof(struct sockaddr_in));
            destaddr.sin_port = 0;
            destaddr.sin_addr.s_addr = htonl(INADDR_ANY);
            destaddr.sin_family = AF_INET;

            buf = (char *)malloc(msg.f_args.ttcp_args.pkt_len);
            if ( buf == NULL ) {
                DBG_WKR("aswd-%d-%d ttcp_transmitter_thread: malloc error\n", slot_no, instance_no);
                break;
            }

            sock = socket(AF_INET, msg.f_args.ttcp_args.protocol?SOCK_DGRAM:SOCK_STREAM, 0);
            if ( sock < 0 ) {
                DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread: socket error\n", slot_no, instance_no);
                break;
            }

            if ( msg.f_args.ttcp_args.dscp ) {
                msg.f_args.ttcp_args.dscp <<= 2;
                if ( setsockopt(sock, IPPROTO_IP, IP_TOS,  &msg.f_args.ttcp_args.dscp, sizeof(msg.f_args.ttcp_args.dscp)) ){
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread: setsockopt for IP_TOS failed\n", slot_no, instance_no);
                }
            }

            DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread: socket:%d\n", slot_no, instance_no,sock);
            if ( !msg.f_args.ttcp_args.protocol ) {
                signal(SIGPIPE, ignore_sigpipe);
                while( ntries < 5 ) {
                    if ( connect(sock, (struct sockaddr*)&srcaddr, sizeof(srcaddr)) <= FAILURE_VAL ) {
                        DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread connect failed... will retry after %d sec\n", slot_no, instance_no, BREAKTIME);
                        sleep( BREAKTIME );
                        ntries++;
                    } else {
                        DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread connect successful sock:%d\n", slot_no, instance_no,sock);
                        break;
                    }
                }
                if ( ntries >= 5 ) {
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread connect failed\n", slot_no, instance_no);
                    break;
                }
            } else {
                while( ntries < 5 ) {
                    if ( bind(sock, (struct sockaddr * )&destaddr, sizeof(destaddr)) <= FAILURE_VAL ) {
                        DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread bind failed... will retry after %d sec\n", slot_no, instance_no, BREAKTIME);
                        sleep( BREAKTIME );
                        ntries++;
                    } else {
                        DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread bind successful:%d\n", slot_no, instance_no,sock);
                        break;
                    }
                }
                if ( ntries >= 5 ) {
                    DBG_WKR("ASWD-%d-%d ttcp_transmitter_thread bind failed\n", slot_no, instance_no);
                    break;
                }
            }
        }while(FALSE);

        pattern(buf, msg.f_args.ttcp_args.pkt_len);

        DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread Initial time:%lf\n", slot_no, instance_no,(double)start);
        time(&start);

        if (msg.f_args.ttcp_args.max_rate - msg.f_args.ttcp_args.rate > 0){
        extra_duration =  msg.f_args.ttcp_args.duration % WINDOW_WIDTH;
        if (extra_duration){
            send_traffic(buf, &sock, &srcaddr, &byte_count, &msg, extra_duration );
        }

        if (msg.f_args.ttcp_args.duration - extra_duration) {
            no_of_slices = msg.f_args.ttcp_args.duration / WINDOW_WIDTH;
            for ( i = 0 ; i < no_of_slices; i ++){
                send_traffic(buf, &sock, &srcaddr, &byte_count, &msg, WINDOW_WIDTH );
            }
        }
        }else{
                send_traffic(buf, &sock, &srcaddr, &byte_count, &msg, msg.f_args.ttcp_args.duration );
        }
        time (&stop);
        timer = difftime (stop, start);

        if ( timer > 0 ) {
            rate_tot = (byte_count * 8) /  msg.f_args.ttcp_args.duration;
            rate_tot = rate_tot/(1024*1024);
            npkts = (byte_count/msg.f_args.ttcp_args.pkt_len);
            //DBG_WKR("ASWD-%d-%d: Overall Time taken:%d \n", slot_no, instance_no, (int)timer);
        }
        DBG_WKR("ASWD-%d-%d Transmitted rate: %lf Mbps Transmitted Packets: %lu byte_count:%lu\n ",
                slot_no, instance_no, rate_tot, npkts, byte_count);


        if (ltcid != 0){
            del_entry_wkr_list(ltcid, lsno);
        }
        if ( (ltcid == tc_id) && (lsno == sno) ) {
            tc_id = 0;
            sno = 0;
        }
        if ( sock > 0 ) {
            close(sock);
            sock = 0;
        }
        if ( buf != NULL ) {
            free(buf);
            buf = NULL;
        }
        DTTCP_WKR("ASWD-%d-%d ttcp_transmitter_thread end\n", slot_no, instance_no);

        ttcp_thread_state = FALSE;
        pthread_exit(NULL);
} /* End of ttcp_transmitter_thread */

void send_traffic(char *buf, int *sock, struct sockaddr_in* srcaddr, uint64_t* byte_count, FEAT_CMD_MSG* msg, int duration ){

    uint32_t packet_transmit_count;
    int time_tick = 0, rand_rate = 0, extra_rate = 0;
    int bytes_transmitted = 0, i = 0;
    bool shall_sleep = TRUE;
    struct timespec sec_timer_begin, sec_timer_end, diff;
    time_t init_time = 0;

    extra_rate = msg->f_args.ttcp_args.max_rate - msg->f_args.ttcp_args.rate;
    if(extra_rate > 0){
        rand_rate = msg->f_args.ttcp_args.rate + rand() % extra_rate;
    }else {
        rand_rate = msg->f_args.ttcp_args.rate;
    }

    DBG_WKR("ASWD-%d-%d ttcp_send_traffic start. duration:%d, rate: %d\n", slot_no, instance_no, duration, rand_rate);
    /*
     * Convert rate into Mbps. Packet_len is in Bytes.
     * Getting the packet transmit count/sec
     */
    packet_transmit_count =
        (duration * rand_rate * 1024 * 1024) / (msg->f_args.ttcp_args.pkt_len * 8);

        /* Packets to be transmitted/second */
        for (i = 0; i < packet_transmit_count; i++) {
            bytes_transmitted =
                sendto (*sock, (void *)buf,
                        msg->f_args.ttcp_args.pkt_len, 0,
                        (struct sockaddr * )srcaddr,
                        sizeof(struct sockaddr_in));
            if (bytes_transmitted > 0) {
                *byte_count += bytes_transmitted;
            }else{

            }
        }

} /* End of function*/

void *ttcp_reciever_thread(void *p) {
    int ltcid = 0, lsno = 0, sock = 0, accept_fd = 0, ntries = 0, cnt = 0, bAccept = TRUE, com = 0;
    char *buf = NULL;
    unsigned long int npkts = 0;
    struct sockaddr_in destaddr, frominet;
    socklen_t len = sizeof(frominet);
    double timer = 0, rate_tot = 0;
    uint64_t nbytes = 0;
    FEAT_CMD_MSG msg;
    time_t start = 0, stop = 0;
    int gen_sock;
    struct timespec timeout;
    int status = 0;
    fd_set read;

    ttcp_thread_state = TRUE;
    do {
        DTTCP_WKR("ASWD-%d-%d ttcp_reciever_thread start\n", slot_no, instance_no);
        memcpy(&msg, (FEAT_CMD_MSG *)p, sizeof(FEAT_CMD_MSG));
        ltcid = tc_id;
        lsno = sno;
        if (ltcid != 0) {
            com = FALSE;
        } else {
            com = TRUE;
        }

        DTTCP_WKR("ASWD-%d-%d slot_no=>%d instance=>%d\n", slot_no, instance_no,slot_no, msg.instance);
        if ( msg.f_args.ttcp_args.pkt_len == 0 ) {
            msg.f_args.ttcp_args.pkt_len = 1500;
        }

        if ( msg.f_args.ttcp_args.port == 0 ) {
            msg.f_args.ttcp_args.port = DEFAULT_TTCP_PORT;
            DTTCP_WKR("ASWD-%d-%d ttcp_reciever_thread port %d\n", slot_no, instance_no,msg.f_args.ttcp_args.port);
        }

        memset(&destaddr,0,sizeof(struct sockaddr_in));
        /* rcvr */
        destaddr.sin_port =  htons(msg.f_args.ttcp_args.port);
        destaddr.sin_family = AF_INET;
        destaddr.sin_addr.s_addr = htonl(INADDR_ANY);

        buf = (char *)malloc(msg.f_args.ttcp_args.pkt_len);
        if ( buf == NULL ) {
            DBG_WKR("ASWD-%d-%d ttcp_reciever_thread: malloc error\n", slot_no, instance_no);
            break;
        }

        sock = socket(AF_INET, msg.f_args.ttcp_args.protocol?SOCK_DGRAM:SOCK_STREAM, 0);
        if ( sock < 0 ) {
            DBG_WKR("ASWD-%d-%d ttcp_reciever_thread: socket error\n", slot_no, instance_no);
            break;
        }
        if(fcntl(sock, F_SETFL, O_NONBLOCK) < 0){
            DBG_WKR("ASWD-%d-%d ttcp_reciever_thread:fcntl failed to set socket to nonblocking\n", slot_no, instance_no);
        }

        while( ntries < 5 ) {
            if ( bind(sock, (struct sockaddr*)&destaddr, sizeof(destaddr)) <= FAILURE_VAL ) {
                DBG_WKR("ASWDS-%d-%d ttcp_reciever_thread bind failed... will retry after %d sec\n", slot_no, instance_no, BREAKTIME);
                sleep( BREAKTIME );
                ntries++;
            } else {
                DTTCP_WKR("ASWD-%d-%d ttcp_reciever_thread bind successful sock:%d\n", slot_no, instance_no,sock);
                break;
            }
        }
        if ( ntries >= 5 ) {
            DBG_WKR("ASWD-%d-%d ttcp_reciever_thread bind failed\n", slot_no, instance_no);
            break;
        }

        if ( !msg.f_args.ttcp_args.protocol )  {
            listen(sock, 0);
            while ( bAccept && get_run_flag(ltcid, lsno, com) ) {
                accept_fd = accept(sock, (struct sockaddr *)&frominet, &len);
                if ( accept_fd >= SUCCESS_VAL ) {
                    bAccept = FALSE;
                }
            }
        }

        if ( msg.f_args.ttcp_args.protocol == con_TCP ) {
            gen_sock = accept_fd;
        } else {
            gen_sock = sock;
        }

        /* Wait till the first packet arrived or duration expired */
        timeout.tv_sec = ( 4 * msg.f_args.ttcp_args.duration );
        timeout.tv_nsec = 0;


        FD_ZERO(&read);
        FD_SET(gen_sock, &read);

        status = pselect(gen_sock +1, &read, NULL, NULL, &timeout, NULL);
        if (status <= 0) {
            /* No Packet arrived till the duration assigned */
            DBG_WKR("ASWD-%d-%d No Packets arived for the given duaration \n",
                    slot_no, instance_no);
            break;
        }

        time(&start);

        do {
            if (!get_run_flag(ltcid, lsno, com) && (terminate_feature == TERM_TTCP)) {
                break;
            }

            cnt = recvfrom( gen_sock, buf, msg.f_args.ttcp_args.pkt_len, 0,
                    (struct sockaddr * )&frominet, &len );
            time(&stop);

            if (cnt > 0) {
                nbytes += cnt;
            }
        } while (difftime(stop,start) < msg.f_args.ttcp_args.duration);
        timer = difftime(stop,start);

    }while(FALSE);

    sleep (2);
    if ( timer > 0 ) {
        rate_tot = (nbytes * 8 ) / (int)timer;
        rate_tot = rate_tot / (1024*1024);
    }

    npkts = (nbytes / msg.f_args.ttcp_args.pkt_len);
    DBG_WKR("ASWD-%d-%d Received rate:%lf Mbps Received Packets:%lu\n", slot_no, instance_no, rate_tot, npkts);
    del_entry_wkr_list(ltcid, lsno);
    if ( (ltcid == tc_id) && (lsno == sno) ) {
        tc_id = 0;
        sno = 0;
    }
    if ( accept_fd >= 0 ) {
        close(accept_fd);
        accept_fd = 0;
    }
    if ( sock > 0 ) {
        close(sock);
        sock = 0;
    }
    if ( buf != NULL ) {
        free(buf);
        buf = NULL;
    }
    DTTCP_WKR("ASWD-%d-%d ttcp_reciever_thread end\n", slot_no, instance_no);
    ttcp_thread_state = FALSE;
    pthread_exit(NULL);
} /* End of ttcp_reciever_thread */
#endif
void pattern( char *cp, int cnt ) {
    char c = 0;

    while( cnt-- > 0 )  {
        while( !isprint((c&0x7F)) )  c++;
        *cp++ = (c++&0x7F);
    }
} /* End of pattern() */

int  timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec) {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_nsec - y->tv_nsec > 1000000000) {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     *      *      tv_nsec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

xkumapu@camolx2220:~/working_dir/TTCP/DYN_BW/native (topic)$

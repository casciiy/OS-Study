/* sw.c */
/*
	使用停等协议来进行通信的例子，程序运行方法如下：

	sw s_s.dat s_r.dat sdl rdl 400
	sw r_s.dat r_r.dat rdl sdl 500

	400、500的延时很重要，如果设置太小，会导致信道忙，传输反而慢。
	设置太大，那么也会传输太慢。
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <io.h>


#define	true 	1
#define	false 	0

#define	FILE_WRITE_PERMISSION	2
#define	FILE_READ_PERMISSION    4

char PHYSICAL_DATA_LINK_FILE_WRITE[12];
char PHYSICAL_DATA_LINK_FILE_READ[12];

enum protocol_event {
    PACKET_READY,
    FRAME_ARRIVAL,
    TIME_OUT,
    NO_EVENT,
    ChannelIdle,
    CheckSumError
};

typedef	char packet;

struct frame		/* frame structure */
{
    packet info;
    int seq;
    int ack;
};

packet buffer;

int NextFrameToSend;
int FrameExpected;
int PacketLength;
int waiting;
int network_layer_flag;

FILE *fps, *fpr;
int fps_length;

#define INTR 0X1C    /* The clock tick interrupt */

int	init_timer_value;	/* a second is 17.2 tick */

void interrupt ( *oldhandler)();

int timer_count = 0;
int timer_run_flag = false;
int delay_time;

void interrupt handler()
{
    if(timer_run_flag)
    {
        timer_count--;
    }

    oldhandler();		/* call the old routine */
}


char *event_string(enum protocol_event e, char *e_str)
{
    switch (e)
    {
    case PACKET_READY:
        strcpy(e_str, "PACKET_READY");
        break;
    case FRAME_ARRIVAL:
        strcpy(e_str, "FRAME_ARRIVAL");
        break;
    case TIME_OUT:
        strcpy(e_str, "TIME_OUT");
        break;
    case NO_EVENT:
        strcpy(e_str, "NO_EVENT");
        break;
    case ChannelIdle:
        strcpy(e_str, "ChannelIdle");
        break;
    case CheckSumError:
        strcpy(e_str, "CheckSumError");
        break;
    default:
        strcpy(e_str, "Unknow Event");
        break;
    }
    return e_str;
}


void initialize(char *send_data, char *recv_data, char *send_dl, char *recv_dl, char *dt)
{
    fps = fopen(send_data, "rb");
    if(!fps){
        printf("Open file %s to read error!\n", send_data);
        exit(1);
    }
    fseek(fps,0L,SEEK_END);
    fps_length = ftell(fps);
    fseek(fps,0L,SEEK_SET);

    fpr = fopen(recv_data, "wb");
    if(!fpr){
        printf("Open file %s to write error!\n", recv_data);
        exit(1);
    }

    strcpy(PHYSICAL_DATA_LINK_FILE_WRITE, send_dl);
    strcpy(PHYSICAL_DATA_LINK_FILE_READ, recv_dl);

    delay_time = atoi(dt);			/* this is microsecond */
    init_timer_value =  ((((float)delay_time) / 1000.0) * 17.2) * 2;
    /*init_timer_value 設置為兩倍的傳輸時間!!*/

    /* at first, the physical link is idle */
    unlink(PHYSICAL_DATA_LINK_FILE_WRITE);
    unlink(PHYSICAL_DATA_LINK_FILE_READ);

    /* save the old interrupt vector */
    oldhandler = getvect(INTR);

    /* install the new interrupt handler */
    setvect(INTR, handler);

    NextFrameToSend = 0;
    FrameExpected = 0;
    PacketLength = 0;
    waiting = false;
    network_layer_flag = true;
}

void cleanup()
{
    fclose(fps);
    fclose(fpr);

    /* when exit, set the state of physical link to idle */
    unlink(PHYSICAL_DATA_LINK_FILE_WRITE);
    unlink(PHYSICAL_DATA_LINK_FILE_READ);

    /* reset the old interrupt handler */
    setvect(INTR, oldhandler);
}


void start_timer(void)
{
    timer_count = init_timer_value;
    timer_run_flag = true;
}

void stop_timer(void)
{
    timer_run_flag = false;
}

int check_timer_expire(void)
{
    if((timer_count <= 0) && (timer_run_flag))
        return 1;
    else
        return 0;
}

void enable_network_layer(void){
    network_layer_flag = true;
}

void disable_network_layer(void){
    network_layer_flag = false;
}

void get_packet_from_network(packet *buffer, int *PacketLength)
{
    (*buffer) = (packet)fgetc(fps);
    (*PacketLength) = sizeof(packet);
}

void put_packet_to_network(packet pk)
{
    fputc((int)pk, fpr);
    fflush(fpr);
}

void send_frame_to_physical(struct frame fr)
{
    FILE *fp;

    /* 二進位打開檔寫，如果檔原來有內容，將刪除原來的內容 */
    fp = fopen(PHYSICAL_DATA_LINK_FILE_WRITE, "wb");
    if(!fp)
    {
        printf("Open data link file %s to write error!\n", PHYSICAL_DATA_LINK_FILE_WRITE);
        return;
    }
    fwrite(&fr, sizeof(struct frame),1, fp);
    fclose(fp);
}

void get_frame_from_physical(struct frame *fr)
{
    FILE *fp;

    /* 設置幀為無效 */
    fr->seq = -255;
    fr->ack = -255;

    fp = fopen(PHYSICAL_DATA_LINK_FILE_READ, "rb");
    if(!fp)
    {
        printf("Open data link file %s to read error!\n", PHYSICAL_DATA_LINK_FILE_READ);
        return;
    }
    fread(fr, sizeof(struct frame),1, fp);
    fclose(fp);

    /* this frame has been read, the file will be unlink */
    unlink(PHYSICAL_DATA_LINK_FILE_READ);
}

enum protocol_event get_event_from_physical()
{
#define	FILE_EXIST_FLAG	0
#define	FILE_READ_FLAG	4

    if(access(PHYSICAL_DATA_LINK_FILE_READ, FILE_READ_FLAG) == 0)
        return	FRAME_ARRIVAL;
    else
        return NO_EVENT;
}

int channel_idle(void)
{
    return true;		/* 認為物理通道總是空閒 */
}

enum protocol_event get_event_from_timer()
{
    if(check_timer_expire())
    {
        stop_timer();
        return TIME_OUT;
    }
    else
        return NO_EVENT;
}

/* Main protocol service routine */
void stop_wait_protocol_datalink(int event)
{
    struct frame send_frame, recv_frame, ack_frame;
    int PacketLength;

    switch(event)
    {
    case PACKET_READY :
        /* a packet has arrived from the network layer */
        get_packet_from_network(&buffer, &PacketLength);
        send_frame.info = buffer;		/* place packet in frame */
        send_frame.seq = NextFrameToSend;

        /* piggyback acknowledge of last frame received */
        send_frame.ack = 1-FrameExpected;
        if(channel_idle())
        {
            send_frame_to_physical(send_frame); /* send it to physical layer */
            start_timer();
            printf("Send DATA frame {packet = '%c', seq = %d, ack = %d} to physical\n", send_frame.info, send_frame.seq, send_frame.ack);
            waiting = false;
        }
        else
            waiting = true;

        disable_network_layer();
        break;

    case FRAME_ARRIVAL :
        /* a frame has arrived from the physical layer */
        get_frame_from_physical(&recv_frame);		/* get the frame */
        printf("Get a frame {packet = '%c', seq = %d, ack = %d} from physical\n", recv_frame.info, recv_frame.seq, recv_frame.ack);
        /* check that it is the one that is expected */
        if (recv_frame.seq == FrameExpected)	/* 有效幀到達，送到網路層 */
        {
            put_packet_to_network(recv_frame.info);	/* valid frame */
            printf("Put a packet to network {packet = %c}............\n", recv_frame.info);
            FrameExpected = 1-FrameExpected;

            /* 發送確認幀 */
            ack_frame.info = '\0';
            ack_frame.seq = -1;
            ack_frame.ack = recv_frame.seq;		/* 確認剛剛收到的一幀 */
            if(channel_idle())
            {
                send_frame_to_physical(ack_frame); /* send it to physical layer */
                printf("Send ACK frame {seq = %d, ack = %d}\n", ack_frame.seq, ack_frame.ack);
            }
        }
        if(recv_frame.ack == NextFrameToSend) /* acknowledgment has arrived，確認到達，可以發送下一幀 */
        {
            stop_timer();		/* 停止重發計時器 */

            enable_network_layer();
            NextFrameToSend = 1-NextFrameToSend;
        }
        break;

    case TIME_OUT :
        /* a frame has not been ACKed in time, */
        /* so re-send the outstanding frame */
        send_frame.info = buffer;
        send_frame.seq = NextFrameToSend;
        send_frame.ack = 1-FrameExpected;
        send_frame_to_physical(send_frame);
        start_timer();
        printf("Send DATA frame {packet = '%c', seq = %d, ack = %d) to physical\n", send_frame.info, send_frame.seq, send_frame.ack);
        break;

    case ChannelIdle:
        if ( waiting )
        {
            send_frame.info = buffer;
            send_frame.seq = NextFrameToSend;
            send_frame.ack = 1-FrameExpected;
            send_frame_to_physical(send_frame); /* send it to physical layer */
            start_timer();
            waiting = false;
        }
        break;
    case CheckSumError :	/* ignored */
        break;
    }
}

enum protocol_event get_event_from_network()
{
    if(!network_layer_flag)
        return NO_EVENT;

    if(ftell(fps) < fps_length)
        return PACKET_READY;
    else
        return NO_EVENT;

}

/*
	arg1	network send data (send file name)
	arg2	network receive data (receive file name)
*/
int main(int argc, char *argv[]){

    enum protocol_event event;
    char ev[20];
    unsigned int count;

    if(argc != 6)
    {
        printf("Usage: %s send_data recv_data send_dl  recv_dl delay_time", argv[0]);
        exit(0);
    }

    initialize(argv[1], argv[2], argv[3], argv[4], argv[5]);

    count = 1;
    printf("Protocol begin, any key to exit!\n\n");
    while(true)
    {
        printf("%d\n", count++);

        event = get_event_from_network();
        printf("Event from network = %s\n", event_string(event, ev));
        stop_wait_protocol_datalink(event);

        event = get_event_from_physical();
        printf("Event from physical = %s\n", event_string(event, ev));
        stop_wait_protocol_datalink(event);

        event = get_event_from_timer();
        printf("Event from timer = %s, timer_count = %d\n", event_string(event, ev), timer_count);
        stop_wait_protocol_datalink(event);

        delay(delay_time);

        if(kbhit())	break;
    }

    cleanup();

    return 0;
}

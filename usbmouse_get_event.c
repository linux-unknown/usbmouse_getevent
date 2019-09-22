#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>

#include <sys/prctl.h>

#include <sys/time.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/input.h>

#include <sys/select.h>


#define DEBUG 1

#ifdef DEBUG
#define u_log(fmt, ...)	\
	 printf(fmt, ##__VA_ARGS__)
#else
#define u_log(fmt, ...)
#endif

 /*
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
*/

/*
Event types
#define EV_SYN                  0x00
#define EV_KEY                  0x01
#define EV_REL                  0x02
#define EV_ABS                  0x03
*/
/*
 Relative axes
#define REL_X                   0x00
#define REL_Y                   0x01
#define REL_Z                   0x02
#define REL_MAX                 0x0f
#define REL_CNT                 (REL_MAX+1)
*/
//event8  mouse
//event9  keyboard

#define INPUT_DEVICES "/proc/bus/input/devices"

#define USB_MOUSE_NUM	(4)
#define portnum 3333
#define WIRELESS_SERIAL "/dev/ttyS4"
#define NET_CONNECT		(1)
#define NET_DISCONNECT	(0)
#define HEARTBEAT_ID_HIG	(0xff)
#define HEARTBEAT_ID_LOW	(0xff)

struct device_info {
	int fd;
	char dev_path[64];
	pthread_t tidp;
};

struct usb_mouse_data{
	signed char id;
	signed char data[4];
};

struct mouse_dev{
	struct device_info dev_info[USB_MOUSE_NUM];
	struct input_event event_data[USB_MOUSE_NUM];
	struct usb_mouse_data umouse_data[USB_MOUSE_NUM];
};


static struct mouse_dev usb_mouse_devs;
static int sockfd;
static int wiress_serial_fd;
static pthread_mutex_t mutex;
static int mouse_cnt = 0;
static int net_status = NET_DISCONNECT;
static pthread_t heart_beat_tpid;
static char *ip_addr = NULL;

unsigned char frame_data[] = {
	0x55, 0x55, 0x55, 0x55,/*frame head*/
	0x00, 0x00, /*device id*/
	0x00, /*frame count*/
	0x00, /*mouse id*/
	0x00, /*left right move*/
	0x00, /* up down move*/
	0xAA, 0xAA, 0xAA, 0xAA /*frame end*/
};

int net_connect(char *ip_addr)
{
	struct sockaddr_in server_addr;
	int cnt = 0;
	int ret = 0;
	
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		u_log("create socket error!\n");
		return -1;
	}

	u_log("socket successfully\n");
	u_log("server ipaddr:%s\n", ip_addr);
	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portnum);
	//server_addr.sin_addr.s_addr = inet_addr("10.10.10.100");
	server_addr.sin_addr.s_addr = inet_addr(ip_addr);

	while (1) {
		ret = connect(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr));
		u_log("ret:%d\n", ret);
		if (ret < 0){
			perror("connect error\n");
			if (ECONNREFUSED == errno) {
				cnt++;
				u_log("connect cnt:%d\n", cnt);
				sleep(1);
				/* retry 5 minute */
				if (cnt >= 300)
					break;
				continue;
			}
		}
		u_log("connect successfully\n");
		if (ret >= 0)
			net_status = NET_CONNECT;
		break;
	}

	return ret;
}


void rel_event_process(struct input_event *event_data,
		struct usb_mouse_data *mouse)
{
	switch (event_data->code){
	case REL_X:
		u_log("event_mouse.code_X:%d\n", event_data->code);
		u_log("event_mouse.value_X:%d\n", event_data->value);
		mouse->data[1] = event_data->value;
		break;
	case REL_Y:
		u_log("event_mouse.code_Y:%d\n", event_data->code);
		u_log("event_mouse.value_Y:%d\n", event_data->value);
		mouse->data[2] = event_data->value;
		break;
	case REL_WHEEL:
		u_log("event_mouse.code_W:%d\n", event_data->code);
		u_log("event_mouse.value_W:%d\n", event_data->value);
		mouse->data[3] = event_data->value;
		break;
	default:
		u_log("%s invalod evnet code:%d\n", __func__, event_data->code);
		return;
	}
	
}

void key_event_process(struct input_event *event_data,
		struct usb_mouse_data *mouse)
{
	switch (event_data->code) {
	case BTN_LEFT:
		u_log("event_mouse.code_BTN_LEFT:%d\n", event_data->code);
		u_log("event_mouse.value_BTN_LEFT:%d\n", event_data->value);
		if (1 == event_data->value)
			mouse->data[0] |= 0x01;
		else if (0 == event_data->value)
			mouse->data[0] &= 0xFE;
		else if (2 == event_data->value)
			u_log("repeat key\n");
		break;
	case BTN_RIGHT:
		u_log("event_mouse.code_BTN_RIGHT:%d\n", event_data->code);
		u_log("event_mouse.value_BTN_RIGHT:%d\n", event_data->value);
		if (1 == event_data->value)
			mouse->data[0] |= 0x02;
		else if (0 == event_data->value)
			mouse->data[0] &= 0xFD;
		else if (2 == event_data->value)
			u_log("repeat key\n");

		break;	
	case BTN_MIDDLE:
		u_log("event_mouse.code_BTN_MIDDLE:%d\n", event_data->code);
		u_log("event_mouse.value_BTN_MIDDLE:%d\n", event_data->value);
		if (1 == event_data->value)
			mouse->data[0] |= 0x04;
		else if (0 == event_data->value)
			mouse->data[0] &= 0xFB;
		else if (2 == event_data->value)
			u_log("repeat key\n");

		break;
	case BTN_SIDE:
		u_log("event_mouse.code_BTN_SIDE:%d\n", event_data->code);
		u_log("event_mouse.value_BTN_SIDE:%d\n", event_data->value);
		if (1 == event_data->value)
			mouse->data[0] |= 0x08;
		else if (0 == event_data->value)
			mouse->data[0] &= 0xF7;
		else if (2 == event_data->value)
			u_log("repeat key\n");
		break;
	case BTN_EXTRA:
		u_log("event_mouse.code_BTN_EXTRA:%d\n", event_data->code);
		u_log("event_mouse.value_BTN_EXTRA:%d\n", event_data->value);
		if (1 == event_data->value)
			mouse->data[0] |= 0x10;
		else if (0 == event_data->value)
			mouse->data[0] &= 0xEF;
		else if (2 == event_data->value)
			u_log("repeat key\n");
		break;
	default:
		u_log("%s invalod evnet code:%d\n", __func__, event_data->code);
	}
}


static int event_data_report(unsigned short id, char data_x, char data_y)
{
	int i = 0;
	ssize_t cnt = 0;
	ssize_t data_len = sizeof(frame_data);
	ssize_t write_len = 0;
	
	pthread_mutex_lock(&mutex);
#if 0
	send(sockfd, udata, sizeof(*udata), MSG_NOSIGNAL);
#endif
	frame_data[6]++;
	frame_data[7] = id & 0xff;
	frame_data[8] = data_x;
	frame_data[9] = data_y;

	u_log("report mouse %x\n", id);
	
	u_log("data_x:%d ", data_x);
	u_log("data_y:%d ", data_y);
	u_log("frame cnt:%d ", frame_data[6]);
	u_log("\n");

	if (net_status != NET_CONNECT) {
		u_log("net disconnetc\n");
		return -1;
	}
	while(cnt < data_len && i < 10) {
#if 0
		cnt = write(wiress_serial_fd, frame_data, data_len - write_len);
#else
		cnt = send(sockfd, frame_data, data_len - write_len,  MSG_NOSIGNAL);
		u_log("send cnt %d\n", cnt);
#endif
		if (cnt < 0) {
			perror("write serial error");
			pthread_mutex_unlock(&mutex);
			return cnt;
		}
		i++;
		write_len = write_len + cnt;
	}
	pthread_mutex_unlock(&mutex);

	return cnt;
}

static void *event_process_pthread(void *arg)
{
	struct usb_mouse_data *umouse_data = arg;
	int id = umouse_data->id;
	char pname[128] = {0};
	ssize_t len = 0;
	int j = 0;

	u_log("%s id:%d\n", __func__, id);
	sprintf(pname, "event_process_%d", id);
	prctl(PR_SET_NAME, pname);

	usb_mouse_devs.dev_info[id].fd = open(usb_mouse_devs.dev_info[id].dev_path, O_RDWR);
	if (usb_mouse_devs.dev_info[id].fd < 0) {
		perror("open mouse event failed!\n");
		u_log("open error device:%s\n", usb_mouse_devs.dev_info[id].dev_path);
		return NULL;
	}
	while(1) {
		len = read(usb_mouse_devs.dev_info[id].fd, &(usb_mouse_devs.event_data[id]) ,
									sizeof(usb_mouse_devs.event_data[id]));
		if (len < 0) {
			u_log("read error fd:%d\n", usb_mouse_devs.dev_info[id].fd);
			perror("");
		}
		u_log("%s %s\n", __func__, pname);
		switch(usb_mouse_devs.event_data[id].type) {
		case EV_REL:
			rel_event_process(&(usb_mouse_devs.event_data[id]), &(usb_mouse_devs.umouse_data[id]));
			break;
		case EV_KEY:
			key_event_process(&(usb_mouse_devs.event_data[id]), &(usb_mouse_devs.umouse_data[id]));
			break;
		case EV_SYN:
			u_log("sync!\n");
			u_log("mouse %d\n", id);
			for (j = 0; j < 4; j++)
				 u_log("data:%d ", usb_mouse_devs.umouse_data[id].data[j]);
			u_log("\n\n");

			event_data_report(usb_mouse_devs.umouse_data[id].id,
				usb_mouse_devs.umouse_data[id].data[1], usb_mouse_devs.umouse_data[id].data[2]);
#if 0
			send(sockfd, &(usb_mouse_devs.umouse_data),
				sizeof(usb_mouse_devs.umouse_data), 0);
#endif
			memset(usb_mouse_devs.umouse_data[id].data, 0, 
				sizeof(usb_mouse_devs.umouse_data[id].data));

			break;
		default:
			break;
		}
	}
}


static void *heart_beat_pthread(void *arg)
{
	fd_set rfds;
	struct timeval tv;
	int retval;

	unsigned char recv_frame_data[] = {
		0x55, 0x55, 0x55, 0x55,/*frame head*/
		0x00, 0x00, /*device id*/
		0x00, /*frame count*/
		0x00, /*mouse id*/
		0x00, /*left right move*/
		0x00, /* up down move*/
		0xAA, 0xAA, 0xAA, 0xAA, /*frame end*/
	};

	int frame_data_len = sizeof (recv_frame_data);
	int recv_len = 0;
	int i = 0;
	ssize_t len = 0;

	int ret = 0;
	int net_status_cnt = 0;
	int j = 0;

	unsigned short heat_beat_id = (HEARTBEAT_ID_HIG << 8) | HEARTBEAT_ID_LOW;

	while (1) {
		memset(recv_frame_data, 0, frame_data_len);
		ret = event_data_report(heat_beat_id, 0xde, 0xed);
		if (ret < 0) {
			perror("send hart beat errpr\n");
		}

		while(1) {

			FD_ZERO(&rfds);
			FD_SET(sockfd, &rfds);

			/* Wait up to five seconds. */
			tv.tv_sec = 5;
			tv.tv_usec = 0;

			retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
			/* Don't rely on the value of tv now! */
			if (retval == -1) {
				perror("select()");
			} else if (retval) {
				u_log("data is available now\n");
				i = 0;
				recv_len = 0;
				while (recv_len < frame_data_len && i < 10) {
					len = recv(sockfd, recv_frame_data, frame_data_len - recv_len, MSG_DONTWAIT);
					u_log("recv len:%d\n", len);
					recv_len = recv_len + len;
					i++;
				}
				u_log("recv data len:%d\n", recv_len);
				u_log("time left:%d seconds\n", tv.tv_sec);
				u_log("recv data\n");
				sleep(tv.tv_sec);
	
				{
					int j = 0;
					for (j = 0; j < frame_data_len; j++)
						u_log("%x ", recv_frame_data[j]);
					u_log("\n");
				}

				if ((0x55 == recv_frame_data[0]) && (0x55 == recv_frame_data[1]) 
					&& (0x55 == recv_frame_data[2]) && (0x55 == recv_frame_data[3])) { /*head*/
					if ((0xAA == recv_frame_data[10]) && (0xAA == recv_frame_data[11]) 
						&& (0xAA == recv_frame_data[12]) && (0xAA == recv_frame_data[13])) {/*end*/
						/*mouse id & data*/
						if ((0xff == recv_frame_data[7]) && (0xcf == recv_frame_data[8]) 
							&& (0xfc == recv_frame_data[9])) {
							u_log("recv heart beat packet\n");
							break;
						}
					}
				} else {
					u_log("invalid heart beat packet\n");
					net_status_cnt++;
					u_log("net_status_cnt:%d\n", net_status_cnt);
					if (net_status_cnt > 3) {
						net_status_cnt = 0;
						net_status = NET_DISCONNECT;
						while (net_status == NET_DISCONNECT) {
							u_log("connect network......\n");
							u_log("net_status:%d\n", net_status);
							net_connect(ip_addr);
							sleep(5);
						}
						break;
					}
				}
			} else {
				u_log("select timeout 1 seconds net_status_cnt:%d\n", net_status_cnt);
				net_status_cnt++;
				if (net_status_cnt > 3) {
					net_status_cnt = 0;
					net_status = NET_DISCONNECT;
					while (net_status == NET_DISCONNECT) {
						u_log("connect network......\n");
						u_log("net_status:%d\n", net_status);
						net_connect(ip_addr);
						sleep(5);
					}
				}
				break;
			}	
		}
	}
}

int parse_mouse_devices(struct mouse_dev *mdev)
{
	FILE *file = NULL;
	char *line = NULL;
	char *start = NULL;
	char *end = NULL;
	size_t len = 0;
	ssize_t nread;
	long mouse_id = 0;
	long event_id = 0;
	char tmp[20] = {0};
	int i = 0;
	file = fopen(INPUT_DEVICES, "r");
	if (NULL == file) {
		u_log("open file %s failed\n", INPUT_DEVICES);
		perror("");
		exit(-1);
	}

	while ((nread = getline(&line, &len, file)) != -1) {
		if (strstr(line, "Handlers") != NULL) {
			u_log("Handlers:%s\n", line);
			start = strstr(line, "mouse");
			if (start != NULL){
				u_log("start:%s\n", start);
				u_log("start + strlen(\"mouse\"):%s\n", start + strlen("mouse"));
				end = strstr(start, " ");
				if (NULL != end) {
					u_log("end:%s\n", end);
					memset(tmp, 0, sizeof(tmp));
					memcpy(tmp, start + strlen("mouse"), end - start - strlen("mouse"));
					u_log("tmp:%s\n", tmp);
					mouse_id = strtol(tmp, NULL, 10);
					mdev->umouse_data[i].id = mouse_id;
					u_log("mouse_id:%ld\n", mouse_id);	
				}

				start = strstr(line, "event");
				if (start != NULL) {
					u_log("start:%s\n", start);
					end = strstr(start, " ");
					if (NULL != end) {
						u_log("end:%s\n", end);
						memset(tmp, 0, sizeof(tmp));
						memcpy(tmp, start + strlen("event"), end - start - strlen("event"));
						u_log("tmp:%s\n", tmp);
						event_id = strtol(tmp, NULL, 10);
						u_log("event_id:%ld\n", event_id);
						sprintf(mdev->dev_info[i].dev_path, "/dev/input/event%ld", event_id);
						i++;
					}
				}
			}
		}
	}

	mouse_cnt = i;
	u_log("mouse_cnt:%d\n", mouse_cnt);
	return 0;
}

int wiress_serial_init(void)
{
	struct termios options;
	wiress_serial_fd = open(WIRELESS_SERIAL, O_RDWR | O_NOCTTY | O_NDELAY);
	if (wiress_serial_fd < 0) {
		u_log("open %s error\n", WIRELESS_SERIAL);
		perror("");
		return wiress_serial_fd;
	}

	tcgetattr(wiress_serial_fd, &options);

	/* raw input */
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cflag &= ~(CSIZE | PARENB);
	options.c_cflag |= CS8;
	options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10;
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~CSTOPB;

	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);
	tcflush(wiress_serial_fd,TCIFLUSH);
	if(tcsetattr(wiress_serial_fd,TCSANOW,&options) < 0)
	{
		perror("tcsetattr failed");
		return -1;
	} 
}


int main(int argc, char *argv[])
{
	int i = 0;
	int j = 0;
	int ret;
	int type;
	int len = 0;
	fd_set rfds;
	int max_fd = 0;
	int cnt = 0;


#if 0
	usb_mouse_devs.dev_info[0].dev_path = "/dev/input/event6";
	usb_mouse_devs.dev_info[1].dev_path = "/dev/input/event7";
	usb_mouse_devs.dev_info[2].dev_path = "/dev/input/event2";
	usb_mouse_devs.dev_info[3].dev_path = "/dev/input/event3";
#endif

	if (argc < 2) {
		u_log("usage\nusbmouse_get_event 192.168.1.100\n");
		exit(-1);
	}

	ip_addr = argv[1];

	parse_mouse_devices(&usb_mouse_devs);

	for(i = 0; i < USB_MOUSE_NUM && i < mouse_cnt; i++) {
		u_log("id:%d\n", usb_mouse_devs.umouse_data[i].id);
		u_log("dev_path:%s\n", usb_mouse_devs.dev_info[i].dev_path);
	}

	net_connect(ip_addr);

	sleep(3);

	pthread_mutex_init(&mutex, NULL);

	for(i = 0; i < USB_MOUSE_NUM && i < mouse_cnt; i++) {
		//usb_mouse_devs.umouse_data[i].id = i;
		if (pthread_create(&(usb_mouse_devs.dev_info[i].tidp), NULL, event_process_pthread, 
				(void*)(&(usb_mouse_devs.umouse_data[i]))) == -1) {
         	perror("event process thread create error\n");
         	return -1;
     	}
	}

	if (pthread_create(&heart_beat_tpid, NULL, heart_beat_pthread, NULL) == -1) {
         	perror("heart_beat thread create error!\n");
         	return -1;
     }

	for(i = 0; i < USB_MOUSE_NUM && i < mouse_cnt; i++) {
		if (pthread_join(usb_mouse_devs.dev_info[i].tidp, NULL)) {
			u_log("thread is not exit...\n");
		}
	}

	if (pthread_join(heart_beat_tpid, NULL)) {
			u_log("heart beat thread is not exit...\n");
	}
	return 0;
}


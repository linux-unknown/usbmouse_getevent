#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


struct usb_mouse_data{
	signed char id;
	signed char data[4];
};

unsigned char frame_data[] = {
	0x55, 0x55, 0x55, 0x55,/*frame head*/
	0x00, 0x00, /*device id*/
	0x00, /*frame count*/
	0x00, /*mouse id*/
	0x00, /*left right move*/
	0x00, /* up down move*/
	0xAA, 0xAA, 0xAA, 0xAA, /*frame end*/

};

unsigned char heart_frame_data[] = {
	0x55, 0x55, 0x55, 0x55,/*frame head*/
	0x00, 0x00, /*device id*/
	0x00, /*frame count*/
	0xff, /*mouse id*/
	0xcf, /*left right move*/
	0xfc, /* up down move*/
	0xAA, 0xAA, 0xAA, 0xAA, /*frame end*/

};


#define portnum 3333

int main()
{
	int sockfd;
	int new_fd;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	char buffer[128] = {0};
	int nByte;
	int sin_size;

	struct usb_mouse_data mouse;


	//1.创建套接字
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("create socket error!\n");
		exit(1);
	}
	
	//2.1设置要绑定的地址
	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portnum); //字节序(大小端)
	server_addr.sin_addr.s_addr =  inet_addr("10.10.10.100");
		
	//2.绑定地址
	bind(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr));
	
	//3.监听端口
	listen(sockfd, 5);
		
	while(1)
	{
		//4.等待连接
		sin_size = sizeof(struct sockaddr);
		new_fd = accept(sockfd, (struct sockaddr*)(&client_addr), &sin_size);//注意:第3个参数为socklen_t*（整形的指针）
		printf("server get connection from %s\n",inet_ntoa(client_addr.sin_addr)); //将整数型的IP地址转化为字符型(192.168.1.1)
		
		//5.接收数据
		while (1) {
			#if 1
			int i = 0;
			memset(frame_data, 0 , sizeof(frame_data));
			nByte = recv(new_fd, &frame_data, sizeof(frame_data), 0);
			printf("server recv data:\n");
			for (i = 0; i < sizeof(frame_data); i++)
				printf("%x\n", frame_data[i]);
			printf("\n");

			if (frame_data[7] == 0xff) {
				printf("heart beate packet\n");
				{
					int i = 0;
					for (i = 0; i < sizeof(heart_frame_data); i++)
						printf("%x ", heart_frame_data[i]);
					printf("\n");

				}
				send(new_fd, heart_frame_data, sizeof(heart_frame_data),  MSG_NOSIGNAL);
			}
			#if 0
			printf("id:%x\n%d %d %d %d\n\n", 
				mouse.id, mouse.data[0], mouse.data[1], mouse.data[2], mouse.data[3]);
			#endif
			#else
			nByte = recv(new_fd, buffer, 128, 0);
			printf("%s\n", buffer);
			#endif
		}
		//6.结束连接
		close(new_fd);
	}	
	
	close(sockfd);
	
	return 0;
	
}


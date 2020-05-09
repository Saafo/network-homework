#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    // 获得socker描述符
    int client_socket = socket(PF_INET, SOCK_STREAM,0);
    if(client_socket < 0){
        perror("Create socket failed: ");
        exit(1);
    }

    //套接字地址结构
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    //绑定服务器端口
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(18888);

    //连接服务器
    if(connect(client_socket, (struct  sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Cannot connect to server: ");
        exit(1);
    }

    //发送消息
    printf("Please type what you want to send:\n");
    char send_message[256];
    scanf("%s", send_message);
    if(send(client_socket, send_message, sizeof(send_message), 0) < 0) {
        perror("Sent Msg failed: ");
        exit(1);
    }

    //接受消息
    char recv_message[256];
    if(recv(client_socket, recv_message, sizeof(recv_message), 0) < 0) {
        perror("Recv Msg failed: ");
        exit(1);
    }

    //打印消息
    printf("Received a msg:\n%s\n",recv_message);

    //关闭连接
    close(client_socket);
    return 0;
}
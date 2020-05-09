#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void strupr(char *str){
    for (; *str!='\0'; str++)
        *str = toupper(*str);
}

int main(int argc, char* argv[]){
    printf("Server is running now!\n");
    
    //创建套接字地址结构
    struct sockaddr_in server_addr, client_addr;
    int listen_socket, connect_socket;

    // 获得套接字描述符
    if((listen_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("Cannot create socket");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    //接收指定端口的所有消息
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(18888);

    char* addr_s;
    uint16_t port;
    // 将整形地址转为字符型
    addr_s = inet_ntoa(server_addr.sin_addr);
    // 网络字节序转换
    port = ntohs(server_addr.sin_port);
    printf("localhost:%s, port:%u\n", addr_s, port);

    // 将套接字与地址结构绑定
    if(bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Cannot bind socket: ");
        exit(1);
    }

    //允许最大连接数为1024，并且转换为被动套接字
    if(listen(listen_socket, 1024) < 0){
        perror("Cannot convert to passive socket: ");
        exit(1);
    }

    uint16_t clilen = sizeof(client_addr);
    // 返回已连接的套接字（已完成TCP三次握手），完成服务时，将会被关闭
    // 第一个参数为监听套接字，在整个服务器生命周期一直存在，仅一个，用于监听新TCP连接
    // 第二、三参数为已连接TCP请求的客户端信息，可设为空指针
    if((connect_socket = accept(listen_socket, (struct sockaddr*)&client_addr, (socklen_t*)&clilen)) < 0){
        perror("Accept error");
        exit(1);
    }
    addr_s = inet_ntoa(client_addr.sin_addr);
    port = ntohs(client_addr.sin_port);
    printf("remote host:%s, port:%u\n",addr_s, port);

    ssize_t n;
    char buf[256];

    //接受消息，read会一直阻塞
    if((n = recv(connect_socket, buf, sizeof(buf), 0)) > 0){
        buf[n] = '\0';
        printf("Received a msg: %s\n",buf);
        strupr(buf);
        if(send(connect_socket, buf, sizeof(buf), 0) < 0){
            perror("Send msg error: ");
        }
        printf("Sent a msg: %s\n", buf);
    }
    else{
        perror("Recv msg error: ");
    }
    close(connect_socket);
    close(listen_socket);
}
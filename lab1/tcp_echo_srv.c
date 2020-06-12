#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define BACKLOG 1024

//全局变量，提示目前的信号类型是SIGPIPE,还是SIGINT
int sig_type = 0;
int sig_to_exit = 0;

//定义SIGINT的处理函数
void sig_int(int signo) {	
	sig_type = signo;
	printf("[srv] SIGINT is coming!\n");
    sig_to_exit = 1;
}

//定义SIGPIPE的处理函数
void sig_pipe(int signo) {	
	sig_type = signo;
	printf("[srv] SIGPIPE is coming!\n");
}

//这部分为echo服务，接收来自客户端的字符串，将其返回给客户端
void echo_rep(int sockfd)
{
	int len = 0;		//读取到的字符串的长度，字节数
	int len_n = 0; //读取到的网络长度
	int read_res;		//
	char *buf = NULL;	//读取到的字符串对应的存放空间

    do {
		read_res = read(sockfd, &len_n, sizeof(len_n));
		len = ntohl(len_n);
		if(read_res < 0){//判断，如果读取到的字节数小于0，打印提示信息，参考指南！
			printf("[srv] read len return %d and errno is %d\n", read_res, errno);
			if(errno == EINTR){//如果errno为EINTR，表示read中断
				printf("Errno: EINTR\n");
				//表示SIGINT中断，直接退出echo服务函数，准备结束服务器端程序
				if(sig_type == SIGINT){
					printf("sig_type is SIGINT, quitting...\n");
					return;	
				}
				continue;//如果不是SIGINT中断，而是其他问题产生的中断，则继续循环，read数据
			}
			return;//如果是异常导致的读取字节数为负，则退出echo服务函数
		}
		if(read_res == 0){//读取0字节，也退出echo服务函数
			// printf("[srv] read data return 0.\n");
			return;
		}
		
		//此段注意判断边界
		//如果从服务套接字描述符读取的字节数都正确，则继续完成读取字符串和发送给客户端的工作
		//根据指南提示服务器端读取数据也是要受到边界控制的，每行最大100，最小1
		//read_amnt定义读到的字节数，len_to_read是收到的客户端发送的字节数
		
		//分配空间用于存储收到的字符串
		int read_amnt = 0;
		int len_to_read = len;
		buf = (char*)malloc(len * sizeof(char));
		do{
			//printf("[srv] len to read is %d\n", len_to_read);
			read_res = read(sockfd, &buf[read_amnt], len_to_read);
			//读取字符串存到buf空间中，read_res存放读取的字节数
			if(read_res < 0){//判断，如果读取到的字节数小于0，打印提示信息，参考指南！
				printf("[srv] read len return %d and errno is %d\n", read_res, errno);
				if(errno == EINTR) {
					printf("Errno: EINTR\n");
					if(sig_type == SIGINT) {
						printf("sig_type is SIGINT\n");
						free(buf);
						return;
					}
					continue;//如果不是SIGINT中断，而是其他问题产生的中断，则继续循环，read数据
				}
				free(buf);
				return;
			}
			//下面就是判断客户端正常退出
			if(read_res == 0){
				// printf("[srv] read data return 0.\n");
				free(buf);//注意这里要释放buf空间
				return;
			}
			//不断的从服务套接字读取收到的字符数，加入到read_amnt统计变量中
			read_amnt += read_res;
			if(read_amnt == len) {
				break; //读完了，跳出循环
			}
			else if(read_amnt < len) {
				printf("[srv] Continue reading...\n");
				len_to_read = len - read_amnt;
			}
			else {
				printf("[srv] Read more than expected, quitting...\n");
				free(buf);
				return;
			}
			
		}while(1);
		
		//需要打印的字符串
		printf("[echo_rqt] %s\n", buf);
		//将要发送数据的总长度发送到TCP连接中
		len_n = htonl(len);
		write(sockfd, &len_n, sizeof(len_n));
        //将要发送的数据发送到TCP连接中
		write(sockfd, buf, len);
		//printf("[srv] write() done as expected wtih read_amnt[%d]\n", read_amnt);		
        //释放buf空间
		free(buf);
    }while(1);
}
//主函数，程序入口，仍然需要考虑命令行参数的传入
int main(int argc, char* argv[])
{
	//判断是否3个参数，第1个参数是程序名，第2个参数是本机IP地址，第3个参数是本机端口号
	//判断命令行输入的是否3个参数，如果不满足要提示正确的输入信息
	//打印提示信息，注意一定要和指南上的要求一致
	//给出程序退出的返回值0
	if(argc != 3)
	{
		printf("Usage: tcp_echo_srv <addr> <port>\n");
		return 0;
	}
	
	
	//考虑SIGPIPE信号的捕获
	struct sigaction sigact_pipe, old_sigact_pipe;
	sigact_pipe.sa_handler = sig_pipe;//sig_pipe()，信号处理函数
	sigemptyset(&sigact_pipe.sa_mask);
	sigact_pipe.sa_flags = 0;
	sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
	sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);
	
	//考虑SIGINT信号的捕获
	struct sigaction sigact_int, old_sigact_int;
    sigact_int.sa_handler = &sig_int;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;
    sigaction(SIGINT, &sigact_int, &old_sigact_int);
	
	//定义一个字符串变量，用于存储点分十进制数据表示的IP地址（字符串），用于打印输出
	char ip_str[20]={0};
	//定义服务器端的地址结构
	struct sockaddr_in srv_addr;
	//定义客户端的地址结构
	struct sockaddr_in cli_addr, cli_addr_peer;
	//定义监听套接字描述符和用于服务的套接字描述符，服务套接字描述符在为某个客户端服务完成后，需要关闭，释放空间
	int listenfd, connfd, cli_addr_peer_len;
	//socklen_t标准的用法，表示地址结构的长度
	socklen_t clilen;

	//首先创建监听套接字描述符
	listenfd = socket(PF_INET, SOCK_STREAM, 0);
		
	//将服务器端地址结构请0，(srv_addr)
	bzero(&srv_addr, sizeof(srv_addr));
	
	//初始化服务器的地址结构信息
	//family设置为IPv4
	srv_addr.sin_family = AF_INET;
	//将命令行输入的第1个参数，初始化IP地址
	srv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	//将命令行输入的端口号，初始化地址结构中的端口号，注意字节序的转换
	srv_addr.sin_port = htons(atoi(argv[2]));
	//一定要打印消息，提示服务器初始化完成，参照指南格式
	printf("[srv] server[%s:%d] is initializing!\n", inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str)), (int)ntohs(srv_addr.sin_port));
    //将监听套接字描述符绑定到服务器的地址结构
	bind(listenfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
	
	//启动侦听命令，注意第1个参数是监听套接字描述符，第2个参数BACKLOG是定义的服务器允许接受的TCP连接数
	listen(listenfd,BACKLOG);
	
	
	//如果服务器端程序收到了一个中断信号，ctrl+c，sig_to_exit就会被设置为1，如果没有中断就为0
	//会一直执行以下这个while循环
	while(!sig_to_exit)
	{
		//读取客户端地址结构的长度
		clilen = sizeof(cli_addr);
		
		//调用accept函数同意建立连接请求，返回的是服务套接字描述符
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
		
		//判断如果accept函数返回值为-1，则退出本轮while循环，继续监听
		if(connfd == -1 && errno == EINTR)
			break;
		
		//注意如果TCP连接建立成功，一定要打印成功的信息，格式参考指南！
		printf("[srv] client[%s:%d] is accepted!\n", inet_ntop(AF_INET, &cli_addr.sin_addr, ip_str, sizeof(ip_str)), (int)ntohs(cli_addr.sin_port));
		
		if(connfd < 0)
			continue;
		//调用echo的服务函数
		echo_rep(connfd);
		//echo服务执行完毕，关闭服务套接字
		close(connfd);
		printf("[srv] connfd is closed!\n");
	}
	//如果程序收到中断信号会退出运行，关闭监听套接字描述符
	close(listenfd);
	//注意一定要打印程序终止的信息，信息格式参考指南
	printf("[srv] listenfd is closed!\n");
	printf("[srv] server is exiting\n");
	return 0;//设定函数正常退出的返回值为0
}
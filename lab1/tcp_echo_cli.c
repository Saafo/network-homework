#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#define MAX_CMD_STR 100	//最大输入的字符数

//这里的SIGPIPE中只有1个打印命令，我们已经给出，原则上要释放套接字描述符等释放空间的操作，这里不考察
void sig_pipe(int signo) {	
	printf("[cli] SIGPIPE is coming!\n");
}

//定义echo函数，来实现从键盘读入字符串以及发送给服务器，并将从服务器收到的字符串在
//标准输出终端输出的功能
int echo(int sockfd)
{
	//定义一个字符串存储空间，注意需要比最低长度+1，考虑末尾加\0
	char buf[MAX_CMD_STR+1];
	memset(&buf, 0, sizeof(buf));
	//从键盘读入输入的字符串，放在定义的缓冲区中
	while (fgets(buf, MAX_CMD_STR, stdin))
	{
		//如果键盘输入的字符串是exit，表示是客户端程序的退出，就直接中断while循环，退出程序
		if(strncmp(buf, "exit", 4) == 0){
			break;
		}
		//比较输入字符串是否exit，否则继续执行

		//取字符串长度
		int len = strnlen(buf, MAX_CMD_STR);
		int len_n = htonl(len);
		// If only '\n' input, '\0' will be send out;添加结束符
		if(buf[len-1] == '\n')
			buf[len-1] = 0;
        write(sockfd, &len_n, sizeof(len_n));	//根据测评必须使用wirte函数，第一次发送长度
        write(sockfd, buf, len);			//第二次read发送缓存区数据
        memset(buf, 0, sizeof(buf));		//缓冲区清零
        read(sockfd, &len_n, sizeof(len_n));	//从套接字标识符读出数据，第一次读的是长度
		len = ntohl(len_n);
        // read(sockfd, buf, len);				//第二次读字符串数据，存到缓冲区
		// 上面一排代码没有做read不完整的处理，以下代码为优化后代码
		int read_amnt = 0;
		int len_to_read = len;
		int read_res;
		int sig_type = 0;
		do{
			//printf("[srv] len to read is %d\n", len_to_read);
			read_res = read(sockfd, &buf[read_amnt], len_to_read);
			//读取字符串存到buf空间中，read_res存放读取的字节数
			if(read_res < 0){//判断，如果读取到的字节数小于0，打印提示信息，参考指南！
				printf("[cli] read len return %d and errno is %d\n", read_res, errno);
				if(errno == EINTR) {
					printf("Errno: EINTR\n");
					if(sig_type == SIGINT) {
						printf("sig_type is SIGINT\n");
						return -1;
					}
					continue;//如果不是SIGINT中断，而是其他问题产生的中断，则继续循环，read数据
				}
				return -1;
			}
			//下面就是判断客户端正常退出
			if(read_res == 0){
				// printf("[srv] read data return 0.\n");
				return 0;
			}
			//不断的从服务套接字读取收到的字符数，加入到read_amnt统计变量中
			read_amnt += read_res;
			if(read_amnt == len) {
				break; //读完了，跳出循环
			}
			else if(read_amnt < len) {
				printf("[cli] Continue reading...\n");
				len_to_read = len - read_amnt;
			}
			else {
				printf("[cli] Read more than expected, quitting...\n");
				return -1;
			}
			
		}while(1);
		// ----------------

        printf("[echo_rep] %s\n", buf);		//打印字符串[echo_rep] ，注意格式，方括号后面有空格
		memset(&buf, 0, sizeof(buf));
		fflush(stdin);
    }
	return 0;//重要，定义函数正常退出的返回值
}

/*这是主函数入口，考虑到要求需要从命令行读入参数，因此要采用argc,argv的形式*/
int main(int argc, char* argv[])
{
	//定义结构体存储服务器端的地址信息，注意使用的是sockaddr_in
	struct sockaddr_in srv_addr;
	struct sockaddr_in cli_addr;
	int cli_addr_len;
	//定义套接字标识符
	int connfd;
	//要求必须有3个参数，第1个参数是命令（客户端程序名），第2个参数是服务器的IP地址，第3个参数是服务器端端口号
	//如果不是3个参数就给出错误提示信息
		//如果有错，返回0，结束主程序
	if(argc != 3) {
		printf("Usage:%s <IP> <PORT>\n", argv[0]);
		return 0;
	}


	struct sigaction sigact_pipe, old_sigact_pipe;	//定义2个信号结构体
	// sigact_pipe.sa_handler = sig_pipe;	//定义SIGPIPE信号的处理函数sig_pipe()，当SIGPIPE信号被捕获到时，进程调用这个函数
	sigemptyset(&sigact_pipe.sa_mask);	//信号集初始化为空
	sigact_pipe.sa_flags = 0;			//初始标志位初始化为0
	sigact_pipe.sa_flags |= SA_RESTART;	//设置受影响的慢系统调用重启
	sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);	//定义sigaction函数，详细见实验补充资料

	//创建套接字标识符，并根据参数初始化服务器的地址结构
	//创建套接字描述符
	connfd = socket(PF_INET, SOCK_STREAM, 0);
	//服务器的地址信息存储空间清零
	memset(&srv_addr, 0, sizeof(srv_addr));
	//服务器的地址信息存储结构体，指定为IPv4地址族
	srv_addr.sin_family = AF_INET;
	//将作为命令行参数的点分十进制数表示的IP地址转化为二进制表示
	inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);
	//将作为命令行参数的端口号转换为网络字节序，存储到地址结构体的元素中
	srv_addr.sin_port = htons(atoi(argv[2]));
	
	//客户端发起连接请求
	do{
	//这是一个死循环，除非终止条件满足：终止条件是
		//connect函数发起连接请求，第1个参数是套接字标识符，第2个参数是地址结构，第3个参数是地址结构的长度
		int conn_res = connect(connfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
		
		//判断connect函数返回值是否成功，成功返回值为0,不成功返回值为-1
		if(conn_res == 0){
		//如果按照成功来写
			char ip_str[20]={0};
			//根据指南要求，这里需要打印一句话：[cli] server（）is connected！
			//inet_ntop函数的功能是将二进制数据转换为字符串
			printf("[cli] server[%s:%d] is connected!\n",inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str)), ntohs(srv_addr.sin_port));
			//调用echo函数，只需将套接字标识符作为参数传递
			//如果是非0值，表示程序是非正常退出，中断
			if(!echo(connfd))
				break;
		}
		//如果connect函数创建没有成功，则重新发起连接请求，注意errno一定要考虑
		else if(conn_res == -1 && errno == EINTR){
			continue;
		}
	}while(1);
		
	//正常退出后，关闭客户端的套接字描述符
	close(connfd);
	//打印指南规定的提示信息
	printf("[cli] connfd is closed!\n");
	printf("[cli] client is exiting!\n");
	return 0;//必须的，函数正常退出返回值
}
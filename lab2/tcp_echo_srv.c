#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <assert.h>

#define BACKLOG 1024
#define bprintf(fp, format, ...) \
	if(fp == NULL){printf(format, ##__VA_ARGS__);} 	\
	else{printf(format, ##__VA_ARGS__);	\
			fprintf(fp, format, ##__VA_ARGS__);fflush(fp);}

int sig_type = 0, sig_to_exit = 0;
FILE * fp_res = NULL;

void sig_int(int signo) {	
	sig_type = signo;
	pid_t pid = getpid();
	bprintf(fp_res, "[srv](%d) SIGINT is coming!\n", pid);
	//如果服务器端，强制终止程序运行，则sig_to_exit = 1
    sig_to_exit = 1;
}
void sig_pipe(int signo) {	
	sig_type = signo;
	pid_t pid = getpid();
	bprintf(fp_res, "[srv](%d) SIGPIPE is coming!\n", pid);
}
void sig_chld(int signo) {
    sig_type = signo;
	pid_t pid = getpid(), pid_chld = 0;
    int stat; 
	bprintf(fp_res, "[srv](%d) SIGCHLD is coming!\n", pid);
	while ((pid_chld = waitpid(-1, &stat, WNOHANG)) > 0){
		//bprintf(fp_res, "[srv](%d) child process(%d) terminated.\n", pid, pid_chld);
	}
}
int install_sig_handlers(){
	int res = -1;
	struct sigaction sigact_pipe, old_sigact_pipe;
	sigact_pipe.sa_handler = sig_pipe;//sig_pipe()，信号处理函数
	sigact_pipe.sa_flags = 0;
	sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
	sigemptyset(&sigact_pipe.sa_mask);
	res = sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);
	//如果SIGPIPE的处理函数安装失败，则返回-1
	if(res)
		return -1;

	struct sigaction sigact_chld, old_sigact_chld;
    // sigact_chld.sa_handler = SIG_IGN; //建议采用SIG_IGN
	sigact_chld.sa_handler = sig_chld;
    sigact_chld.sa_flags = 0;
	sigact_chld.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
	sigemptyset(&sigact_chld.sa_mask);
    res = sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);
	//如果SIGCHLD的处理函数安装失败，则返回-2
	if(res)
		return -2;
	
	struct sigaction sigact_int, old_sigact_int;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;
	sigact_int.sa_handler = &sig_int;
    sigaction(SIGINT, &sigact_int, &old_sigact_int);
	//如果SIGINT的处理函数安装失败，则返回-3
	if(res)
		return -3;
	//三个信号都安装成功，返回0
	return 0;
}

/*
	返回：
		-1，未获取客户端PIN
		>1，有效的客户端PIN
*/
int echo_rep(int sockfd)
{
	//初始时，len_h（主机序的PDU的长度），pin_h（主机序的客户端子进程创建序号）
	int len_h = -1, len_n = -1;
	int pin_h = -1, pin_n = -1;
	int res = 0;
	char *buf = NULL;
	//当前的进程号
	pid_t pid = getpid();

	// 读取客户端PDU并执行echo回复
    do {
		// 读取客户端PIN（Process Index， 0-最大并发数），并创建记录文件
		do {
			//用read读取PIN（网络字节序）到pin_n中；返回值赋给res
			res = read(sockfd, &pin_n, sizeof(pin_n));
			if(res < 0){
				bprintf(fp_res, "[srv](%d) read pin_n return %d and errno is %d!\n", pid, res, errno);
				if(errno == EINTR){
					if(sig_type == SIGINT)
						return pin_h;
					continue;
				}
				return pin_h;
			}
			if(!res){
				return pin_h;
			}
			//如果读取PDU的PIN字段成功，转换为主机序
			//将pin_n字节序转换后存放到pin_h中
			pin_h = ntohl(pin_n);
			break;//跳出
		}while(1);

		// 读取客户端echo_rqt数据长度
		do{
			//用read读取客户端echo_rqt数据长度（网络字节序）到len_n中:返回值赋给res
			res = read(sockfd, &len_n, sizeof(len_n));
			
			if(res < 0){
				bprintf(fp_res, "[srv](%d) read len_n return %d and errno is %d\n", pid, res, errno);
				if(errno == EINTR){
					if(sig_type == SIGINT)
						return len_h;
					continue;
				}
				return len_h;
			}
			if(!res){
				return len_h;
			}
			//将len_n字节序转换后存放到len_h中
			len_h = ntohl(len_n);
			break;
		}while(1);
		
		// 读取客户端echo_rqt数据
		//read_amnt：表示已经读取的字节数，len_to_read：长度允许读取的字节数
		//初始时，len_to_read就是收到的PDU的LEN值
		int read_amnt = 0, len_to_read = len_h;
		buf = (char*)malloc(len_h * sizeof(char)+8); // 预留PID与数据长度的存储空间，为后续回传做准备
		do{
			//使用read读取客户端数据，返回值赋给res。注意read第2、3个参数，即每次存放的缓冲区的首地址及所需读取的长度如何设定
			res = read(sockfd, &buf[read_amnt]+8, len_to_read);
			
			if(res < 0){
				bprintf(fp_res, "[srv](%d) read data return %d and errno is %d,\n", pid, res, errno);
				if(errno == EINTR){
					if(sig_type == SIGINT){
						free(buf);
						return pin_h;
					}
					continue;
				}
				free(buf);
				return pin_h;
			}
			if(!res){//什么都没读到
				free(buf);
				return pin_h;
			}

			//此处计算read_amnt及len_to_read的值，注意考虑已读完和未读完两种情况，以及其它情况（此时直接用上面的代码操作，free(buf),并 return pin_h）
			read_amnt += res;
			if(read_amnt == len_h) {
				break;//退出循环
			}
			else if(read_amnt < len_h) {
				len_to_read = len_h - read_amnt;
			}
			else {
				free(buf);
				return pin_h;
			}
		}while(1);

		// 解析客户端echo_rqt数据并写入res文件；
		bprintf(fp_res, "[echo_rqt](%d) %s\n", pid, buf+8);
		// 将客户端PIN写入PDU缓存（网络字节序）
		memcpy(buf, &pin_n, 4);
		// 将echo_rep数据长度写入PDU缓存（网络字节序）
		memcpy(buf+4, &len_n, 4);
		
		// 发送echo_rep数据:
		write(sockfd, buf, len_h+8);
        free(buf);
    }while(1);
	return pin_h;
}

//主程序入口，命令行参数：./tcp_echo_srv 192.168.2.2 5000
int main(int argc, char* argv[])
{
	// 基于argc简单判断命令行指令输入是否正确；
	if(argc != 3)
	{
		//输出正确格式的提示信息
		printf("Usage:%s <IP> <PORT>\n", argv[0]);
		return -1;
	}

 	// 获取当前进程PID，用于后续父进程信息打印；
	pid_t pid = getpid();
	// 定义IP地址字符串（点分十进制）缓存，用于后续IP地址转换；
	//用于IP地址转换，初始化清0
	char ip_str[20] = {0};
	// 定义res文件名称缓存，用于后续文档名称构建；
	//初始化清0
	char fn_res[20] = {0};
	// 定义结果变量，用于承接后续系统调用返回结果；
	int res = -1;

	// 安装信号处理器，包括SIGPIPE，SIGCHLD以及SIGITN；
	//install_sig_handlers函数正常退出，返回0，异常退出分别返回-1，-2，-3
	res = install_sig_handlers();
	
	//如果信号安装有问题，则打印错误信息，并退出主函数
	if(res){
		printf("[srv](%d) parent exit failed to install signal handlers!\n", pid);
		return res;
	}

	// 打开文件"stu_srv_res_p.txt"，用于后续父进程信息记录；
	fp_res = fopen("stu_srv_res_p.txt", "wb");
	if(!fp_res){
		printf("[srv](%d) failed to open file \"stu_srv_res_p.txt\"!\n", pid);
		return res;
	}
	//将文件被打开的提示信息打印到stdout
	printf("[srv](%d) stu_srv_res_p.txt is opened!\n", pid);

    // 定义服务器Socket地址srv_addr，以及客户端Socket地址cli_addr；
	struct sockaddr_in srv_addr, cli_addr;
	
    // 定义客户端Socket地址长度cli_addr_len（类型为socklen_t）；
	socklen_t cli_addr_len;
	// 定义Socket监听描述符listenfd，以及Socket连接描述符connfd；
	int listenfd, connfd;

	// 初始化服务器Socket地址srv_addr，其中会用到argv[1]、argv[2]
	/* IP地址转换推荐使用inet_pton()；端口地址转换推荐使用atoi(); */
	bzero(&srv_addr, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	srv_addr.sin_port = htons(atoi(argv[2]));
	
	// 按题设要求打印服务器端地址server[ip:port]，推荐使用inet_ntop();
	inet_ntop(AF_INET, &srv_addr.sin_addr, ip_str, sizeof(ip_str));
	//[srv](进程号) server[IP:PORT] is initializing! 服务器初始化完成
	bprintf(fp_res, "[srv](%d) server[%s:%d] is initializing!\n", pid, ip_str, (int)ntohs(srv_addr.sin_port));
	
	// 获取Socket监听描述符: listenfd = socket(x,x,x);
	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	//考虑监听套接字创建失败
	if(listenfd == -1)
		return listenfd;
	
	// 绑定服务器Socket地址: res = bind(x,x,x);成功返回0，失败返回-1
	res = bind(listenfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
	
	//bind失败，退出主程序
	if(res)
		return res;

	// 开启服务监听: res = listen(x,x);
	res = -9;
	res = listen(listenfd, BACKLOG);
	
	if(res)//res为非0
		printf("[srv](%d) listen() returned %d\n", pid, res);
	else if(res == 0)//listen命令设置被动监听模式成功
		printf("[srv](%d) listen() returned 0\n",pid);
	
    // 开启accpet()主循环，直至sig_to_exit指示服务器退出；sig_to_exit = 0;
	while(!sig_to_exit)
	{
		// 获取cli_addr长度，执行accept()：connfd = accept(x,x,x);
		cli_addr_len = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addr_len);
		// 以下代码紧跟accept()，用于判断accpet()是否因SIG_INT信号退出（本案例中只关心SIGINT）；也可以不做此判断，直接执行 connfd<0 时continue，因为此时sig_to_exit已经指明需要退出accept()主循环，两种方式二选一即可。
		if(connfd == -1 && errno == EINTR){
			if(sig_type == SIGINT)
				break;//如果accept调用失败，并且是因为SIGINT，强制终止程序运行，退出主进程
			continue;//否则，如果是其他原因导致的accept函数调用失败，则继续阻塞，等待其他TCP连接请求
		}

		//二进制表示的客户端IP地址转换成点分十进制的IP地址字符串
		inet_ntop(AF_INET, &cli_addr.sin_addr, ip_str, sizeof(ip_str));
		//按题设要求打印客户端地址：[srv](进程号) client[IP:PORT] is accepted!
		bprintf(fp_res, "[srv](%d) client[%s:%d] is accepted!\n", pid, ip_str, (int)ntohs(cli_addr.sin_port));
		//将缓冲区的数据刷入文件
		fflush(fp_res);

		//fork子进程对接客户端进行通信
		if(!fork()){//子进程
			//获取当前子进程PID，用于后续子进程信息打印
			pid = getpid();
			//打开res文件，命名，子进程退出前根据echo_rep()返回的PIN值对文件更名
			sprintf(fn_res, "stu_srv_res_%d.txt", pid);
			fp_res = fopen(fn_res, "wb");// Write only， append at the tail. Open or create a binary file;
			if(!fp_res){
				printf("[srv](%d) child exits, failed to open file \"stu_srv_res_%d.txt\"!\n", pid, pid);
				exit(-1);
			}
			//文件打开成功，向记录文件中写入[srv](进程号) child process is created!
			bprintf(fp_res, "[srv](%d) child process is created!\n", pid);

			//子进程中，只是为客户端传输数据，因此关闭监听描述符
			//关闭监听描述符，打印提示信息到文件中
			close(listenfd);
			bprintf(fp_res, "[srv](%d) listenfd is closed!\n", pid);
			
			//执行业务函数echo_rep（返回客户端PIN到变量pin中，以便用于后面的更名操作）
			int pin = echo_rep(connfd);
			if(pin < 0) {
				bprintf(fp_res, "[srv](%d) child exits, client PIN returned by echo_rqt() error!\n", pid);
				exit(-1);
			}

			//更名子进程res文件(PIN替换PID)
			char fn_res_n[20]= {0};
			//建立记录文件需要更名的新名字， stu_srv_res_客户端进程序号.txt
			sprintf(fn_res_n, "stu_srv_res_%d.txt", pin);
			if(!rename(fn_res, fn_res_n)){
				//更名成功，将日志写入文件
				bprintf(fp_res, "[srv](%d) res file rename done!\n", pid);
			}
			else {
				//更名失败
				bprintf(fp_res, "[srv](%d) child exits, res file rename failed!\n", pid);
			}

			//关闭连接描述符，输出信息到res文件中
			close(connfd);
			bprintf(fp_res, "[srv](%d) connfd is closed!\n", pid);
			bprintf(fp_res, "[srv](%d) child process is going to exit!\n", pid);

			//关闭子进程res文件,并按指导书要求打印提示信息到stdout,然后exit
			if(!fclose(fp_res))
				printf("[srv](%d) stu_srv_res_%d.txt is closed!\n", pid, pin);
			
			//退出子进程
			exit(1);
		}
		else{// 父进程		
			close(connfd);// 关闭服务套接字描述符，主进程只负责监听
			continue;// 继续accept()，处理下一个请求
		}
	}

	//只有当sig_to_exit=1（说明服务器端采用了强制终止），while循环退出，关闭监听描述符
	close(listenfd);
	bprintf(fp_res, "[srv](%d) listenfd is closed!\n", pid);
	bprintf(fp_res, "[srv](%d) parent process is going to exit!\n", pid);
	
	// 关闭父进程res文件
	if(!fclose(fp_res))
		printf("[srv](%d) stu_srv_res_p.txt is closed!\n", pid);
	return 0;
}
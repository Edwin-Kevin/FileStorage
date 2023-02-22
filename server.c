#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<dirent.h>

#define PORT 8888
#define MAXLINE 1024

int server_fd,new_socket,valread; //server_fd:监听套接字描述符; 
	                              //new_socket:连接套接字描述符
	                              //valread:客户端发送的数据

//输入CTRL+C时关闭套接字并退出程序
void sig_handler(int sig){
	switch(sig){
		case SIGINT:
			printf("\nDetect SIGINT signal, closing sockets ...\n");
			close(new_socket);
			close(server_fd);
			printf("Sockets closed. Process exit.\n");
			exit(0);
			break;
		default:
			printf("Invalid input signal.\n");
	}
}

int main(){
	int filefd,filelen,filecnt;
	struct sockaddr_in address;       //服务端地址
	struct sockaddr_in remote_addr;   //客户端地址
	int addrlen = sizeof(address);
	
	char buffer[MAXLINE] = {0};
	char filename[MAXLINE] = {0};
	char filebuf[MAXLINE] = {0};
	char *hello = "Hello from server";
	
	//注册信号处理函数
	signal(SIGINT, sig_handler);
	
	//创建socket文件描述符
	if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		perror("socket");
		exit(1);
	}
	
	//绑定地址和端口
	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&(address.sin_zero),8);
	
	if(bind(server_fd,(struct sockaddr *)&address, sizeof(struct sockaddr)) < 0){
		perror("bind");
		exit(1);
	}
	
	//监听端口
	if(listen(server_fd,5) < 0){
		perror("listen");
		exit(1);
	}
	while(1){
		//等待客户端连接
		if((new_socket = accept(server_fd,(struct sockaddr *)&remote_addr,&addrlen)) < 0){
			perror("accept");
			exit(1);
		}
		
		//打印客户端地址
		printf("Received a connection from %s\n",inet_ntoa(remote_addr.sin_addr));
			
		//向客户端发送欢迎信息
		if(send(new_socket,"Hello, you are connected!\n",26,0) == -1){
			perror("send");
			close(new_socket);
			exit(2);
		}
			
		valread  = read(new_socket,buffer,MAXLINE);  //接收客户端欢迎词并打印
		printf("%s\n",buffer);
		memset(buffer, 0, MAXLINE);

		while(1){
			//从客户端读取操作指令
			valread  = read(new_socket,buffer,MAXLINE);
			printf("%s\n",buffer);
			send(new_socket,"COMMANDOK",9,0);    //告诉客户端，服务端收到了操作指令
			printf("COMMANDOK\n");
		
			//解析客户端的请求
			if(strncmp(buffer,"UPLOAD",6) == 0){
				//处理上传文件请求代码
				printf("Begin to upload file\n");
				memset(buffer,0,MAXLINE);
				
				//获取文件名
				read(new_socket,buffer,MAXLINE);
				//客户端没找到文件
				if(strncmp(buffer,"SKIP",4) == 0){
					printf("Failed to fetch filename.\n");
					continue;
				}
				
				//创建新文件
				filefd = open(buffer,O_WRONLY | O_CREAT | O_TRUNC,0757);
				//创建文件失败
				if(filefd == -1){
					printf("Failed to create new file!\n");
					send(new_socket,"fail",4,0);
					continue;
				}
				if(write(new_socket,"FILEOK",6) < 0){   //发送创建成功的回应
					continue;
				}
				printf("Success to open file.\n");
				
				//开始写入文件
				memset(filebuf,0,MAXLINE);
				while((filecnt = read(new_socket,filebuf,MAXLINE)) > 0){
					//上传完毕的检测条件
					if(strncmp(filebuf, "exit", 4) == 0){
						printf("Upload completed!\n");
						break;
					}
					//写入文件失败时，发送退出指令
					if(write(filefd,filebuf,filecnt) != filecnt){
						write(new_socket,"fail",4);
						printf("File transfer failed!\n");
						break;
					}
					//确认接收到了上一批数据
					write(new_socket,"DATAOK",6);
					memset(filebuf,0,MAXLINE);
				}
				close(filefd);
				memset(filebuf,0,MAXLINE);
			}
			else if(strncmp(buffer,"DOWNLOAD",8) == 0){
				memset(buffer,0,MAXLINE);
				//处理下载文件请求
				read(new_socket,buffer,MAXLINE);
				//客户端打开文件
				filefd = open(buffer,O_RDONLY);
				//打开文件失败，退出
				if(filefd == -1){
					send(new_socket,"FAIL",4,0);
					printf("Failed to open file!\n");
					perror("open");
					continue;
				}
				//打开文件成功
				send(new_socket,"FILEOK",6,0);
				read(new_socket,buffer,MAXLINE);
				//客户端创建文件失败
				if(strncmp(buffer,"SKIP",4) == 0){
					printf("Client failed to open file!\n");
					continue;
				}
				//客户端创建文件成功
				if(strncmp(buffer,"FILEOK",6) == 0){
					memset(filebuf,0,MAXLINE);
					while((filecnt = read(filefd,filebuf,MAXLINE)) > 0){
						//发送数据失败
						if(write(new_socket,filebuf,filecnt) != filecnt){
							printf("Send file failed!\n");
							break;
						}
						//判断上次数据是否发送成功
						read(new_socket,buffer,MAXLINE);
						if(strncmp(buffer,"DATAOK",6) != 0){
							printf("Failed to transer file.\n");
							break;
						}
						memset(filebuf,0,MAXLINE);
						memset(buffer,0,MAXLINE);
					}
					//数据发送完毕，发送退出信号
					send(new_socket,"exit",4,0);
					close(filefd);
					printf("Download completed!\n");
				}
				else{
					printf("Client failed to create file!\n");
					close(filefd);
					continue;
				}
			}
			else if(strncmp(buffer,"LIST",4) == 0){
				//处理列表请求
				DIR *mydir = NULL;
				struct dirent *myitem = NULL;
				memset(buffer,0,MAXLINE);
				read(new_socket,buffer,MAXLINE);
				//客户端准备好接收列表
				if(strncmp(buffer,"LISTOK",6) != 0){
					continue;
				}
				//循环读取列表中的文件
				if((mydir = opendir(".")) == NULL){
					perror("opendir");
					exit(3);
				}
				//依次发送文件名
				while((myitem = readdir(mydir)) != NULL){
					memset(buffer,0,MAXLINE);
					if(sprintf(buffer,myitem -> d_name,MAXLINE) < 0){
						printf("Sprintf error!\n");
						exit(3);
					}
					if(write(new_socket,buffer,MAXLINE) < 0){
						perror("write");
						exit(1);
					}
					//printf("%s",buffer);
					//printf("1\n");
				}
				//列表发送完毕
				write(new_socket,"exit",4);
				closedir(mydir);
			}
			else if(strncmp(buffer,"DELETE",6) == 0){
				//处理删除请求
				memset(buffer,0,MAXLINE);
				//读取要删除的文件名
				read(new_socket,buffer,MAXLINE);
				if(remove(buffer) == 0){
					//删除成功
					printf("Removed %s.\n",buffer);
					write(new_socket,"REMOVEOK",8);
				}
				else{
					//没找到删除的文件
					write(new_socket,"NOFILE",6);
				}
			}
			else if(strncmp(buffer,"EXIT",4) == 0){
				//客户端主动断开连接
				printf("Disconnecting from client %s!\n",inet_ntoa(remote_addr.sin_addr));
				break;
			}
			memset(buffer,0,MAXLINE);
		}
	
		//关闭连接套接字
		close(new_socket);
		printf("Connection terminated!\n");
	}
	return 0;
}
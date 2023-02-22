#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>

#define PORT 8888
#define MAXLINE 1024

int main(int argc,char *argv[]){
	int sockfd,valread,filecnt;
	struct sockaddr_in serv_addr;
	char buffer[MAXLINE] = {0};
	char filename[MAXLINE] = {0};
	char filebuf[MAXLINE] = {0};
	int filefd,filelen;
	char *hello = "Hello from client.";
	
	//创建套接字
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("socket");
		exit(1);
	}
	
	//设置服务器地址和端口
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&(serv_addr.sin_zero),8);
	
	//向服务器端发起连接
	if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr)) < 0){
		perror("connect");
		exit(1);
	}
	
	//接收服务器端发来的欢迎词并显示
	if((valread = recv(sockfd,buffer,MAXLINE,0)) < 0){
		perror("recv");
		exit(1);
	}
	buffer[valread] = '\0';  //设置字符串结束符
	printf("Received:%s\n",buffer);
	
	//向服务端发送欢迎词
	if(send(sockfd,hello,strlen(hello),0) == -1){
		perror("send");
		close(sockfd);
		exit(2);
	}
	
	while(1){
		//读取用户输入的命令并发送
		printf("COMMAND:UPLOAD|DOWNLOAD|LIST|DELETE|MOVE|EXIT\n");
		fgets(buffer,MAXLINE,stdin);      //读取用户输入的命令
		buffer[strlen(buffer) - 1] = '\0';
		
		/*此处开始上传部分逻辑*/
		if(strncmp(buffer,"UPLOAD",6) == 0){
			//上传部分
			if(send(sockfd,"UPLOAD",6,0) == -1){
				perror("send");
				exit(2);
			}
			memset(buffer,0,MAXLINE);
			recv(sockfd,buffer,MAXLINE,0);
			if(strncmp(buffer,"COMMANDOK",9) == 0){              //服务端收到了操作指令
				memset(buffer,0,MAXLINE);
				printf("Please enter file path and name:\n");   //读取文件路径
				fgets(filename,MAXLINE,stdin);
				filename[strlen(filename) - 1] = '\0';
				filefd = open(filename,O_RDONLY);
				//打开文件失败，退出
				if(filefd == -1){
					printf("Failed to open file!\n");
					write(sockfd,"SKIP",4);
					perror("open");
					continue;
				}
				send(sockfd,filename,strlen(filename),0);  //发送文件名称
				if(recv(sockfd,buffer,MAXLINE,0) <= 0){
					continue;
				}
				
				//服务端成功创建文件描述符
				if(strncmp(buffer,"FILEOK",6) == 0){
					memset(buffer,0,MAXLINE);
					//开始循环读取文件内容并发送
					while((filecnt = read(filefd,filebuf,MAXLINE)) > 0){
						if(write(sockfd,filebuf,filecnt) != filecnt){
							printf("Send file failed.\n");
							break;
						}
						//判断上次发送数据是否成功
						read(sockfd,buffer,MAXLINE);
						if(strncmp(buffer,"DATAOK",6) != 0){
							send(sockfd,"SKIP",4,0);
							printf("Failed to transer file.\n");
							break;
						}
						memset(buffer,0,MAXLINE);
						memset(filebuf,0,MAXLINE);
					}
					send(sockfd,"exit",4,0);
					close(filefd);
					printf("Upload completed!\n");
				}
				else{
					printf("Server cannot create file.\n");
					close(filefd);
				}
			}
			else
				send(sockfd,"SKIP",4,0);
		}
		
		/*此处开始下载部分逻辑*/
		else if(strncmp(buffer,"DOWNLOAD",8) == 0){
			if(send(sockfd,"DOWNLOAD",8,0) == -1){
				perror("send");
				exit(2);
			}
			
			memset(buffer,0,MAXLINE);
			recv(sockfd,buffer,MAXLINE,0);
			if(strncmp(buffer,"COMMANDOK",9) == 0){
				memset(buffer,0,MAXLINE);
				printf("Please enter file path and name:\n");
				fgets(filename,MAXLINE,stdin);
				filename[strlen(filename) - 1] = '\0';
				send(sockfd,filename,strlen(filename),0);
				recv(sockfd,buffer,MAXLINE,0);
				//服务器找到了文件并成功打开
				if(strncmp(buffer,"FILEOK",6) == 0){
					filefd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0757);
					//本地创建文件失败
					if(filefd < 0){
						printf("Failed to create file!\n");
						send(sockfd,"SKIP",4,0);
						perror("open");
						continue;
					}
					send(sockfd,"FILEOK",6,0);
					printf("Success to create file!\n");
					//清空文件缓存，准备写入
					memset(filebuf,0,MAXLINE);
					while((filecnt = read(sockfd,filebuf,MAXLINE)) > 0){
						if(strncmp(filebuf,"exit",4) == 0){
							printf("Download completed!\n");
							break;
						}
						if(write(filefd,filebuf,filecnt) != filecnt){
							write(sockfd,"fail",4);
							printf("File transfer failed!\n");
							break;
						}
						write(sockfd,"DATAOK",6);
						memset(filebuf,0,MAXLINE);
					}
					close(filefd);
					memset(filebuf,0,MAXLINE);
				}
			}
		}
		/*这里开始列表部分*/
		else if(strncmp(buffer,"LIST",4) == 0){
			//列出文件列表
		}
		else if(strncmp(buffer,"DELETE",6) == 0){
			//删除指定文件
		}
		else if(strncmp(buffer,"MOVE",4) == 0){
			//移动指定文件
		}
		else if(strncmp(buffer,"EXIT",4) == 0){
			if(send(sockfd,"EXIT",6,0) == -1){
				perror("send");
				exit(2);
			}
			break;
		}
		memset(buffer,0,MAXLINE);
	}
	
	close(sockfd);
	return 0;
}
	
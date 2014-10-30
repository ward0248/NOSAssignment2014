/*     
  Sample solution for NOS 2014 assignment: implement a simple multi-threaded 
  IRC-like chat service.
 
  (C) Gail Ward 2014.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, US.

  Sections of this code are based on work by Paul Gardner-Stephen 2014 
  this can be found at https://github.com/gardners/nos2014assignment.git
  Any code contained at the above address is therefore attributed entirely 
  to Paul Gardner-Stephen 2014.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>


/*
  variables
 */
int connection_count = 0;


/*
  struct by Paul Gardner-Stephen
 */
struct client_thread {
  pthread_t thread;
  int thread_id;
  int fd;
  char nickname[32];
  int state;
  int user_command_seen;
  int user_has_registered;
  time_t timeout;
  char line[1024];
  int line_len;
  int next_message;
};

pthread_rwlock_t message_log_lock;

int create_listen_socket(int port)
{
  int sock = socket(AF_INET,SOCK_STREAM,0);
  if (sock==-1) return -1;
  int on=1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) 
== -1) {
    close(sock); return -1;
  }
  if (ioctl(sock, FIONBIO, (char *)&on) == -1) {
    close(sock); return -1;
  }
  
  /* Bind it to the next port we want to try. */
  struct sockaddr_in address;
  bzero((char *) &address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(sock, (struct sockaddr *) &address, sizeof(address)) == -1) {
    close(sock); return -1;
  } 
  if (listen(sock, 20) != -1) return sock;
  close(sock);
  return -1;
}

int accept_incoming(int sock)
{
  struct sockaddr addr;
  unsigned int addr_len = sizeof addr;
  int asock;
  if ((asock = accept(sock, &addr, &addr_len)) != -1) {
    return asock;
  }
  return -1;
}

/*
  read_from_socket copied from test.c 
  All attribution to Paul Gardner-Stephen
 */
int read_from_socket(int sock, unsigned char *buffer, int *count, int buffer_size, int timeout){

  fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, NULL)| O_NONBLOCK);

  int t=time(0) + timeout;
  if(*count>=buffer_size) return 0;
  int r=read(sock, &buffer[*count], buffer_size-*count);
  while(r!=0){
    if(r>0){
	(*count)+=r;
	break;
      }
      r=read(sock, &buffer[*count], buffer_size-*count);
      if(r==-1&&errno!=EAGAIN){
	perror("read() returned error.Stopping reading from socket.");
	return -1;
      }else usleep(100000);
      //timeout after a few seconds of nothing
      if(time(0)>=t) break;
  }
    buffer[*count]=0;
    return 0;

}

/*
  this should handle all incoming connections
  first welcomes user
  closes connection on timeout of 5 seconds
  if receives JOIN or PRIVMSG before registered then informs of error
 */
int doing_connections(int fd){

  connection_count++;
  printf("Connection %d  seen \n", connection_count);
  char msgbuff[1024];
  snprintf(msgbuff, 1024, ":dancingcats.com 020 * Hello and welcome to the server.\n");
  write(fd, msgbuff, strlen(msgbuff));

  char unknownbuff[8192];
  int length=0;
  int r = read_from_socket(fd,(unsigned char *) unknownbuff, &length, 8192, 5);
  
  if(length==0){
    snprintf(msgbuff, 1024, "ERROR:Closing Link: Dancing cats timed out (Longer than 5 secs)\n");
    write(fd, msgbuff, strlen(msgbuff));
    close(fd);
  } 
  
  char nick[1024], channel_name[1024], user[1024];
  
  r = sscanf(unknownbuff, "JOIN %s\n", channel_name);
  if(strlen(nick) == 0){
    snprintf(msgbuff, 1024, ":dancingcats.com 241 * JOIN command sent before registration\n");
    write(fd, msgbuff, strlen(msgbuff));
  }
  
  r = sscanf(unknownbuff, "PRIVMSG %s\n", channel_name);
  if(strlen(nick) == 0){
    snprintf(msgbuff, 1024, ":dancingcats.com 241 * PRIVMSG command sent before registration\n");
    write(fd, msgbuff, strlen(msgbuff));
  }
  
  r = sscanf(unknownbuff, "NICK: %s\n", nick);
  if(r != 1){
      snprintf(msgbuff, 1024, "ERROR: nick not resolved");
      write(fd, msgbuff, strlen(msgbuff));
}
  if(strlen(nick) != 0){
//    snprintf(msgbuff, 1024, ":dancingcats.com 241 %s Greetings, welcome 
//to Clefable Cottage\n",  nick);
//    write(fd, msgbuff, strlen(msgbuff));
  }

  r = sscanf(unknownbuff, "USER: %s\n", user);
  if(strlen(nick) == 0){
     snprintf(msgbuff, 1024, "ERROR: nick not resolved");
     write(fd, msgbuff, strlen(msgbuff));}
  if(strlen(nick) != 0){
     snprintf(msgbuff, 1024, ":dancingcats.com 001 %s Greetings, Welcome to Clefable Cottage\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 002 %s Poke Centre is to the left\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 003 %s Gym is to the right\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 004 %s Check out the Ghost Tower\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 253 %s There are 50 :unknown connections\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 254 %s There are 3 :channels formed\n", nick);
     write(fd, msgbuff, strlen(msgbuff));
     snprintf(msgbuff, 1024, ":dancingcats.com 255 %s There are 1004 clients and 1 server\n", nick);
}

  close(fd);
  return 0;
}


int main(int argc,char **argv)
{
  signal(SIGPIPE, SIG_IGN);
  if (argc!=2) {
  fprintf(stderr,"usage: sample <tcp port>\n");
  exit(-1);
  }
  
  int master_socket = create_listen_socket(atoi(argv[1]));
  
  fcntl(master_socket,F_SETFL,fcntl(master_socket, F_GETFL, NULL)&(~O_NONBLOCK));  
  
  while(1) {
    int client_sock = accept_incoming(master_socket);
    if (client_sock!=-1) {
      // Got connection -- do something with it.
      doing_connections(client_sock);

    }
  }
}

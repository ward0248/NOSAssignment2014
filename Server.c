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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

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

int doing_connections(int fd){

  connection_count++;
  printf("Connection %d  seen \n", connection_count);
  char msgbuff[1024];
  snprintf(msgbuff, 1024, ":dancingcats.com 020 * Hello and welcome to the server.\n");
  write(fd, msgbuff, strlen(msgbuff));
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

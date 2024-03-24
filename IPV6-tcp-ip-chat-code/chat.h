//#include "unp.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>	/* inet(3) functions */
#include <sys/stat.h>	/* for S_xxx file mode constants */
#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <sys/time.h>	/* timeval{} for select() */
#include <time.h>		/* timespec{} for pselect() */

/*#include <readline/readline.h>  //ler linhas 
#include <readline/history.h>
*/
#include <signal.h> // control-z ....
#include <unistd.h> // exit()
#include <fcntl.h> // open

#include <stdlib.h> //calloc

#include <unistd.h>  // função read (read_fd)

#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))

#define	MAXLINE		4096	/* max text line length */
#define SIZE_NICK 10
#define SIZE_SALA 10
#define MAX_SALAS 7
#define FILE_D  int 
#define BYTE unsigned char
#define	SA	struct sockaddr
#define	LISTENQ		1024	/* 2nd argument to listen() */


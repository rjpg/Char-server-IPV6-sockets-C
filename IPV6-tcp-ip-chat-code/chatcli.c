#include	"chat.h"

/***************************************************************
 * Variaveis globais                                           *
 ***************************************************************/
// variaveis relativas à sokcet
int sockfd;
struct sockaddr_in servaddr;
// 
char nick[SIZE_NICK]="NULL";
char sendline[MAXLINE], recline[MAXLINE];
int indexw=0;
char file_send[255];
char file_rec[255];
char nick_recf[SIZE_NICK];
/***************************************************************
 * Envia um END e sai                                          *
 ***************************************************************/
void logout ()
{
  write(sockfd, "END\n", 4);
  printf("END\n");
  close(sockfd);
  exit (1);
}


/***************************************************************
 * lê linhas de um fd RET: 0-eof -1-erro 1-leu\n 2-leu sem \n  *
 ***************************************************************/
int read_fd(FILE_D fd,char  *vptr, int maxlen)
{
  int	n,i;
  char  *c=vptr;
  if ((n=read(fd, c, maxlen))!=0) // le n char de fd
    {
      if (n==-1)               
	return -1; // erro de leitura
      for(i=0;i<n;i++)                 //procura \n
	if(c[i]=='\n')                 // encontra: fim da msg
	  {
	    c[i+1]='\0';               // marcar o fim
	    return 1;  // leu com \n
	  }
      c[i+1]='\0';              // não apanho \n  marca o fim da msg
      if (i==n)
	return 2; //leu sem \n
    }
  else
    return 0;  // eof fechou a ligaçao 

  return -1; // nunca executa este return ...
}

/***************************************************************
 * aumenta dest com mais e ret o numero de char aumentados     *
 ***************************************************************/
int mais_char(char *dest,char *mais)
{
  strcat(dest,mais);
  return strlen(mais);
}

int run_snd_file(char* ip_aux,int porto)
{
  //int up;
  /* decla
     fork
     open
     sock
     conenct
     x=read f
//     ver se escreveu tudo

     write s
     close*/
  int	sockfd2;
  struct sockaddr_in servaddr2;
  int file;
  char buff[MAXLINE];
  int nchar;
  int x;

  if((x=fork())==-1)
    {
      printf("ERRO:CLIENT: forck não conseguido\n");
      return -1;
    }
  else if(x==0) //filho
    {
      if((file=open(file_send,O_RDONLY))==-1)
	{
	  printf("ERRO:CLIENT: erro de abertura de ficheiro %s para leitura\n",file_send);
	  return -1;
	}
      if((sockfd2 = socket(AF_INET, SOCK_STREAM, 0) ) < 0)
	{
	  printf("ERRO:CLIENT: erro de soket para upload de %s\n",file_send);
	  return -1;
	}
      bzero(&servaddr2, sizeof(servaddr2)); //iniciar 
      servaddr2.sin_family = AF_INET;
      servaddr2.sin_port = htons(porto);
      inet_pton(AF_INET, ip_aux, &servaddr.sin_addr);
      if(connect(sockfd2, (SA *) &servaddr2, sizeof(servaddr2))==-1) //conectar
	{
	  printf("ERRO:CLIENT: Servidor para upload não activo\n"); //erro
	  return -1;
	}
      for(;;)
	{
	  nchar=read(file,buff,10);
	  if(nchar==0) break;
	  write(sockfd2,buff,nchar);
	}
      printf("REM:RECFILE: fim de upload: %s\n",file_send);
      close (file);
      close (sockfd2);
      exit(0);
    }
  else //pai
    {   
    }
  return 0;
}
  
   
int inst_rec_file(char * file,char * nick)
{
  int x,randport,clilen,nchar;
  int down;
  int sockfd2,listenfd;
  struct sockaddr_in servaddr2,cliaddr2;
  char buff[MAXLINE],ipaux[INET_ADDRSTRLEN];
  
  if((down=open(file,O_WRONLY | O_CREAT | O_TRUNC,0600))==-1)
    {
      printf("ERRO:CLIENT: erro de abertura de ficheiro %s para escrita\n",file);
      return -1;
    }

  listenfd=socket(AF_INET,SOCK_STREAM,0);
  
  bzero(&servaddr2, sizeof(servaddr2));
  servaddr2.sin_family = AF_INET;
  servaddr2.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr2.sin_port = htons(0);
  
  if (bind(listenfd,(SA *) &servaddr2,sizeof(servaddr2))==-1) //por à escuta
    {printf("ERRO:CLIENT: bind error\n");return -1;}       //erro

  listen(listenfd,LISTENQ);

 // printf("depois do listen\n");
  if((x=fork())==-1)
    {
      printf("ERRO:CLIENT: forck não conseguido\n");
      return -1;
    }
  else if(x==0) //filho
    {
      clilen=sizeof(cliaddr2);
      sockfd2=accept(listenfd,(SA *) &cliaddr2,&clilen); //retorna o fd
      for(;;)
	{
	  nchar=read(sockfd2,buff,MAXLINE);
	  if(nchar==0) break;
	  write(down,buff,nchar);
	}
      printf("REM:RECFILE: fim de download: %s\n",file);
      close (down);
      close (sockfd2);
      exit(0);
    }
  else //pai
    {
      clilen=sizeof(servaddr2);
      getsockname(listenfd,(SA *) &servaddr2,&clilen);
      randport=ntohs(servaddr2.sin_port);
      sprintf(buff,"RECFILE:%s:%s:%d\n",nick,
	      inet_ntop(AF_INET,&servaddr.sin_addr,ipaux,sizeof(ipaux)),randport);
   //   printf("sadjk:%s",buff);
      write(sockfd, buff, strlen(buff));// vai a linha toda 
      close(listenfd);
    }
  return 0;
}
/***************************************************************
 * processar msg recebidas                                     *
 ***************************************************************/
int proc_srv_msg(char* msg)
{
  int i,x;
  char *caux,c,strpars[MAXLINE];
  char ip_aux[INET_ADDRSTRLEN],port_aux[5];
  int porto;
  for(i=0;(c=msg[i])!=':' && msg[i]!='\0';i++); //sai do ciclo com c==lerlinha[i]==':' ou '\0'
  
  msg[i]='\0'; //partir a srt (caso c==':')
  for(caux=msg;*caux==' ';caux++); //tirar os espaços iniciais
  strcpy(strpars,caux); //tirar até ':' (até ao fim)
  //printf("strpars:%s tamanho:%d indexorig:%d\n",strpars,strlen(strpars),i);
  msg[i]=c;  //voltar a juntar lerlinha original
  
  if(strncmp(strpars,"SNDFILE",7)==0)
    {
      x=i=strlen(msg);
      while (msg[x]!=':') x--; // encontrar os ':'
      while (msg[i]=='\n' || msg[i]=='\0' || msg[i]==' ') i--;//ver o ultimo char valido
      strncpy(file_rec,msg+x+1,i-x);
      //printf("file:%s\n",file_rec);
      i=(x-1);
      while (msg[i]!=':') i--;i--;while (msg[i]!=':') i--; // encontrar sala:nick
      i++;
      strncpy(nick_recf,msg+i,x-i);
      printf("nick:%s\n",nick_recf);
      inst_rec_file(file_rec,nick_recf);
    }
  
  if(strncmp(strpars,"RECFILE",7)==0)
    {
      x=i=strlen(msg);
      while (msg[x]!=':') x--; // encontrar os ':'
      while (msg[i]=='\n' || msg[i]=='\0' || msg[i]==' ') i--;//ver o ultimo char valido
      strncpy(port_aux,msg+x+1,i-x);
      sscanf(port_aux,"%d",&porto);
      //printf("porto:%d\n",porto);
      i=(x-1);
      while (msg[i]!=':') i--;
      i++;
      strncpy(ip_aux,msg+i,x-i);
      //printf("ip:%s\n",ip_aux);
      run_snd_file(ip_aux,porto);
      
    }
  
  printf("%s",msg);  
  return 0;
}

/***************************************************************
 * processar msg PARA ENVIAR (CASO END E SENDFILE)             *
 ***************************************************************/
int proc_cli_msg(char* msg)
{
  int i,x;
  char *caux,c,strpars[MAXLINE];
  for(i=0;(c=msg[i])!=':' && msg[i]!='\0';i++); //sai do ciclo com c==lerlinha[i]==':' ou '\0'
  
  msg[i]='\0'; //partir a srt (caso c==':')
  for(caux=msg;*caux==' ';caux++); //tirar os espaços iniciais
  strcpy(strpars,caux); //tirar até ':' (até ao fim)
  //printf("strpars:%s tamanho:%d indexorig:%d\n",strpars,strlen(strpars),i);
  msg[i]=c;  //voltar a juntar lerlinha original
  
  if(strncmp(strpars,"END",3)==0) //caso "END"
    {
      logout();
      return 0; //envia "END" e sai 
    }

  if(strncmp(strpars,"SNDFILE",7)==0)
    {
      x=i=strlen(msg);
      while (msg[x]!=':') x--; // encontrar os ':'
      while (msg[i]=='\n' || msg[i]=='\0' || msg[i]==' ') i--;//ver o ultimo char valido
      strncpy(file_send,msg+x+1,i-x);
      //printf("file:%s\n",file_send);
    }
    write(sockfd, sendline, strlen(sendline));// vai a linha toda 
    return 0;
}
/***************************************************************
 * envia e receba de/para o servidor                           *
 ***************************************************************/
void sen_rec(FILE *fp, int sockfd)
{
  int maxfdp1, x;
  fd_set rset;
  char aux[MAXLINE];
  
  FD_ZERO(&rset); 
  for ( ; ; ) 
    {
     
      FD_SET(fileno(fp), &rset);
      FD_SET(sockfd, &rset);
      maxfdp1 = max(fileno(fp), sockfd) + 1;
      select(maxfdp1, &rset, NULL, NULL, NULL);

      if (FD_ISSET(sockfd, &rset)) 
	{	// socket is readable 
	  if ((x=read_fd(sockfd, aux, MAXLINE)) == 0) 
	    {
	      printf("ERRO:SERVER:Server crash\n");
	      logout();
	    }
	  else if(x==2)//buffer de leitura ainda não está completo (não\n)
	    {
	      mais_char(recline,aux);
	    }
	  else if(x==1)	// leu uma linha inteira(até \n)
	    {
	      mais_char(recline,aux);
	      if(proc_srv_msg(recline)==-1) //processar msg...
		printf("ERRO:CLIENT: proc_msg()\n"); 
	      recline[0]='\0';
	    }
	  else if(x==-1)
	    printf("ERRO:SERVER: ler do servidor\n");
	    
	}
      
      if (FD_ISSET(fileno(fp), &rset)) // pronto para ler do teclado
	{  
	  if (fgets(sendline, MAXLINE, fp) != NULL) // não deu erro
	    proc_cli_msg(sendline);
	  else
	    printf("ERRO:CLIENT: ler do teclado\n");
	}
    }
}


/***************************************************************
 * main (faz ligaçao )                                         *
 ***************************************************************/
int main(int argc, char **argv)
{  
  signal (SIGTSTP,logout); 
  signal (SIGINT,logout); 
  printf ("REM: (c) X-prog 2002 (Trabalho de R.S.D.) Chat - TCP/IP\n");

  if (argc != 3)
    {
      printf("ERRO:CLIENT: uso  chatcli <IPaddress> <Port>\n");
      return -1;
    }
  if( (sockfd = socket(AF_INET, SOCK_STREAM, 0) ) < 0)
    {
      printf("ERRO:CLIENT: socket error\n");
      return -1;
    }
  
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(atoi(argv[2]));
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
  
  if(connect(sockfd, (SA *) &servaddr, sizeof(servaddr))==-1)
    {
      printf("ERRO:SERVER: Servidor não activo\n");
      return -1;
    }
  
  sen_rec(stdin, sockfd);		/* do it all */


  return 0;
}

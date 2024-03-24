#include "chat.h"

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


/***************************************************************
 * Variaveis globais                                           *
 ***************************************************************/
typedef struct cliente          // estrutura de cliente
{
  struct in_addr ip;            // ip em binario
  char nick[SIZE_NICK];         // nick do cliente 
  FILE_D file_d;                // file_d  para comunicar
  int sala[MAX_SALAS];         // nome da sala em que está 
  char lerlinha[MAXLINE];       // buf aux de ler 
  //int index_ler;              // até onde leu  // não necessario: leu até \0
  char esclinha[16][MAXLINE];   // buf aux de escrever (max 16 msg em buff)
  int index_esc;                // até onde ecreveu (dentro da msg)
  BYTE index_msg;               // qual mgs para escrever ao cli
  BYTE index_buf;               // posição em buf para a proxima msg
}*LP_client,client;

client clientes[FD_SETSIZE];    //tabela de clientes
int last_cli_index;             //index do ultimo cliente na lista(optimização)
int maxfd;                      //max fd para testar na função select()
FILE_D listenfd;                // fd geral para listen()
struct sockaddr_in cliaddr,servaddr; // as socket's (1 para todos os cli)
fd_set rset,wset,allset,writeset;    // controle de ler,escrever,tudo
char salas[MAX_SALAS][SIZE_SALA]={"Negocios","Desporto","\0","\0","\0","\0","\0"};


/***************************************************************
 * inicia o servidor: limpa os clientes e coloca o bind        *
 ***************************************************************/
int init_chat_serv()      // 0 se tudo bem -1 erro
{
  int i=1,x;
  listenfd=socket(AF_INET,SOCK_STREAM,0);
  if (listenfd==-1) // erro 
    {printf("socket error\n");return -1;}
  bzero(&servaddr,sizeof(servaddr));//limpar
  servaddr.sin_family      = AF_INET;  //preencher
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(9999);

  setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&i,sizeof(int));// tempo bind
  
  if (bind(listenfd,(SA *) &servaddr,sizeof(servaddr))==-1) //por à escuta
    {printf("bind error\n");return -1;}       //erro
  //
  if (listen(listenfd,LISTENQ)==-1) //inicializar 
    return -1;       //erro
  
  maxfd=(int)listen; //(inicia com n testes para a funçao select() ??)
  
  for(i=0;i<FD_SETSIZE;i++) //limpar
    {
      if (inet_pton(AF_INET, "0.0.0.0", &clientes[i].ip) <= 0)//limpar ip's
	return -1;
      clientes[i].nick[0]='\0'; //limpar nick
      clientes[i].file_d=-1;    //limpas fd's
      for(x=0;x<MAX_SALAS;x++)  // limpa as salas 
	clientes[i].sala[x]=-1;
      //  bzero(&clientes[i].ip, sizeof(struct in_addr));  // e limpar o ip
      clientes[i].lerlinha[0]='\0'; 
      
      for(x=0;x<16;x++) // limpar todas as mgs em buff
	{
	  clientes[i].esclinha[x][0]='\0'; 
	}
      clientes[i].index_msg=0;           // msg em escrita
      clientes[i].index_buf=0;           // index de por msg em buff
      clientes[i].index_esc=0;           // até onde ecreveu
    }
  last_cli_index=0;  //iniciar o ultimo index na lista com cliente
  
  FD_ZERO(&allset);      
  FD_SET(listenfd,&allset);     //fd geral com tudo "ligado" (?)
  FD_ZERO(&writeset);       //só activa quando tiver de escrever...
  return 0;
}


/***************************************************************
 *Novas conecçoes: por na lista o cli e retorna o index|-1:erro*
 ***************************************************************/
int new_connect()
{
  int i,clilen;
  FILE_D confd;
  if (FD_ISSET(listenfd,&rset)) // ligou um cliente ?
    {
      clilen=sizeof(cliaddr);
      confd=accept(listenfd,(SA *) &cliaddr,&clilen); //retorna o fd
      if (confd==-1)
	{printf("erro em nova conecção(accept)\n");return -1;}
      
      for(i=0;i<FD_SETSIZE;i++) //procurar uma pos livre para novo cli
	{
	  if(clientes[i].file_d<0)  //encontro uma livre
	    {
	      clientes[i].file_d=confd; // preenche 
	      clientes[i].ip=cliaddr.sin_addr;  //ip bit
	      break;                    // e sai da procura
	    }
	}
      if (i==FD_SETSIZE)
	{
	  printf("erro em nova conecção(tabela está cheia)\n");return -1;
	}
      
      FD_SET(confd,&allset); //novo descritor para testar em select
      if(confd>maxfd)
	maxfd=confd;   // actualizar o numero dos testes em select
      
      return i; //retorna a posição da lista onde foi posto 
    }
  else return -1;
  return -1;
}


/***************************************************************
 *fechar a conecção e limpar a estrutura cliente               *
 ***************************************************************/
int kill_connect(int index)
{
  int i;
  char ip_aux[INET_ADDRSTRLEN]; // ver a constante ...
  printf("fecho ligação de: %s\n",
	 inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)));
  close(clientes[index].file_d);
  FD_CLR(clientes[index].file_d,&allset);
  clientes[index].nick[0]='\0';
  clientes[index].file_d=-1;
  for(i=0;i<MAX_SALAS;i++)     // Limpar as salas todas
    clientes[index].sala[i]=-1;
  bzero(&clientes[index].ip, sizeof(struct in_addr));  // e limpar o ip
  clientes[index].lerlinha[0]='\0'; 
  for(i=0;i<16;i++) // limpar todas as mgs em buff
    {
      clientes[index].esclinha[i][0]='\0'; 
    }
  clientes[index].index_esc=0;           // até onde ecreveu
  clientes[index].index_msg=0;           // msg em escrita
  clientes[index].index_buf=0;           // index de por msg em buff
  return 0;
}


/***************************************************************
 *enviar msg em buf:: 0-enviou tudo >0-index onde ficou <0-erro*
 ***************************************************************/
int send_msg(int index)
{
  /*se  
    (struck cliente)
    BYTE index_msg;               // qual mgs para escrever ao cli
    BYTE index_buf;               // posição em buf para a proxima msg
    forem iguais é porque não tem nada para escrever
 */
  int x,slen;
  BYTE i_msg=clientes[index].index_msg; //aux 
  int index_i_esc=clientes[index].index_esc;  //aux

/*  se entrou é porque tem de escrever
    if(i_msg==clientes[index].index_buf)  //falta &0x0F... 
    FD_CLR(clientes[index].file_d,&writeset);
  else 
    {*/
      slen=strlen(clientes[index].esclinha[i_msg&0x0F]+index_i_esc);// slen resto
      x=write(clientes[index].file_d,        //cliente fd
	      clientes[index].esclinha[i_msg&0x0F]+index_i_esc, // *str para esc
	      slen); //n do resto de caracteres
      //escrever e ver até onde 
      clientes[index].index_esc+=x;
      if(clientes[index].esclinha[i_msg&0x0F][index_i_esc+x]=='\0') //fim
	{
	  clientes[index].esclinha[i_msg&0x0F][0]='\0';
	  clientes[index].index_msg++;
	  clientes[index].index_esc=0;
	}
      if((clientes[index].index_msg & 0x0F) == (clientes[index].index_buf & 0x0F))
	FD_CLR(clientes[index].file_d,&writeset); 
        //se foi até ao fim e não tem mains nada no buf 
   // }
  return 0;
}


/***************************************************************
 *buferizar uma string      0-ok  -1-err                       *
 ***************************************************************/
int spool_msg(int index, char *msg)
{
  BYTE i_buf=clientes[index].index_buf;
  if((clientes[index].index_msg & 0x0F) == ((clientes[index].index_buf+1) & 0x0F))
	return -1; // o buffer de escrita está cheio 
  
  strcpy(clientes[index].esclinha[i_buf&0x0F],msg); // copia para o buf.
  clientes[index].index_buf++;  //index da proxima msg
  if(!FD_ISSET(clientes[index].file_d,&writeset))  // se não está activo
    FD_SET(clientes[index].file_d,&writeset); //passa a estar
  return 0;
}


/***************************************************************
 *buferizar as salas a um cliente separadas por' '  0-ok  -1-err*
 ***************************************************************/
int spool_salas(int index)
{
  int i;
  char smsg[MAXLINE];
  smsg[0]='\0';     //limpar
  strcat(smsg,"ROOMS:");
  for(i=0;i<MAX_SALAS;i++)  //smsg  todas as salas separadas por ' ' 
    if(salas[i][0]!='\0') {strcat(smsg," ");strcat(smsg,salas[i]);}
  strcat(smsg,"\n");// '\n' antes do '\0'
  if(spool_msg(index,smsg)==-1) // emviar a smsg ao cliente
    {printf("erro de spool:spool_msg()\n");return -1;}
  return 0;
}


/***************************************************************
 *ver se um cliente está na sala sal 0-sim   -1-não            *
 ***************************************************************/
int esta(int index,int sal)
{
  int i;
  if(clientes[index].nick[0]=='\0') return -1;
  for(i=0;i<MAX_SALAS; i++)
    {
      if(clientes[index].sala[i]==sal) return 0;
    }
  return -1;
}


/***************************************************************
 *buferizar os nicks a um cliente separadoss por' '  0-ok  -1-er*
 ***************************************************************/
int spool_nicks(int index,char* sala)
{
  int aux=-1;
  int i;
  char nmsg[MAXLINE];
  nmsg[0]='\0'; //limpar
  strcat(nmsg,"NICKS:");
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sala,strlen(salas[i])-3)==0) aux=i;
 // printf("sala:%s numero:%d\n",sala,aux);
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:LISTNICKS:Sala invalida\n");
      return -1;
    }
  sprintf(nmsg,"%s%s:",nmsg,salas[aux]);
//  strcat(nmsg,salas[aux]);
 // strcat(nmsg,":");
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0) 
      {
	printf(" %s está na sala\n",clientes[i].nick);
	printf(" %s\n",clientes[i].nick);
	sprintf(nmsg,"%s %s",nmsg,clientes[i].nick);
	//strcat(nmsg," ");  // separar por espaços
	//strcat(nmsg,clientes[i].nick);
      }
  strcat(nmsg,"\n");
  spool_msg(index,nmsg);
  
//  spool_msg(index,"ninguem\n");//teste
  return 0;
}


/***************************************************************
 *escolher um nick        0-ok -1-erro                         *
 ***************************************************************/
int nick(int index,char *ni)
{
  int i;
  for(i=0;i<MAX_SALAS;i++)
    if (clientes[index].sala[i]!=-1) 
      {
	spool_msg(index,"ERRO:NICK:Está numa ou mais salas \n");
	return -1;
      }
  for(i=SIZE_NICK;i>=0;i--)//tirar caracteres "estranhos"
    if(!((ni[i]>='a' && ni[i]<='z') || (ni[i]>='A' && ni[i]<='Z') 
	 || (ni[i]>='0' && ni[i]<='9')))
      ni[i]='\0';
  ni[SIZE_NICK]='\0';
  if(strlen(ni)<2) 
    {
      spool_msg(index,"ERRO:NICK:Nick invalido\n");
      return -1;
    }
  strcpy(clientes[index].nick,ni);
  return 0;
}


/***************************************************************
 *ver o meu nick   Se não tiver nick ainda envia NULL e ret -1 *
 ***************************************************************/
int mynick(int index)
{
  char mmsg[MAXLINE];  //msg a enviar
  mmsg[0]='\0';//limpar
  strcat(mmsg,"MYNICK:"); //comando para cliente
  if(clientes[index].nick[0]=='\0') //se não tem nick envia "NULL"
    {
      strcat(mmsg,"NULL\n");
      spool_msg(index,mmsg); //por na fila de msg do cliente
      return -1;  // erro ... não tem nick
    }
  strcat(mmsg,clientes[index].nick); //caso contrario envia o nick
  strcat(mmsg,"\n"); //fim de msg (comando para cliente)
  spool_msg(index,mmsg); 
  return 0;
}


/***************************************************************
 *entar numa sala                                  0-ok  -1-err*
 ***************************************************************/
int login_sala(int index,char sala[SIZE_SALA])
{
  int i,aux=-1;
  char auxmsg[MAXLINE];
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sala,strlen(salas[i])-3)==0) aux=i;
  //  printf("sala:%s numero:%d\n",sala,aux);
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:LOGIN:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)==0)
    {
      spool_msg(index,"ERRO:LOGIN:Voçe já está na sala\n");
      return -1;
    }
  if(clientes[index].nick[0]=='\0')
    {
      spool_msg(index,"ERRO:LOGIN:Ainda não tem nick valido\n");
      return -1;
    }
  for(i=0;i<=last_cli_index;i++)  //para todos os clientes
    {
      if(clientes[i].file_d!=-1) //se activo & está na sala & mesmo nick
	if(strcmp(clientes[i].nick,clientes[index].nick)==0 && esta(i,aux)==0)
	  {
	    spool_msg(index,"ERRO:LOGIN:Existe um nick igual na sala\n");
	    return -1;
	  }
    }
  //parece estar tudo bem vamos entrar 
  // informar outros que se vai entrar
  sprintf(auxmsg,"LOGIN:%s:%s\n",salas[aux],clientes[index].nick); 
  //contruir a msg
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0) //está ? envia.
      {
	if(spool_msg(i,auxmsg)==-1)  //envia a todos que estão na sala
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }
  //Entrar
  for(i=0;i<MAX_SALAS;i++)
    {
      if(clientes[index].sala[i]==-1)
	{
	  clientes[index].sala[i]=aux;
	  return 0;
	}
    }
  return 0;
}


/***************************************************************
 *Sair de uma Sala                                             *
 ***************************************************************/
int logout_sala(int index,char sal[SIZE_SALA])
{
  int i,aux=-1;
  char auxmsg[MAXLINE];
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sal,strlen(salas[i])-3)==0) aux=i;
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:LOGOUT:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)!=0)
    {
      spool_msg(index,"ERRO:LOGOUT:Voçe não está na sala\n");
      return -1;
    }
  for(i=0;i<MAX_SALAS;i++) //tubo bem: sair da sala
    {
      if(clientes[index].sala[i]==aux) clientes[index].sala[i]=-1;
    }
  
  //informar que se saiu
   sprintf(auxmsg,"LOGOUT:%s:%s\n",salas[aux],clientes[index].nick); 
  //contruir a msg
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0) //está ? envia.
      {
	if(spool_msg(i,auxmsg)==-1)  //envia a todos que estão na sala
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }

  return 0;
}


/***************************************************************
 *enviar msg a todos de uma sala                               *
 ***************************************************************/
int difuse_msg(int index,char* sal,char* msg)
{
  char auxmsg[MAXLINE];
  int i,aux=-1;
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sal,strlen(salas[i])-3)==0) aux=i;
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:MSG:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)!=0)
    {
      spool_msg(index,"ERRO:MSG:Voçe não está na sala\n");
      return -1;
    }

  sprintf(auxmsg,"MSG:%s:%s:%s",salas[aux],clientes[index].nick,msg); 
  //contruir a msg

  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0) //está ? envia.
      {
	if(spool_msg(i,auxmsg)==-1)  //envia a todos que estão na sala
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }
  return 0;
}


/***************************************************************
 *enviar msg privada numa sala                               *
 ***************************************************************/
int private_msg(int index,char* sal,char* nickd,char* msg)
{
  char auxmsg[MAXLINE];
  int i,x,aux=-1;
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sal,strlen(salas[i])-3)==0) aux=i;
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:PRV:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)!=0)
    {
      spool_msg(index,"ERRO:PRV:Voçe não está na sala\n");
      return -1;
    }
  x=-1; // para testar se econtou cliente e enviou 
  sprintf(auxmsg,"PRV:%s:%s:%s",salas[aux],clientes[index].nick,msg); 
  //contruir a msg
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0 &&  //está e o mesmo nick? envia.      
       strncmp(nickd,clientes[i].nick,sizeof(clientes[i].nick))==0)
      {
	x=0;
	if(spool_msg(i,auxmsg)==-1)  //envia a todos que estão na sala
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }
  
  if(x==-1)  //caso erro isto é o que envia ao cliente emissor
      sprintf(auxmsg,"ERRO:PRV: %s não esta na sala %s\n",nickd,sal);

  spool_msg(index,auxmsg);
  return x;
  
}


/***************************************************************
 * aviso de envio de ficheiro                                  *
 ***************************************************************/
int sendfile(int index,char* sal,char* nickd,char* file)
{
  char auxmsg[MAXLINE];
  int i,x,aux=-1;
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sal,strlen(salas[i])-3)==0) aux=i;
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:SNDFILE:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)!=0)
    {
      spool_msg(index,"ERRO:SNDFILE:Voçe não está na sala\n");
      return -1;
    }
  x=-1; // para testar se econtou cliente e enviou 
  sprintf(auxmsg,"SNDFILE:%s:%s:%s",salas[aux],clientes[index].nick,file);
  //contruir a msg
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0 &&  //está e o mesmo nick? envia.      
       strncmp(nickd,clientes[i].nick,sizeof(clientes[i].nick))==0)
      {
	x=0;
	if(spool_msg(i,auxmsg)==-1)  //envia ao nick
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }
  
  if(x==-1)  //caso erro isto é o que envia ao cliente emissor
    {
      sprintf(auxmsg,"ERRO:SNDFILE: %s não esta na sala %s\n",nickd,sal);
      spool_msg(index,auxmsg);  // aviso de recepção do erro
      return -1;
    }

  sprintf(auxmsg,"OK:SNDFILE:%s:%s:%s",salas[aux],nickd,file); // Por o OK
  spool_msg(index,auxmsg);  // aviso de recepção
  return x;
}


/***************************************************************
 * aviso espera de ficheiro                                    *
 ***************************************************************/
int recfile(int index,char* sal,char* nickd,char* ippt)
{
  char auxmsg[MAXLINE];
  int i,x,aux=-1;
  for(i=0;i<MAX_SALAS;i++)  // ver o numero associado a sala
    if(salas[i]!="\0")
      if(strncmp(salas[i],sal,strlen(salas[i])-3)==0) aux=i;
  if(aux==-1) // se não associou a uma sala 
    {
      spool_msg(index,"ERRO:RECFILE:Sala invalida\n");
      return -1;
    }
  if(esta(index,aux)!=0)
    {
      spool_msg(index,"ERRO:RECFILE:Voçe não está na sala\n");
      return -1;
    }
  // ver se entre  ip e porto há : 
  i=-1;
  for(x=0;ippt[x]!='\n' && ippt[x]!='\0';x++)
    {
      if(ippt[x]==':') i=0;   // se encontro i=0
    }
  if (i==-1)// não encontro :
    {
      spool_msg(index,"ERRO:RECFILE: ip:porta incorrecto\n");
      return -1;
    }
  
  x=-1; // para testar se econtou cliente e enviou 
  sprintf(auxmsg,"RECFILE:%s:%s:%s",salas[aux],clientes[index].nick,ippt);
  //contruir a msg
  for(i=0;i<=last_cli_index;i++)
    if(esta(i,aux)==0 &&  //está e o mesmo nick? envia.      
       strncmp(nickd,clientes[i].nick,sizeof(clientes[i].nick))==0)
      {
	x=0;
	if(spool_msg(i,auxmsg)==-1)  //envia ao nick
	  {printf("erro de spool:spool_msg()\n");return -1;}
      }
  
  if(x==-1)  //caso erro isto é o que envia ao cliente emissor
    {
      sprintf(auxmsg,"ERRO:RECFILE: %s não esta na sala %s\n",nickd,sal);
      spool_msg(index,auxmsg);  // aviso de recepção do erro
      return -1;
    }

  sprintf(auxmsg,"OK:RECFILE:%s:%s:%s",salas[aux],nickd,ippt); // Por o OK
  spool_msg(index,auxmsg);  // aviso de recepção
  return x;
}


/***************************************************************
 *processar msg lida:: SALA?? NICK?? ou MSG??                  *
 ***************************************************************/
int proc_read_msg(int index)
{
  int aux,i,x=0;
  char c,*caux;
  char strpars[MAXLINE];
  char strpars2[SIZE_NICK];
  char ip_aux[INET_ADDRSTRLEN]; // ver a constante ...
  printf("linha enviada por %s :%s",
	 inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	 clientes[index].lerlinha); 
  /*if(spool_msg(index,clientes[index].lerlinha)==-1)  //teste
    printf("erro de spool: spool_msg()\n");*/

  aux=strlen(clientes[index].lerlinha);
  for(i=0;
      (c=clientes[index].lerlinha[i])!=':' && 
	clientes[index].lerlinha[i]!='\0';
      i++); //sai do ciclo com c==lerlinha[i]==':' ou '\0'

  clientes[index].lerlinha[i]='\0'; //partir a srt (caso c==':')
  for(caux=clientes[index].lerlinha;*caux==' ';caux++); //tirar os espaços iniciais
  strcpy(strpars,caux); //tirar até ':' (até ao fim)
  //printf("strpars:%s tamanho:%d indexorig:%d\n",strpars,strlen(strpars),i);
  clientes[index].lerlinha[i]=c;  //voltar a juntar lerlinha original
  
  //-----------------------
  while(strpars[x]==' ') x++; //tirar espaços iniciais
  if(strpars[x]=='\n') // se so tem espaços e enter não dá erro 
     {
       clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
       return 0;  // se deu enter
     }
  //-----------------------
  if(strncmp(strpars,"LISTROOMS",9)==0) // comando listar salas
    {
      printf("Cliente:%s Comando:LISTROOMS \n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)));
      if(spool_salas(index)==-1) printf("erro de spool: spool_salas()\n");

      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }
  //-----------------------
  if(strncmp(strpars,"END",3)==0) // comando END fim de ligação
    {
      printf("Cliente:%s Comando:END \n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)));
      if(kill_connect(index)==-1) printf("erro de fecho: kill_connect()\n");

      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }

  //-----------------------
  if(strncmp(strpars,"LISTNICKS",9)==0)// comando listar nicks de uma sala
    {
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // SALA?
       printf("Cliente:%s Comando:LISTNICKS argumentos:%s\n",
	      inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	      caux);
       if(spool_nicks(index,caux)==-1) printf("erro de spool: spool_nicks()\n");
       
       clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
       return 0;
    }
  //------------------------
  if(strncmp(strpars,"NICK",4)==0) // comando escolher nick
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // NICK?
       printf("Cliente:%s Comando:NICK argumentos:%s\n",
	      inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	      caux);
       if(nick(index,caux)==0) 
	 spool_msg(index,"OK:NICK: Nick alterado\n");
       else
	 printf("erro escolha de nick:nick()\n");
       
        clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	return 0;
    }
  //-----------------------
  if(strncmp(strpars,"MYNICK",6)==0) // ver nick
    {
     // for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // NICK?
      printf("Cliente:%s Comando:MYNICK \n",
	      inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)));
      if(mynick(index)==-1) printf("erro ver nick:mynick()\n");
      
      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }
  //-----------------------
  if(strncmp(strpars,"LOGIN",5)==0) // comando escolher nick
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // NICK?
       printf("Cliente:%s Comando:LOGIN argumentos:%s\n",
	      inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	      caux);
       if(login_sala(index,caux)==0) 
	 spool_msg(index,"OK:LOGIN: Login efectuado\n");
       else
	 printf("erro login:login_sala()\n");
       
        clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	return 0;
    }   
 //-----------------------
  if(strncmp(strpars,"LOGOUT",6)==0) // comando escolher nick
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // NICK?
       printf("Cliente:%s Comando:LOGOUT argumentos:%s\n",
	      inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	      caux);
       if(logout_sala(index,caux)==0) 
	 spool_msg(index,"OK:LOGOUT: Logout efectuado\n");
       else
	 printf("erro logout:logout_sala()\n");
       
        clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	return 0;
    }   
 //-----------------------
  if(strncmp(strpars,"MSG",3)==0) // comando enviar msg
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // Sala
      
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]=='\0') 
	{ 
	  printf("erro MSG:linha incompleta()\n");
	  spool_msg(index,"ERRO:MSG:erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars,caux); //tirar até ':' (até ao fim)
      
      caux[i]=c;  //voltar a juntar lerlinha original
      printf("Cliente:%s Comando:MSG argumentos:%s:%s\n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	     strpars,caux+i+1);
      if(difuse_msg(index,strpars,caux+i+1)==0) 
	 spool_msg(index,"OK:MSG: MSG efectuado\n");
       else
	 printf("erro MSG:difuse_msg()\n");
             
      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }   
  //-----------------------
  if(strncmp(strpars,"PRV",3)==0) // comando enviar msg
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // Sala
      
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]=='\0') 
	{ 
	  printf("erro PRV:linha incompleta()\n");
	  spool_msg(index,"ERRO:PRV: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars,caux); //tirar até ':' (até ao fim)
      
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      for(;*caux==' ';caux++); // NICK (tirar espaçao depois dos ':')
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]!=':') 
	{ 
	  printf("erro PRV:linha incompleta()\n");
	  spool_msg(index,"ERRO:PRV: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars2,caux); //tirar até ':' (até ao fim)
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      printf("Cliente:%s Comando:PRV argumentos:%s:%s:%s\n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	     strpars,strpars2,caux);
      if(private_msg(index,strpars,strpars2,caux)==0) 
	spool_msg(index,"OK:PRV: efectuado\n");
       else
	 printf("erro PRV:private_msg()\n");
             
      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }  
  //-----------------------
    if(strncmp(strpars,"SNDFILE",7)==0) // comando enviar aviso de file
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // Sala
      
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]=='\0') 
	{ 
	  printf("erro SNDFILE:linha incompleta()\n");
	  spool_msg(index,"ERRO:SNDFILE: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars,caux); //tirar até ':' (até ao fim)
      
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      for(;*caux==' ';caux++); // NICK (tirar espaçao depois dos ':')
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]!=':') 
	{ 
	  printf("erro SNDFILE:linha incompleta()\n");
	  spool_msg(index,"ERRO:SNDFILE: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars2,caux); //tirar até ':' (até ao fim)
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      printf("Cliente:%s Comando:SNDFILE argumentos:%s:%s:%s\n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	     strpars,strpars2,caux);
      if(sendfile(index,strpars,strpars2,caux)==0) 
	spool_msg(index,"OK:SNDFILE: efectuado\n");
       else
	 printf("erro SNDFILE:sendfile()\n");
             
      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }  

 //-----------------------
    if(strncmp(strpars,"RECFILE",7)==0) // comando pronto para receber file
    {           //tirar espaços do inicio
      for(caux=clientes[index].lerlinha+i+1;*caux==' ';caux++); // Sala
      
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]=='\0') 
	{ 
	  printf("erro RECFILE:linha incompleta()\n");
	  spool_msg(index,"ERRO:RECFILE: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars,caux); //tirar até ':' (até ao fim)
      
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      for(;*caux==' ';caux++); // NICK (tirar espaçao depois dos ':')
      for(i=0;(c=caux[i]!=':') && caux[i]!='\0';i++); 
      //sai do ciclo com c==caux[i]==':' ou '\0'
      if(caux[i]!=':') 
	{ 
	  printf("erro RECFILE:linha incompleta()\n");
	  spool_msg(index,"ERRO:RECFILE: erro de sintaxe\n");
	  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
	  return -1;
	}
      caux[i]='\0'; //partir a srt (caso c==':')
      strcpy(strpars2,caux); //tirar até ':' (até ao fim)
      caux[i]=c;  //voltar a juntar lerlinha original
      caux+=(i+1);
      printf("Cliente:%s Comando:RECFILE argumentos:%s:%s:%s\n",
	     inet_ntop(AF_INET,&clientes[index].ip,ip_aux,sizeof(ip_aux)),
	     strpars,strpars2,caux);
      if(recfile(index,strpars,strpars2,caux)==0) 
	spool_msg(index,"OK:RECFILE: efectuado\n");
       else
	 printf("erro RECFILE:sendfile()\n");
             
      clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
      return 0;
    }  
  //-----------------------
    
    
  spool_msg(index,"ERRO:CMD: Comando invalido\n");
  clientes[index].lerlinha[0]='\0'; //limpar buff de leitura
  return 0;
}


/***************************************************************
 *main                                                         *
 ***************************************************************/
int main()
{
  int aux,n;
  FILE_D sockfd;
  char ip_aux[INET_ADDRSTRLEN]; // ver a constante ...
  char line[MAXLINE];
  
  //signal (SIGTSTP,SIG_IGN); 
  //signal (SIGINT,SIG_IGN); 
  if (init_chat_serv()==-1)
    {
      printf("erro na inicialização:init_chat_serv()\n");
      return -1;
    }
  
  for( ; ; )
    {
      rset=allset;    // para não ser alterado no select
      wset=writeset; // para não ser alterado no select
      /*so activo os writeset nos file_d que tiver de escrever
        Enquanto o return do write < n caracteres para escrever
        manter o FD_SET do "fd_cli". Quando acabar fazer FD_CLR ao fd */
      if(select(maxfd+1,&rset,&wset,NULL,NULL)==-1)//ultimo NULL é do tempo ...
	{printf("erro em select\n"); return -1;}
      
      if((aux=new_connect())>=0)          //  novas conecções
	{
	  if (aux>last_cli_index) last_cli_index=aux; //var de opt
	  printf("nova ligação de : %s\n",
		inet_ntop(AF_INET,&clientes[aux].ip,ip_aux,sizeof(ip_aux)));   
//	  spool_salas(aux);  // enviar as salas existentes
	}
      
      /*
	LEITURAS E ESCRITAS ...  (cont)
	*/

      for(aux=0;aux<=last_cli_index;aux++) // procura até last_cli_index ...
	{
	  if((sockfd=clientes[aux].file_d)< 0)
	    continue; //se este cliente não está activado passa à frente
	  if(FD_ISSET(sockfd,&rset)) // se está pronto para ler
	    {
	      if( (n=read_fd(sockfd,line,MAXLINE))==0) //lê. se n==0 fechou 
		{
		  kill_connect(aux);
		}
	      else if (n==2) //buffer de leitura ainda não está completo (não\n)
		{
		  mais_char(clientes[aux].lerlinha,line); //acrescentar o que leu
		}
	      else if (n==1) 
		// leu uma linha inteira(até \n) ou resto de linha: processar a linha 
		{
		  mais_char(clientes[aux].lerlinha,line); //acrescentar o resto que leu
		  proc_read_msg(aux);
		}
	      else if (n==-1) // erro
		printf("erro de leitura no cielnte: %s",
		      inet_ntop(AF_INET,&clientes[aux].ip,ip_aux,sizeof(ip_aux)));
	    }
	  
	  if(FD_ISSET(sockfd,&wset)) // se está pronto para escrever
	    {
	      send_msg(aux); // escrever...
	    }
	}
 
   }// fim do for(;;)
  return 0;
}

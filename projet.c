#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>

#define IFLAGS (SEMPERM | IPC_CREAT)
#define SKEY   (key_t) IPC_PRIVATE	
#define SEMPERM 0600		 /* Permission */

int sem_id ,sem_id_process;
struct sembuf sem_oper_P ;  /* Operation P */
struct sembuf sem_oper_V ;  /* Operation V */
int shm_desc,shm_trniquet[10],shm_allocCPU,shm_quantum;
int *p_desc,*p_trn[10],*p_alloc,*p_quantum;
int nb;//nb process
int d_max;//date de soumission max
int t_max;//tmp exec max
int length;
pid_t grd_parent,parent;//pid of parent
pid_t *t_child;//tableau des pid enfants
int n_child;//nb of pid in t_child

int getSemVal(int semid,int semnum)
{
	int status;
	status = semctl(semid, 0, GETVAL);
	printf("Value of SEMID: %d SEMNUM: %d is %d\n",semid,semnum,status);
	return(status);
	
}

void P(int semid,int semnum) {
	int v;
	sem_oper_P.sem_num = semnum;
	sem_oper_P.sem_op  = -1 ;
	sem_oper_P.sem_flg = 0 ;
	v=semop(semid,&sem_oper_P,1);
	if (v == -1) {
		perror("semop");
		exit(EXIT_FAILURE);
	    }
}

void V(int semid,int semnum) {
	int v;
	sem_oper_V.sem_num = semnum;
	sem_oper_V.sem_op  = 1 ;
	sem_oper_V.sem_flg  = 0 ;
	v=semop(semid,&sem_oper_V,1);
	if (v == -1) {
		perror("semop");
		exit(EXIT_FAILURE);
	    }
}





int initsem(key_t semkey,int n) 
{
	int status = 0;		
	int semid_init;
   	union semun {
		int val;
		struct semid_ds *stat;
		short * array;
	} ctl_arg;
    if ((semid_init = semget(semkey, 11, IFLAGS)) > 0) {
		
	    	
	    	short *array;
	    	array= malloc(n*sizeof(short));
	    	for (int i = 0; i < n; ++i)
	    	{
	    		array[i]=0;
	    	}
	    	ctl_arg.array = array;
	    	status = semctl(semid_init, 0, SETALL, ctl_arg);
	    	free(array);
    }

   if (semid_init == -1 || status == -1) { 
	perror("Erreur initsem");
	return (-1);
    } else return (semid_init);

}


int liberationSem(int sem)
{
	int r;
	if((r=semctl(sem, 0, IPC_RMID))==-1)
	{
		perror("semctl rm");
		exit(5);
	}
	return r;
}

int liberationSeg(int shm,int *p )
{
	int r,v;
	if((v=shmdt(p))==-1)
	{
		perror("shmdt");
		exit(6);
	}
	if((r=shmctl(shm, IPC_RMID,NULL ))==-1){
		perror("shmctl rm");
		exit(4);
	}
	return r;
}

int* init_seg(key_t shmkey,int *seg,int n/*taille*/){
	int shm;
	int *p;
	if((shm=shmget(shmkey, n*sizeof(int), IPC_CREAT | 0666))==-1)
	{
		perror("shmget");
		exit(2);
	}
	*seg=shm;
	if((p=(int *)shmat(shm, NULL, 0))==-1){
		perror("shmat");
		exit(3);

	}
	return p;


}

int print_desc_table()
{
	printf("id              : ");
	for (int i = 0; i < nb; ++i)
	{
		printf("%d 	 ",p_desc[i*4]);
	}
	printf("\n");
	printf("prio            : ");
	for (int i = 0; i < nb; ++i)
	{
		printf("%d 	 ",p_desc[i*4+1]);
	}
	printf("\n");
	printf("date soumission : ");
	for (int i = 0; i < nb; ++i)
	{
		printf("%d 	 ",p_desc[i*4+2]);
	}
	printf("\n");
	printf("temp execution  : ");
	for (int i = 0; i < nb; ++i)
	{
		printf("%d 	 ",p_desc[i*4+3]);
	}
	printf("\n");
	
}


int* remplir_desc(int n)
{
		srand(time(0));
	p_desc=init_seg(SKEY,&shm_desc,n*4);
	int t=t_max-1 ;// so that the execution time are at least 1

	for(int i=0;i<n;i++)
	{
		p_desc[4*i]=i+1;//id process going from 1 to n
		
		p_desc[4*i+1]=(rand()%10)+1;
		if(d_max>0)
		{
			p_desc[4*i+2]=rand()%d_max;
		}
		else
		{
			p_desc[4*i+2]=0;	
		}
		if(t_max>0)
		{

			p_desc[4*i+3]=rand()%t+1;
		}
		else
		{
			p_desc[4*i+3]=0;	
		}
		

	}
	print_desc_table();

	return p_desc;
} 



int sortir_int(int * p,int n)//n taille du segment à décaler/fction servant a sortir le premier entier de la file et décaler les nb restant
{
	int r;
	if((r=p[0])!=-1){
		for(int i=0;i<n-1;i++){
			p[i]=p[i+1];
		}
		p[n-1]=-1;//possible? sinon =0 ou -1
		return r;
	}
	else
	{
		return 0;//return 0 si vide
	}
}

int sortir_int_trn(int prio)
{
	int r;
	
	for (int i = 0; i < 10; ++i)
	{
		if((r=sortir_int(p_trn[prio-1],nb))!=0)
		{
			return r;
		}
		prio--;
		if(prio<1)
		{
			prio=prio+10;
		}
	}
	printf("tout les tourniquets de priorité sont vides\n");
	return 0;


}

int ajouter_int(int *p,int n, int e)//ajouter l'entier e a la fin de la file
{

	for(int i=0;i<n;i++)
	{
		if(p[i]==-1)
		{
			p[i]=e;
			return 1;
		}
	}
}

int ajouter_int_alloc(int *p,int n, int e)//ajouter l'entier e a la fin de la file//spécifique à la fction remplir alloc pour l'instant
{

	for(int i=0;i<n;i++)
	{
		if(p[i]==-1)
		{
			p[i]=e;
			return 1;
		}
	}
}

int check_array(int *array,int n)
{
	for (int i = 0; i < n; ++i)
	{
		if(array[i]>0)
		{
			return 1;
		}
	}
	return 0;

}



int print_trn()
{
	printf("fction print_trn\n");
	for (int i = 0; i < 10; ++i)
	{
		printf("prio %d :",i+1 );
		for (int k = 0; k < nb; ++k)
		{
			printf("%d ",p_trn[i][k] );
		}
		printf("\n");
	}
	printf("\n");
}

int* remplir_alloc()//creer un seg de taille date soumission max plus n * tmp exec max //au passage
{
	int l=nb*t_max+d_max,e;
	length=l;
	p_alloc=init_seg(SKEY,&shm_allocCPU,l);

	for (int i = 0; i < l; ++i)
	  {
	  	p_alloc[i]=-1;
	  }

	 for (int i=0;i<l;i++)
	  {
	  	e=(rand()%10)+1;
	  	p_alloc[i]=e;
	  }
	  printf("alloc table : ");
	for (int i = 0; i < nb*t_max+d_max; ++i)
	{
		printf("%d ",p_alloc[i] );
	}
	printf("\n");



}

int check_process()//check if any process isnt finished
{

	for(int i=0;i<nb;i++)
	{
		if(p_desc[4*i+3]>0)
		{
			return 0;
		}
	}
	return 1;
}

int get_process_t(int id){
	id--;
	return p_desc[4*id+3];
}


int start_process(int id)
{
	int k,v,prio;
		printf("hello dans start process id : %d\n",id );
		k=id-1;
		prio=p_desc[4*k+1];
		printf("prio of process to start is : %d\n", prio);
		ajouter_int(p_trn[prio-1],nb,id);

		n_child++;
		if( !( t_child[n_child-1]=fork() ) )
		{

			while((get_process_t(k+1))>0)
			{

				P(sem_id_process,k);
				printf("Debut exec quantum for process %d\n",k+1);
				usleep(100000);
				printf("fin quantum exec for process %d\n",k+1);
				V(sem_id,0);

			}
			
			exit(0);

		}
}



int check_start_process()
{
	for (int i = 0; i < nb; ++i)
	{
		if(p_desc[4*i+2]==p_quantum[0])
		{
			
			printf("le process %d doit commencer\n",i+1 );
			start_process(i+1);
		}
	}
}

int fonc_roundRobin()
{
	int v;
	
	if(!(parent=fork()))//this is the schduler
	{
		parent=getpid();
		int elu_id,prio,r_prio;
		while(!check_process()){
			P(sem_id,0);
			check_start_process();
			print_trn();
			
			
			prio=sortir_int(p_alloc,length);//on obtient la prio de la table des allocs
			printf("prio choisie est : %d\n",prio );
			if((elu_id=sortir_int_trn(prio))!=0)
			{
				
				
				printf("elu_id is :%d\n",elu_id );
				printf("!intermediary trniquet!\n");
				print_trn();
				r_prio=p_desc[(elu_id-1)*4+1];//priorité reelle du process élu
				p_desc[(elu_id-1)*4+1]--;//on decremente prio de 1
				p_desc[(elu_id-1)*4+3]--;//on decremente tmp exec de 1	
				if(p_desc[(elu_id-1)*4+3]>0)//test si l'on doit faire une autre entrée dans les tourniquets de priorité ou si le process est fini
				{	
					if((r_prio--)>=1)//test si la prio passe en dessous du minimum
					{
						ajouter_int(p_trn[r_prio-1],nb,elu_id);
						V(sem_id_process,elu_id-1);
					}
					else
					{
						p_desc[(elu_id-1)*4+1]+=10;//on repasse la prio a 10 si elle passe en dessous de 1
						ajouter_int(p_trn[(r_prio+10)-1],nb,elu_id);
						V(sem_id_process,elu_id-1);
					}
				}
				else//sinon le programme est fini
				{
					
					V(sem_id_process,elu_id-1);
					
				}

			}
			else
			{
				printf("pas d'élu trouvé\n");
				V(sem_id,0);
			}

			p_quantum[0]++;
			
			


		}
		printf("end of fonc_roundRobin\n");
		for (int i = 0; i < nb; ++i)
		{
			wait(0);
		}
		printf("end of wait\n");
		exit(0);


	}
}

int* creerTourniquet(){
	for(int i=0;i<10;i++)
	{
		p_trn[i]=init_seg(SKEY,&shm_trniquet[i],nb);
		for(int k=0;k<nb;k++)
		{
			p_trn[i][k]=-1;
		}
		
	}

}

void erreur(const char* msg) {

  fprintf(stderr,"%s\n",msg);

  }

void traitantSIGINT(int num) {

    if (num!=SIGINT)
    	erreur("Pb sur SigInt...");
    
	printf("Entré dans le traitant SIGINT\n");

	if(getpid()!=parent  && getpid()!=grd_parent)
	{
		kill(parent,SIGINT);
	}
	else if(getpid()!=grd_parent)
	{
		for (int i = 0; i < n_child; ++i)
			{
				kill(t_child[i],SIGKILL);
			}
		exit(0);
	}
	else
	{
		printf("SIGINT in process grd_parent, freeing memeory\n");
		return;
	}

    exit(0);
  }



int main(int argc, char* argv[])
{

	int k=1,v;
	grd_parent=getpid();
	if(argc!=4){
		printf("argument number error\nprogram call must be of type : ./Prg_name nb_process date_soumission_max tmp_exec_max\n ");
		exit(7);
	}
	else
	{
		
		if (sscanf(argv[1], "%d", &nb) != 1) {
        perror("sscanf nb");
        exit(8);
    	}
		if (sscanf(argv[2], "%d", &d_max) != 1) {
        perror("sscanf nb");
        exit(8);
    	}
		if (sscanf(argv[3], "%d", &t_max) != 1) {
        perror("sscanf nb");
        exit(8);
    	}
	}

	if(nb==0)
	{
		printf("0 process to schdule, end\n");
		exit(0);
	}

	t_child=malloc(nb*sizeof(pid_t));
	for (int i = 0; i < nb+1; ++i)
	{
		t_child[i]=-1;
	}

	sem_id=initsem(SKEY,1);
	getSemVal(sem_id,0);
	V(sem_id,0);
	getSemVal(sem_id,0);
	sem_id_process=initsem(SKEY,nb);
	printf("id of sem_id is : %d\n",sem_id);
	printf("id of sem_id_process is : %d\n",sem_id_process);
  
	p_quantum=init_seg(SKEY,&shm_quantum,1);
	p_quantum[0]=0;

	remplir_desc(nb);
	creerTourniquet();
	remplir_alloc();
	fonc_roundRobin();	
	wait(0);
	

	liberationSeg(shm_allocCPU,p_alloc);
	liberationSeg(shm_quantum,p_quantum);

	for(int i=0;i<10;i++)
	{
		liberationSeg(shm_trniquet[i],p_trn[i]);
	}

	liberationSeg(shm_desc,p_desc);

	liberationSem(sem_id); 
	liberationSem(sem_id_process);

	free(t_child);
} 

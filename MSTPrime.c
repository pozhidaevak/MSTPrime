//#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#define MATR(i, j) matrix[mSize * (i) + (j)]
#define P_MATR(i, j) procMatrix[mSize * (i) + (j)]
#define MY_RND() rand() % 20
//#define TEST

FILE *f_matrix, *f_time, *f_res;

int size;//кол во создаваемых тредов

int* matrix; 
int mSize; // размерность матрицы

int* pProcInd; // массив номеров первой строки, расположенной на процессе
int* pProcNum; // количество строк линейной системы, расположенных на процессе

typedef struct { int parent; int child; } Edge;
typedef struct 
{
	int low;
	int hi;
}
Range;


int* MST; // английский МОД :-)
int weight; //вес МОД

pthread_t* threadsId;
Edge* threadsRes;


/**
 * Определяется кол-во строк для каждого процессора, заполняется матрица, дробится на части и отправляется процессорам
 */
void ProcessInitialization()
{
  int i,j;       

  //рассчитьтать кол-во и начальную строку для каждого процесса 
  pProcInd = (int*)malloc(sizeof(int) * size);   
  pProcNum = (int*)malloc(sizeof(int) * size);

  pProcInd[0] = 0;
  pProcNum[0] = mSize / size;
  int remains = size - (mSize % size); // кол-во процессов с mSize / size строк, у остальных на одну больше
  for (i = 1; i < remains; ++i) 
  {
    pProcNum[i] = pProcNum[0];
    pProcInd[i] = pProcInd[i - 1] + pProcNum[i - 1];
  }
  for (i = remains; i < size; ++i)
  {
    pProcNum[i] = pProcNum[0] + 1;
    pProcInd[i] = pProcInd[i - 1] + pProcNum[i - 1];
  }

  //заполнение матрицы
	matrix = (int*)malloc(mSize*mSize*sizeof(int));
	for (i = 0; i < mSize; ++i )
	{
	  MATR(i,i) = 0;
	  for(j = i + 1; j < mSize; ++j )
	  {
		#ifdef TEST
		int buf;
		fscanf(f_matrix, "%d\n", &buf);
		MATR(i,j)=MATR(j,i) = buf;
		#else
		MATR(i,j)=MATR(j,i)=MY_RND();
		#endif
	  }
	}
	#ifdef TEST
	fclose(f_matrix);
	for (int i = 0; i <mSize; ++i)
	{
		for (int j = 0; j < mSize; ++j)
		{
			printf("%d ",MATR(i, j));
		}
		printf("\n");
	}
	fflush(stdout);
	#endif 
 
  

  
  
  MST = (int*)malloc(sizeof(int)*mSize); // дерево вида MST[childInd] = parentInd
  for (int i = 0; i < mSize; ++i)
  {
    MST[i] = -1;
  }

  threadsId = (pthread_t*)malloc(sizeof(pthread_t) * size);
  threadsRes = (Edge*)malloc(sizeof(Edge) * size);
}

void* PrimsThread(void* range)
{
  pthread_t self = pthread_self();
  int selfId = 0;
  
  for (int i = 0; i< size;++i)
  {
	
    if(pthread_equal(self,threadsId[i]))
    {
		
		selfId = i;
      break;
    }
	
  }
  

  int mini = INT_MAX;
  int child = -1;
  int parent = -1;
  for (int i = pProcInd[selfId]; i < pProcNum[selfId] + pProcInd[selfId]; ++i)
    {
      if (MST[i] != -1) //одна из вершин должна входить в МОД
      {
        for (int j = 0; j < mSize; ++j)
        {
          if (MST[j] == -1) //а другая нет
          {
            
            if (MATR(i, j) < mini && MATR(i, j) != 0)
            {
              mini = MATR(i, j);
              child = j;
              parent = i;   
            }
          }
        }
      }
    }

    threadsRes[selfId].child = child;
    threadsRes[selfId].parent = parent;
    pthread_exit(NULL);
}
/**
 * Реализация алгоритма Прима с паралельным поиском минимального доступного ребра
 */
void PrimsAlgorithm()
{
  MST[0] = 0;
  weight = 0;

  int mini = INT_MAX;
  int parent = -123;
  int child = 0;

  struct { int miniValue; int rank; } miniRow/*минимальная строка*/, row/*минимальная строка в рамках одного процесса*/;
  Edge edge;
  for (int k = 0; k < mSize - 1; ++k)
  {
	  mini = INT_MAX;
    //создаем нити
    for (int i = 0; i < size; ++i)
    {
      pthread_create(&threadsId[i], NULL, &PrimsThread,NULL);
    }
    //ждем завершения
    for (int i = 0; i < size; ++i)
    {
      if(pthread_join(threadsId[i],NULL))
      {
        printf("join fails\n");fflush(stdout);
      }
    }
    

    for(int i = 0; i < size; ++i)
    {
      if(threadsRes[i].child < 0 || threadsRes[i].parent < 0 )
      {
        continue;
      }
      int currentWeight = MATR(threadsRes[i].parent, threadsRes[i].child);
      if(currentWeight < mini && currentWeight !=0 )
      {
        mini = currentWeight;
        parent = threadsRes[i].parent;
        child = threadsRes[i].child;
      }
    }
    
	
    MST[child] = parent;
    weight += MATR(child, parent);
  }
}
/**
 * Освобождение памяти
 */
void ProcessTerminiation () 
{
  free(MST);
  free(matrix);
  free(threadsRes);
  free(threadsId);
}

int main(int argc,char *argv[])
{
  double start, finish, duration; // для подсчета времени вычислений

   
  srand(time(NULL));
  
    #ifdef TEST
    f_matrix = fopen("example", "r");
    fscanf(f_matrix, "%d\n", &mSize);
	size = 6;
    #else
    if (argc < 3)
    {
      printf("We need two args -- matrix size and amount of threads\n");
      return -1;
    }
    else
    {
      mSize = atoi(argv[1]);
      size = atoi(argv[2]);
    }
    #endif
  

 

  ProcessInitialization();

 
   start = (double)clock()/CLOCKS_PER_SEC;   
  PrimsAlgorithm();
  
 
  duration = (double)clock()/CLOCKS_PER_SEC-start;

  
    #ifdef TEST
    f_res = fopen("Result.txt", "w");
    fprintf(f_res,"%d\n ", weight);
    for (int i=0; i< mSize; ++i)
    {
      fprintf(f_res,"%d %d\n",i, MST[i]);
    }
    fclose(f_res);
    #endif

    f_time = fopen("Time2.txt", "a+");
    fprintf(f_time, " Number of processors: %d\n Number of vertices: %d\n Time of execution: %f\n\n", size, mSize, duration);
    fclose(f_time);
    printf("\n Number of processors: %d\n Time of execution: %f\n\n", size, duration);
  

  // Завершение процесса вычислений
  ProcessTerminiation(); 
 
    return 0;
}

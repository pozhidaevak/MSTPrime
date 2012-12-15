#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#define MATR(i, j) matrix[mSize * (i) + (j)]
#define P_MATR(i, j) procMatrix[mSize * (i) + (j)]
#define MY_RND() rand() % 20
//#define TEST

FILE *f_matrix, *f_time, *f_res;

int size;	// число доступных процессов
int rank;	// ранг текущего процесса

int* procMatrix;
int mSize;

int* pProcInd; // массив номеров первой строки, расположенной на процессе
int* pProcNum; // количество строк линейной системы, расположенных на процессе

typedef struct { int Pred; int Succ; } Edge;

int* MST;
int weight;

void ProcessInitialization()
{
  int i,j;       

  //рассчитьтать кол-во и начальную строку для каждого процесса TODO may be MPI_SCatter?
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

	int* matrix;
	if (!rank)
	{
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
		#endif
		
	}
	procMatrix = (int*)malloc(pProcNum[rank]*mSize*sizeof(int));
	MPI_Datatype matrixString;
	MPI_Type_contiguous(mSize,MPI_INT, &matrixString);
	MPI_Type_commit(&matrixString);
	MPI_Scatterv(matrix,pProcNum,pProcInd,matrixString,procMatrix,pProcNum[rank],matrixString,0,MPI_COMM_WORLD);
	if(!rank)
	{
		#ifdef TEST
		for (i = 0; i < mSize; i++)
		{	
			for (int j = 0; j < mSize; j++)
			{
				printf("%d ", P_MATR(i, j));
			}
			printf("\n");
		}
		#endif
		free(matrix);
	}
	

	MST = (int*)malloc(sizeof(int)*mSize); // дерево вида MST[childInd] = parentInd

	for (int i = 0; i < mSize; i++)
	{
		MST[i] = -1;
	}
	

}

void PrimsAlgorithm()
{
	MST[0] = 0;
	weight = 0;

	int mini;
	int pred = 0;
	int succ = 0;

	struct { int miniValue; int rank; } miniRow, Row;
	Edge edge;
	for (int k = 0; k < mSize - 1; k++)
	{
		mini = INT_MAX;
		for (int i = 0; i < pProcNum[rank]; i++)
		{
			if (MST[i + pProcInd[rank]] != -1)
			{
				for (int j = 0; j < mSize; j++)
				{
					if (MST[j] == -1)
					{
						

						if (P_MATR(i, j) < mini && P_MATR(i, j) != 0)
						{
							mini = P_MATR(i, j);
							succ = j;
							pred = i;
							
						}
					}
				}
			}
		}
		Row.miniValue = mini;
		Row.rank = rank;

		MPI_Allreduce(&Row, &miniRow, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);
		edge.Pred = pred + pProcInd[rank];
		edge.Succ = succ;
		MPI_Bcast(&edge, 1, MPI_2INT, miniRow.rank, MPI_COMM_WORLD);

		MST[edge.Succ] = edge.Pred;
		weight += miniRow.miniValue;
	}
}
void ProcessTerminiation () 
{
	free(procMatrix);
	free(MST);
}

int main(int argc,char *argv[])
{
	double start, finish, duration; // для подсчета времени вычислений

	MPI_Init ( &argc, &argv );
	MPI_Comm_rank ( MPI_COMM_WORLD, &rank);
	MPI_Comm_size ( MPI_COMM_WORLD, &size );
	srand(time(NULL) + rank);
	if (!rank)
	{
		#ifdef TEST
		f_matrix = fopen("example", "r");
		fscanf(f_matrix, "%d\n", &mSize);
		#else
		if (argc < 2)
		{
			printf("No size parameter\n");
			MPI_Finalize();
			return -1;
		}
		else
		{
			mSize = atoi(argv[1]);
		}
		#endif
	}


	MPI_Bcast(&mSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

	ProcessInitialization();

	start = MPI_Wtime();
			
	PrimsAlgorithm();
	

	finish = MPI_Wtime();
	duration = finish-start;

	if (!rank) 
	{
		#ifdef TEST
		f_res = fopen("Result.txt", "w");
		fprintf(f_res,"%d\n ", weight);
		for (int i=0; i< mSize; i++)
		{
			fprintf(f_res,"%d %d\n",i, MST[i]);
		}
		fclose(f_res);
		#endif

		f_time = fopen("Time2.txt", "a+");
		fprintf(f_time, " Number of processors: %d\n Number of vertices: %d\n Time of execution: %f\n\n", size, mSize, duration);
  	fclose(f_time);
		printf("\n Number of processors: %d\n Time of execution: %f\n\n", size, duration);
	}


	// Завершение процесса вычислений
	ProcessTerminiation(); 
	MPI_Finalize();
    return 0;
}

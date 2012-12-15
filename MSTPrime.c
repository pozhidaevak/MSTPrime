#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


FILE *f_matrix, *f_time, *f_res;

int procQuantity;	// число доступных процессов
int procRank;	// ранг текущего процесса

int* matr;
int Size;

int procInd;

struct Edge { double Pred; int Succ; };

int* Vnew;
int weight;

int RowNum;
void ProcessInitialization()
{
	int i,j;     
	MPI_Status Status;
	printf("procRank = %d\n",procRank);
	if (procRank == 0)
	{
		f_matrix = fopen("example", "r");
		fscanf(f_matrix, "%d\n", &Size);
	}

	MPI_Bcast(&Size, 1, MPI_INT, 0, MPI_COMM_WORLD);

	int missingRows = Size;
	RowNum = Size/procQuantity;
	bool fl = false;
	missingRows = Size - RowNum * procQuantity;
	int l = 0;
	procInd = 0;
	if (missingRows != 0)
	{
		for (i = 0; i < procRank; i++) 
		{
			missingRows--;
			if (missingRows == 0)
			{
				fl = true;
				l = i;
				break;
			}
		}
		if (fl == false)
			RowNum++;
		MPI_Bcast(&l, 1, MPI_INT, procQuantity-1, MPI_COMM_WORLD);
	}
	else
		l = procQuantity - 1;
	if (procRank <= l)
	{
		procInd = RowNum * procRank;
	}
	else
	{
		procInd = (RowNum + 1) * (l+1) + RowNum * (procRank - l - 1);
	}

	matr = (int*)malloc(RowNum*Size*sizeof(int));
	if (procRank == 0)
	{
		int* matrix = (int*)malloc(Size*Size*sizeof(int));

		for (i = 0; i < RowNum; i++)
		{
			int buf;
			matrix[i*Size + i] = 99;
			matr[i*Size + i] = 99;
			for (j = i + 1; j < RowNum; j++)
			{
				fscanf(f_matrix, "%i", &buf);
				matrix[i*Size + j] = buf;
				matrix[j*Size + i] = buf;
				matr[i*Size + j] = buf;
				matr[j*Size + i] = buf;
			}
			for (; j < Size; j++)
			{
				fscanf(f_matrix, "%i", &buf);
				matrix[i*Size + j] = buf;
				matrix[j*Size + i] = buf;
				matr[i*Size + j] = buf;
			}
		}
		int destSize = RowNum;		
		int destRank = 1;
		int nextProcInd = RowNum;
		for (destRank = 1; destRank <= l; destRank++)
		{
			int k = 0;
			nextProcInd += destSize;
			for (; i < nextProcInd; i++)
			{	
				matrix[i*Size + i] = 99;
				for (int j = i + 1; j < Size; j++)
				{
					fscanf(f_matrix, "%i", &matrix[i*Size + j]);
					matrix[j*Size + i] = matrix[i*Size + j];
				}
				MPI_Send(&matrix[i*Size], Size, MPI_INT, destRank, k++, MPI_COMM_WORLD);
			}
		}
		destSize--;
		for (; destRank < procQuantity; destRank++)
		{
			int k = 0;
			nextProcInd += destSize;
			for (; i < nextProcInd; i++)
			{	
				matrix[i*Size + i] = 99;
				for (int j = i + 1; j < Size; j++)
				{
					fscanf(f_matrix, "%i", &matrix[i*Size + j]);
					matrix[j*Size + i] = matrix[i*Size + j];
				}
				MPI_Send(&matrix[i*Size], Size, MPI_INT, destRank, k++, MPI_COMM_WORLD);
			}
		}
		fclose(f_matrix);
		delete []matrix;
	}
	else
	{
		for (i = 0; i  < RowNum; i++)
		{
			MPI_Recv(&matr[i*Size], Size, MPI_INT, 0, i, MPI_COMM_WORLD, &Status);
		}
	}

	Vnew = new int [Size];

	for (int i = 0; i < Size; i++)
	{
		Vnew[i] = -1;
	}
	printf("rowNum = %d\n", RowNum);
	printf("size = %d\n", Size);
	for (i = 0; i < RowNum; i++)
	{	
		for (int j = 0; j < Size; j++)
		{
			printf("%d ", matr[i*Size + j]);
		}
		printf("\n");
	}

}

void PrimsAlgorithm()
{
	Vnew[0] = 0;
	weight = 0;

	int min;
	int pred = 0;
	int succ = 0;

	struct { double MinValue; int ProcRank; } minRow, Row;
	Edge edge;
	for (int k = 0; k < Size - 1; k++)
	{
		min = 99;
		for (int i = 0; i < RowNum; i++)
		{
			if (Vnew[i + procInd] != -1)
			{
				for (int j = 0; j < Size; j++)
				{
					if (Vnew[j] == -1)
					{
						if (matr[i*Size + j] < min)
						{
							min = matr[i*Size + j];
							succ = j;
							pred = i;
						}
					}
				}
			}
		}
		Row.MinValue = min;
		Row.ProcRank = procRank;

		MPI_Allreduce(&Row, &minRow, 1, MPI_DOUBLE_INT, MPI_MINLOC, MPI_COMM_WORLD);
		edge.Pred = pred + procInd;
		edge.Succ = succ;
		MPI_Bcast(&edge, 1, MPI_DOUBLE_INT, minRow.ProcRank, MPI_COMM_WORLD);

		Vnew[edge.Succ] = edge.Pred;
		weight += minRow.MinValue;
	}
}
void ProcessTermination () 
{
	delete [] matr;
	delete [] Vnew;
}

int main(int argc,char *argv[])
{
	double start, finish, duration; // для подсчета времени вычислений

	MPI_Init ( &argc, &argv );
	MPI_Comm_rank ( MPI_COMM_WORLD, &procRank);
	MPI_Comm_size ( MPI_COMM_WORLD, &procQuantity );

	ProcessInitialization();

	start = MPI_Wtime();
			
	PrimsAlgorithm();
	//if (!procRank)
	//{
	//	for (i = 0; i < Size; i++)
	//	{
	//			printf("%c %c\n", static_cast<char>(i + 'A'), static_cast<char>(Vnew[i] + 'A'));
	//	}
	//	printf("%d ",weight);
	//}

	finish = MPI_Wtime();
	duration = finish-start;

	if (procRank == 0) 
	{
		f_res = fopen("Solutions/Result2.txt", "w");
		fprintf(f_res,"%d ", weight);
		fprintf(f_res,"%d %d", 1, Vnew[1]);
		for (int i=2; i< Size; i++)
		{
			fprintf(f_res,",%d %d",i, (Vnew[i]));
		}
		fclose(f_res);

		f_time = fopen("Time2.txt", "a+");
		fprintf(f_time, " Number of processors: %d\n Number of vertices: %d\n Time of execution: %f\n\n", procQuantity, Size, duration);
		fclose(f_time);
	}

	// вывод результата
	if (procRank == 0)
	{
		printf("\n Number of processors: %d\n Time of execution: %f\n\n", procQuantity, duration);
	}

	// Завершение процесса вычислений
	ProcessTermination(); 
	MPI_Finalize();
    return 0;
}

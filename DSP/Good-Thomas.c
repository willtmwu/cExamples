#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define M_PI 3.14159265358979323846

typedef struct {
    double real;     
    double img;     
} Complex;

void DEBUG_VECTOR_PRINT(Complex* vector, int len){
    printf("[");
	for (int i=0; i<len; i++) {
		printf("{%.2f, %.2f} ", vector[i].real, vector[i].img);
	}
	printf("]\n");
}

void DEBUG_MATRIX_PRINT(Complex** matrix, int rows, int columns){
    if(matrix[0] == NULL){
        printf("ERR");
    }
    
	for (int i=0; i<rows; i++) {
        DEBUG_VECTOR_PRINT(matrix[i], columns);
	}
}

/*
Takes in a vector and its length to calculate the DFT. This uses the definintion of a DFT with real and imaginary parts
*/
void DFT(Complex* in, Complex* out, int len){
    for (int i=0 ; i<len ; i++) {
    	double realSum = 0;
		double imgSum  = 0;
		for (int j=0 ; j<len ; j++) {
			double arg = 2 * M_PI * i* j / len; // the argument for e^2pi*n*m/N DFT 
			double cosArg = cos(arg);
			double sinArg = sin(arg);
			realSum +=  in[j].real*cosArg + in[j].img*sinArg; // Calculate the real and imaginary part of the DFT
			imgSum  += -in[j].real*sinArg + in[j].img*cosArg; 
		}
		out[i].real = realSum;
		out[i].img = imgSum;
	}
}

//Perform DFT on the columns of a matrix. Copy in and out the answer from the DFT operation
void ColumnDFT(Complex** matrix, int rows, int columns){
	Complex* inSig = malloc(sizeof(Complex)*rows);
	Complex* outFft = malloc(sizeof(Complex)*rows);
	for(int i=0; i<columns; i++){
		for(int j=0; j<rows; j++){
			inSig[j].real = matrix[j][i].real;
            inSig[j].img = matrix[j][i].img;
		}
		DFT(inSig, outFft, rows); 
		for(int j=0; j<rows; j++){
			matrix[j][i] = outFft[j];
		}   
	}
	free(inSig);
	free(outFft);
}

//Perform DFT on the rows of a matrix
void RowDFT(Complex** matrix, int rows, int columns){
	for (int i=0; i<rows; i++){
		Complex* fftSig = malloc(sizeof(Complex)*columns);
		DFT(matrix[i],fftSig,columns);
		free(matrix[i]);
		matrix[i] = fftSig;
	}
}

//Load a vector into a matrix diagonally by the Good-Thomas algorithm. 
void LoadGTMatrix(Complex*vector, Complex** matrix, int rows, int columns){
	int N = rows*columns;
	int r = 0;
	int c = 0;
	for(int i = 0; i<N; i++){
		matrix[r][c] = vector[i];
		r = (++r)%rows;
		c = (++c)%columns;
	}
}

//Unload a vector into a matrix by the Chinese Remainder Theorem
void UnloadGTMatrix(Complex* vector, Complex** matrix, int rows, int columns){
	int N = rows*columns;
	for(int i=0; i<rows; i++){
		for(int j = 0; j<columns; j++){
			vector[ (columns*i + rows*j) % N] = matrix[i][j];
		}
	}
}

int main(int argc, char** argv){
	Complex inSignal[30] 		= { {0 ,0}, {1 ,0}, {2 ,0}, {3 ,0}, {4 ,0}, {5 ,0}, 
									{6 ,0}, {7 ,0}, {8 ,0}, {9 ,0}, {10,0}, {11,0}, 
									{12,0}, {13,0}, {14,0}, {15,0}, {16,0}, {17,0}, 
									{18,0}, {19,0}, {20,0}, {21,0}, {22,0}, {23,0}, 
									{24,0}, {25,0}, {26,0}, {27,0}, {28,0}, {29,0} };

	//Initiliase a matrix of numbers, numbers in the matrix are not important
    Complex **outSplitSignal2 = (Complex **) malloc(sizeof(Complex*)*5);
	for(int i = 0; i<5;i++){
        Complex * tmp2 = (Complex*) malloc (sizeof(Complex)*6);
        for(int j=0; j<6; j++){
            tmp2[j].real = i*6+j;
            tmp2[j].img = 0;
        }   
        outSplitSignal2[i] = tmp2;
	} 

	Complex outSignalVector2[30]; // Final DFT Vector

	printf("In Signal GT Matrix \n");
	LoadGTMatrix(inSignal, outSplitSignal2, 5, 6); 
	// Load the vector into a matrix diagonally
	DEBUG_MATRIX_PRINT(outSplitSignal2, 5, 6);
	printf("\n");
	ColumnDFT((Complex**)outSplitSignal2, 5, 6); 
	// 2D DFT, on columns then all rows of the matrix
	RowDFT((Complex**)outSplitSignal2, 5, 6);
	UnloadGTMatrix(outSignalVector2, outSplitSignal2, 5, 6); 
	// Unload the matrix into the vector by the chinese remainder algorithm
	printf("OUT Signal Matrix Good-Thomas DFT\n");
    DEBUG_VECTOR_PRINT(outSignalVector2, 30);
    printf("\n");
}

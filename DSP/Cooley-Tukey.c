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
Takes in a vector and its length to calculate the DFT. This uses the definintion of a DFT
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

//Perform DFT on the columns of a matrix
void ColumnDFT(Complex** matrix, int rows, int columns){
	for(int i=0; i<columns; i++){
		Complex* inSig = malloc(sizeof(Complex)*rows);
		Complex* outFft = malloc(sizeof(Complex)*rows);
		for(int j=0; j<rows; j++){
			inSig[j].real = matrix[j][i].real;
            inSig[j].img = matrix[j][i].img;
		}
		DFT(inSig, outFft, rows); 
		for(int j=0; j<rows; j++){
			matrix[j][i] = outFft[j];
		}   
	}
}

//Generate and return a Twiddle Matrix for the Cooley-Tukey Algorithm
Complex** TwiddleMatrix(int rows, int columns){
	int N = rows*columns;
	Complex** retTwiddle = malloc(sizeof(Complex*)*rows);
	for (int i=0; i<rows; i++) {
		Complex* rowTwiddle = malloc(sizeof(Complex)*columns);
		for (int j=0; j<columns; j++){
			double arg = 2 * M_PI * i* j / N;
			rowTwiddle[j].real = cos(arg);
			rowTwiddle[j].img  = -sin(arg);
		}
		retTwiddle[i] = rowTwiddle;
	}  
	return retTwiddle;
}

//Element-wise multiplication of 2 complex numbers within matrix of same dimensions.
void MultiplyTwiddle(Complex** signalMatrix, Complex** twiddleMatrix, int rows, int columns){
	for (int i=0; i<rows; i++) {
		for (int j=0; j<columns; j++) {
            double realPart = signalMatrix[i][j].real;
            double imgPart = signalMatrix[i][j].img;            
			signalMatrix[i][j].real = realPart*twiddleMatrix[i][j].real - imgPart*twiddleMatrix[i][j].img;
			signalMatrix[i][j].img  = imgPart*twiddleMatrix[i][j].real + realPart*twiddleMatrix[i][j].img;
		}
	}
}

//Perform DFT on the rows of a matrix
void RowDFT(Complex** matrix, int rows, int columns){
	for (int i=0; i<rows; i++){
		Complex* fftSig = malloc(sizeof(Complex)*columns);
		DFT(matrix[i],fftSig,columns);
		matrix[i] = fftSig;
	}
}

//Unload the Cooley-Tukey Matrix back onto a vector, column by column
void UnloadCTMatrix(Complex* vector, Complex** matrix, int rows, int columns){
	for(int i=0; i<rows; i++){
		for(int j=0; j<columns; j++){
			vector[i + j*rows] = matrix[i][j];
		}
	}
}

int main(int argc, char** argv){
	Complex inSignal[30] 		= { {0 ,0}, {1 ,0}, {2 ,0}, {3 ,0}, {4 ,0}, {5 ,0}, 
									{6 ,0}, {7 ,0}, {8 ,0}, {9 ,0}, {10,0}, {11,0}, 
									{12,0}, {13,0}, {14,0}, {15,0}, {16,0}, {17,0}, 
									{18,0}, {19,0}, {20,0}, {21,0}, {22,0}, {23,0}, 
									{24,0}, {25,0}, {26,0}, {27,0}, {28,0}, {29,0} };
	Complex outSignal[30];

	Complex **outSplitSignal = (Complex **) malloc(sizeof(Complex*)*5);
	for(int i = 0; i<5;i++){
        Complex * tmp = (Complex*) malloc (sizeof(Complex)*6);
        for(int j=0; j<6; j++){
            tmp[j].real = i*6+j;
            tmp[j].img = 0;
        }   
        outSplitSignal[i] = tmp;
	} // Initialise the vector, to overwrite with the DFT

	Complex outSignalVector[30];

	// General DFT
    printf("IN Signal\n");
	DEBUG_VECTOR_PRINT(inSignal, 30);
    printf("\n");
	
    DFT(inSignal, outSignal, 30);
    
    printf("OUT Signal DFT\n");
	DEBUG_VECTOR_PRINT(outSignal, 30); // Print the single-phase DFT
    printf("\n");
    
	//Cooley Tukey
    printf("In Signal CT Matrix\n");
    DEBUG_MATRIX_PRINT(&outSplitSignal[0], 5, 6); 
    printf("\n");
    
    ColumnDFT((Complex**)outSplitSignal, 5, 6); // Perform DFT only on the Columns
	MultiplyTwiddle((Complex**)outSplitSignal, (Complex**)TwiddleMatrix(5,6),5, 6); // Element-wise multiplication with the Twiddle
	RowDFT((Complex**)outSplitSignal, 5, 6); // Perform row DFT on all matrix rows
	UnloadCTMatrix((Complex*)outSignalVector, (Complex**)outSplitSignal, 5, 6); // Unload the matrix into a vector by columns

	printf("Twiddle Matrix\n");
    DEBUG_MATRIX_PRINT((Complex**)TwiddleMatrix(5,6),5, 6); // Double checking the twiddle matrix
    printf("\n");

	printf("OUT Signal Matrix Cooley-Tukey DFT\n");
    DEBUG_VECTOR_PRINT(outSignalVector, 30); // Final answer to using Cooley-Tukey Algorithm
    printf("\n");
}

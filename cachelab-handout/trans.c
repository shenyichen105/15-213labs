/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{

    //this method generates 259 misses for 32 X 32; 1027 misses for 64 X 64; 1743 misses for 64 X 64;

    //asssume size of the matrix is a multiple of 8
    int b_row, b_col, i, j;
    int b_row_size = 8;
    int b_col_size = 8;
    //temporary matrix to hold the block; this is to avoid the collision (conflict miss) of
    //caches blocks in A and B e.g row in the matrix block in A and column in the same matrix block in B can mapped to the same cahce block
    int temp_block[b_row_size][b_col_size];

    for(b_row = 0; b_row < N/b_row_size; b_row++){
        for(b_col = 0; b_col < M/b_col_size; b_col++){
            //code for transposing in each block
            for (i = b_row * b_row_size; i < (b_row+1) * b_row_size; i++){
                for (j = b_col * b_col_size; j < (b_col+1) * b_col_size; j++){
                    temp_block[i-b_row * b_row_size][j-b_col * b_col_size] = A[i][j];
                }
            }
            //when writing to B, using i in the inner loop
            for (j = b_col * b_col_size; j < (b_col+1) * b_col_size; j++){
                for (i = b_row * b_row_size; i < (b_row+1) * b_row_size; i++){
                    B[j][i] = temp_block[i-b_row * b_row_size][j-b_col * b_col_size];
                }
            }
        }
    }

    //deal with reminder of rows (reminder w/repect to the block size)
    if (b_row * b_row_size < N){
        for (b_col = 0; b_col < M/b_col_size; b_col++){
            for (i = b_row * b_row_size; i < N; i++){
                for (j = b_col * b_col_size; j < (b_col+1) * b_col_size; j++){
                    temp_block[i-b_row * b_row_size][j-b_col * b_col_size] = A[i][j];
                }
            }

            for (j = b_col * b_col_size; j < (b_col+1) * b_col_size; j++){
                for (i = b_row * b_row_size; i < N; i++){
                    B[j][i] = temp_block[i-b_row * b_row_size][j-b_col * b_col_size];
                }
            }
        }
    }

    //deal with reminder of coloumns (reminder w/repect to the block size)
    if (b_col * b_col_size < M){
        for (b_row = 0; b_row < N/b_row_size; b_row++){
            for (i = b_row * b_row_size; i < (b_row+1) * b_row_size; i++){
                for (j = b_col * b_col_size; j < M ;j++){
                    temp_block[i-b_row * b_row_size][j-b_col * b_col_size] = A[i][j];
                }
            }
            for (j = b_col * b_col_size; j < M ;j++){
                for (i = b_row * b_row_size; i < (b_row+1) * b_row_size; i++){
                    B[j][i] = temp_block[i-b_row * b_row_size][j-b_col * b_col_size];
                }
            }
        }
    }

    //deal with reminder of row and columns (uses regular simple transpose)
    if (b_col * b_col_size < M && b_row * b_row_size < N){
        int tmp;
        for (i = b_row * b_row_size; i < N; i++){
            for (j = b_col * b_col_size; j < M; j++){
                    tmp = A[i][j];
                    B[j][i] = tmp;
            }
        }
    }

}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


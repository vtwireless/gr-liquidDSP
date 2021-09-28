/*

   gcc -g -Wall less_complex_float.c -o less_complex_float 

 */


#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <stdbool.h>


int main(void) {

    complex float x;

    bool skip = false;

    while(1 == fread(&x, sizeof(complex float), 1, stdin))
        if((skip = (skip ? false: true)))
            fwrite(&x, sizeof(complex float), 1, stdout);


    return 0;

}

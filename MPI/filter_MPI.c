// Median Filter in C - MPI Solution
// 1/11/22

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <mpi.h>
#include "filter_MPI.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image/stb_image_write.h"

int main(int argc, char *argv[])
{

    double timer = 0.0;

    MPI_Init(NULL, NULL);
    int rank;
    int world;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world);

    // Timing the parallel program
    MPI_Barrier(MPI_COMM_WORLD);
    timer -= MPI_Wtime();

    // program should accept arguments for <inputfolder> <outputfolder> <filterwidth>
    if (argc < 4)
    {
        if (rank == 0)
        { 
            printf("You have not entered the correct number of arguments %d", argc);
        }
        MPI_Finalize();
        return(0);
    }

    DIR * inputfolder;
    DIR * outputfolder;
    struct dirent * entry;

    inputfolder = opendir(argv[1]);
    outputfolder = opendir(argv[2]);

    if(inputfolder == NULL)
    {
        if (rank == 0)
        {
            puts("Input directory does not exist"); 
        }
        MPI_Finalize();
        return(0);
    }

    if(outputfolder == NULL)
    {
        if (rank == 0)
        {
            puts("Output directory does not exist");
        }
        MPI_Finalize();
        return(0);
    }

    int n;
    if (argc == 4) 
    {
        // Only the root process count the number of files to save some computation time
        if (rank == 0)
        {
            n = num_images(inputfolder);
        }
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    } else {
        sscanf(argv[4],"%d",&n);
    }

    int my_start = 0, my_end = 0, workload = 0;

    thread_workload(n, world, rank, &my_start, &my_end, &workload);

    int j = 0;
    char inbuf[1024];
    char outbuf[1024];

    // Filter the images in the range allocated to the process
    while( (entry=readdir(inputfolder)))
    {
        if ( !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") )
        {}
        else 
        {
            if ((j >= my_start))
            {
                sprintf (inbuf, "%s/%s", argv[1], entry->d_name);
                sprintf (outbuf, "%s/%s", argv[2], entry->d_name);
                int cols, rows, channels;
                int size;
                sscanf (argv[3],"%d",&size);
                int sd2 = (size-1)/2;

                unsigned char *img = stbi_load(inbuf, &cols, &rows, &channels, 0);
                
                int col_offset = sd2*channels;
                //printf("%d %d\n",col_offset, (cols*channels)+col_offset);

                // now we add a block to test if the image loaded any data in
                if(img == NULL)
                {
                    printf("some error warning");
                    exit(1);
                }
                
                //printf("image loaded sucessfully\n");

                //stbi_write_png("img.png", cols, rows, channels, img, cols * channels);
                unsigned char * img_in = pad_image(img, cols, rows, channels, size);
                unsigned char * img_out = (unsigned char*)malloc((cols*rows*channels)*sizeof(unsigned char));

                for(int i = sd2; i < rows+sd2; ++i)
                {
                    for(int j = col_offset; j < (cols*channels)+col_offset; j += channels)
                    {
                        find_median(img_in,img_out,i,j,channels,size,sd2,col_offset,(cols*channels));
                    }
                }

                if(img_out == NULL)
                {
                    printf("some error warning");
                    exit(1);
                }

                stbi_write_png(outbuf, cols, rows, channels, img_out, cols * channels);

                
                stbi_image_free(img);
                free(img_in);
                free(img_out);
                

            }

            j++;

            if (j >= my_end)
            {
                break;
            }
        }
    }
    closedir(inputfolder);

    // Timing the parallel program
    MPI_Barrier(MPI_COMM_WORLD);
    timer += MPI_Wtime();

    MPI_Finalize();

    if (rank == 0)
    {
        printf("%f\n", timer);
    } 

    exit(0);
}

// I've tried to keep everything in a single function, because I don't know how we want to divvy up the parallel sections
unsigned char quick_median(unsigned char * arr, int lo, int hi, int k)
{    
    /*
        PARTITION HERE
    */
    // assign a position for the pivot element
    int pos = lo;
    
    int n = arr[hi];
    for (int i = lo; i <= (hi-1); ++i)
    {
        // if the current element is smaller than the right pivot
        // move to the left
        
        if (arr[i] <= n)
        {
            swap(&arr[pos], &arr[i]);
            ++pos;
        }
    }

    swap(&arr[pos], &arr[hi]);

    /*
        SEARCH HERE
    */
    // if the new position is the median
    if (k - 1 == pos - 1)
    {
        return arr[pos];
    }
    // search through left array if k is smaller
    if (pos - 1 > k - 1 )
    {
        return quick_median(arr, lo, pos-1, k);
    }
    // search through right array if k is larger
    else
    {
        // alter value of k to find val in upper portion
        return quick_median(arr, pos+1, hi, (k));
    }
}

void swap(unsigned char* first, unsigned char* second)
{
    unsigned char temp = *first;
    *first = *second;
    *second = temp;
}


// finds the median of a collection of pixels, using a quicksort algorithm
void find_median(unsigned char *src_img, unsigned char *out_img, int row, int col, int channels, int size, int sd2, int col_offset, int row_size)
{
    // create an array to determine the median of each channel
    unsigned char * chan_arr = (unsigned char*)malloc((size*size)*sizeof(unsigned char));

    // iterate per-channel
    for (int n = 0; n < channels; ++n)
    {
        int count = 0;
        // populate array with neighbourhood of size*size
        for (int i = row-sd2; i <= row+sd2; ++i)
        {
            for (int j = col-col_offset; j <= col + col_offset; j += channels)
            {
                chan_arr[count] = src_img[(i*row_size)+j+n];
                ++count;
            }
        }
        
        out_img[((row-sd2)*row_size)+(col-col_offset)+n] = (uint8_t) quick_median(chan_arr, 0, (size*size)-1, ((size*size)-1)/2);
        //printf("%d\n", quick_median(chan_arr, 0, (size*size)-1, ((size*size)-1)/2));
        /*printf("array to find median: [");
        for (int m = 0; m < count; ++m){
            printf("%d, ",chan_arr[m]);
        }
        printf("], median is %d\n\n", quick_median(chan_arr, 0, (size*size)-1, ((size*size)-1)/2));*/


    }
    free(chan_arr);
}


unsigned char * pad_image(unsigned char *src_img, int cols, int rows, int channels, int size)
{
    // get the padding size
    int sd2 = (size-1)/2;
    // get the column offset
    int col_offset = sd2 * channels;

    int pad_size = channels*((cols*sd2*2)+(rows*sd2*2)+(sd2*sd2*4));
    int img_size = rows*cols*channels;
    int row_size = cols*channels;

    // allocate new image array
    unsigned char * new_img = (unsigned char*)malloc((img_size+pad_size)*sizeof(unsigned char));

    // populate the columns above the image
    // no. rows should equal size/2 (sd2)
    for (int i = 0; i < sd2; ++i)
    {
        // increment the inner for loop with a stride, the number of channels per pixel
        for (int j = 0;  j < (cols*channels); j += channels)
        {
            // for each RGB value
            for (int k = 0; k < channels; ++k)
            {
                new_img[i*(row_size)+(j+col_offset)+k] = src_img[((sd2-i)*row_size)+j+k];
            }
        }
    }

    // populate the columns below the image
    for (int i = rows+sd2; i < (rows+(sd2*2)); ++i)
    {
        // increment the inner for loop with a stride, the number of channels per pixel
        for (int j = 0;  j < (cols*channels); j += channels)
        {
            // for each RGB value
            for (int k = 0; k < channels; ++k)
            {
                new_img[i*(row_size)+(j+col_offset)+k] = src_img[(rows-(i-rows))*(row_size)+j+k];
            }
        }
    }

    // populate with the original image data
    for (int i = 0; i < rows; ++i)
    {
        // increment the inner for loop with a stride, the number of channels per pixel
        for (int j = 0;  j < (row_size); j += channels)
        {
            // for each RGB value
            for (int k = 0; k < channels; ++k)
            {
                new_img[((i+sd2)*row_size)+(j+col_offset)+k] = src_img[i*(row_size)+j+k];
            }
        }
    }

  
    for (int i = 0; i < rows+sd2; ++i)
    {
        // populate the cols to the left of the image
        // increment the inner for loop with a stride, the number of channels per pixel
        for (int j = 0;  j < (col_offset); j += channels)
        {
            // for each RGB value
            for (int k = 0; k < channels; ++k)
            {
                new_img[i*(row_size)+j+k] = new_img[(i*row_size)+(col_offset+(col_offset-j)+k)];
            }
        }

        // populate the cols to the right of the image
        // increment the inner for loop with a stride, the number of channels per pixel
        for (int j = (row_size)+col_offset;  j < (row_size)+(col_offset*2); j += channels)
        {
            // for each RGB value
            for (int k = 0; k < channels; ++k)
            {
                new_img[i*(row_size)+j+k] = new_img[i*(row_size)+((row_size-(j-row_size))+k)];
            }
        }
    }

    return new_img;
}

int num_images (DIR * inputfolder)
{
    struct dirent * entry;
    int n = 0;
    // Count the number of images to be divided among nodes
    while( (entry=readdir(inputfolder)) )
    {
        if ( !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") )
        {}
        else 
        {
            n++;
        }
    }
    rewinddir(inputfolder);

    return n;
}

void thread_workload(int n, int p, int rank, int* my_start, int* my_end, int* workload)
{
    // Determine the workload of each ran
    int q = n / p;
    int r = n % p;

    if (rank < r)
    {
        *my_start = rank;
        *my_end = 1;
        *workload = 1;
    } else 
    {
        *my_start = r;
    }

    *my_start += (rank*q);
    *my_end += (*my_start + q);
    *workload += q;
}
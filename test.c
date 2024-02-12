#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <pthread.h>
#include "mypthread.h"
#include "umalloc.h"
ucontext_t context_a;
ucontext_t context_b;
ucontext_t context_main;
int a = 0;
int b = 1;
int c = 2;
int* total;
int sum;
mypthread_mutex_t mutex;
//mypthread_mutex_t mutex;  // Define a global mutex
//pthread_mutex_t mutex;
//This can be part of the thread-control-block later to know what thread was running
char whatThread;

void printA(void* threadID)
{
    
   // while(1)
   // {
        //whatThread = 'a';
        //puts("sleeping in a");
        //sleep(5);
        printf("This is thread before mutex: %c\n", threadID);
        pthread_mutex_lock(&mutex);
        printf("This is thread after mutex: %c\n", threadID);
        printf("I am thread: %c", threadID);
        //sleep(1);
        /* int i = 0;
        for(i = 0; i < 30; i++)
        {
            sum+=1;
        } */
        printf("In the thread with value: %d\n", (int)threadID);
        pthread_mutex_unlock(&mutex);
        printf("Im the thread that unlocked mutex %c\n", threadID);
        pthread_exit(threadID);
    //}
    
    
    
    
}

int printB(void* threadID)
{
    
   // while(1)
   // {
        //whatThread = 'a';
        
        mypthread_mutex_lock(&mutex);
        printf("This is thread: %c\n", threadID);
       
        sleep(1);
        mypthread_mutex_unlock(&mutex);
        //pthread_exit();
   // }
        
    return 1;
    
}
void printAAA()
{
    char a = 'a';
    printf("%c\n",a);
    
}
void printAA()
{
    char a = 'a';
    printf("%c\n",a);
    printAAA();
    
}

void printEcho(void* threadID)
{
    
    /* while(1)
    { */
        //whatThread = 'a';
        
           // pthread_mutex_lock(&mutex);
          // printMetaData();
           puts("In thread a");
          
             int *a = malloc(8186);
             puts("A is malloced");
            
            if(a == NULL)
            {
                puts("A is null");
            }
            else
            {
                puts("A is not null");
            }
            puts("Before giving a a value"); 
        
            *a = (int)threadID;
           // free(a);
            
           
            
            
            /* int *b = malloc(8);
            *b = (int)threadID + 1; */
           /* int a;
           int b;
           int c;
           int d; */
           //while(1)
            //{
            //printf("\nthis is sum %d\n", sum);
            /* printAA();
            printAA();
            printAA();
            printAA();
            printAA();
            printAA();
            printAA(); */
            printf("\n-------------This is thread:--------------- %c\n", threadID);
             //printf("\n-------------This is thread value pointer %p:--------------- \n", a);
            //printf("\n-------------This is thread value %d:--------------- \n", *a); 
            
            puts("Before mutex");
           mypthread_mutex_lock(&mutex);
            printf("This is thread in the mutex: %c\n", threadID);
            //*a = 10;
             printf("\n-------------This is thread:--------------- %c\n", threadID);
             //printf("\n-------------This is thread value pointer %p:--------------- \n", a);
           //printf("\n-------------This is thread value %d:--------------- \n", *a); 
            //sleep(1);
            puts("Before unlock");
            
            mypthread_mutex_unlock(&mutex); 
            printf("This is thread after the mutex: %c\n", threadID);
            //sleep(5);
            //exit(1);
          // printf("\n-------------This is thread value:--------------- %d\n", *a);
           //sleep(1);
            //puts("--------In main loop---------------------");
            //} 
             
             /* printf("-------------This is thread value:--------------- %d\n", *a);
             printf("-------------This is thread value:--------------- %d\n", *b); */
              //free(a);
             //free(b); 
           // printAllPageDebug();
            //free(a);
            //printAllPageDebug();
            pthread_exit((int)threadID);
           // pthread_mutex_unlock(&mutex);

    return;
    
}
/* void printB()
{
    
    while(1)
    {
        whatThread = 'b';
        sleep(1);
        puts("B");
    }
    
}


void printC()
{
    
    while(1)
    {
        whatThread = 'c';
        sleep(1);
        puts("C");
    }
    
} */

void mallocStuff()
{
    for(int i = 0; i < 700; i++)
    {
        int *a = malloc(3000);
    }
    puts("Done");
    pthread_exit(NULL);
    //return;
}
void singleThreadTest()
{
    pthread_t threadA;
    int returnVal = pthread_create(&threadA, NULL, (void*)printEcho, (void*)'a');
    printf("thread A return value %d\n", returnVal);
    
    pthread_join(threadA, NULL);
    
}

void twoThreadTest()
{
    pthread_t threadA;
    pthread_t threadB;
    int returnVal = pthread_create(&threadA, NULL, (void*)printEcho, (void*)'a');
    printf("%d", returnVal);
    
   int returnVala = pthread_create(&threadB, NULL, (void*)printEcho, (void*)'b');
    printf("%d", returnVal); 
    pthread_join(threadA, NULL);
    pthread_join(threadB, NULL);
}

int main()
{  
     //singleThreadTest();
    /* int *a = malloc(sizeof(int));
    printf("This is shalloc location for var %p\n", a);
    *a = 5; */

  /*    int *a = malloc(4);
    printf("This si a %p\n",a); 
    pthread_mutex_t test1_mutex;
    
    //pthread_t threadA;
    
    
    pthread_mutex_init(&test1_mutex, NULL);
    pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t) * 4);
    printf("This is address before load: %p\n", &thread[0]);
    /* loadLibraryPages(3);
    printf("This is address after load: %p\n", &thread[0]); */
    
   /* int returnVal = pthread_create(&thread[0], NULL, (void*)printEcho, (void*)'a');
    printf("This is the thread : %d\n", thread[0]);
    puts("Before the thing");
    exit(1); */
    //*a = 4;
  // printMetaData();
    /* for (int i = 0; i < 4; i++) {
        printf("This is pointer for thread[i] %p thread: %d\n", &thread[i], i);
        pthread_create(&thread[i], NULL, (void *) printEcho, (void*)i);
        printf("This is the thread : %d\n", thread[i]);
        printf("This is pointer for thread[i] %p thread: %d\n", &thread[i], i);
        printf("This is the thread number: %d\n", thread[i]);
    } */


  
    /* int *b = shalloc(sizeof(int));
     printf("This is shalloc location for var b%p\n", b);

     free(a);
     free(b);
     a = shalloc(16378);
     free(a);
    //printAllPageDebug();
    exit(1); */
       //mypthread_mutex_init(&mutex, NULL); 

    
      pthread_t threadA;
    int returnVal = pthread_create(&threadA, NULL, (void*)mallocStuff, (void*)'a'); 
    
     
     pthread_t threadB;
     returnVal = pthread_create(&threadB, NULL, (void*)mallocStuff, (void*)'b');   
    
     pthread_t threadC;
     returnVal = pthread_create(&threadC, NULL, (void*)mallocStuff, (void*)'c'); 
     puts("Mallocing in main");

/*     pthread_t threadD;
     returnVal = pthread_create(&threadD, NULL, (void*)mallocStuff, (void*)'d');  */
     puts("Mallocing in main");  

    /* 
    exit(1); */
  /*int *total = malloc(sizeof(int));
    puts("Join in main"); */
    
   pthread_join(threadA, &total);
    printf("Total after: %d\n", total);
    

    pthread_join(threadB, &total);
    printf("Total after: %d\n", total); 

     pthread_join(threadC, &total);
    printf("Total after: %d\n", total);  

   /*  pthread_join(threadD, &total);
    printf("Total after: %d\n", total);   */
   

    /* for(int i = 0; i < 2038 + 1; i++)
    {
        int *a = malloc(4084);
        
    } 
    int *a = malloc(1000); */

    /* for(int i = 0; i < 50 + 1; i++)
    {
        int *a = malloc(4084);
        
    } 
    int *b = malloc(1000);
    for(int i = 0; i < 2000 + 1; i++)
    {
        int *a = malloc(4084);
        
    } 

    *b = 5; */
    //printMetaData();
    int *c = malloc(3000);
    printf("This is pointer for c %p\n",c);
    *c = 4;
    printf("This is c value %d\n", *c);
    unlockMemoryPages();
    printMetaData();
    puts("In main exiting");
    
    exit(1);
    /*  pthread_join(threadC, &total);
    printf("Total after: %d\n", total); 

     pthread_join(threadD, &total);
    printf("Total after: %d\n", total);  */
    //exit(1);
    //printf("thread A return value %d\n", returnVal);
    /* int *a = malloc(sizeof(int));
    *a = 10;
    exit(1); */
    //int b = 10;
    //printf("Pointer for a: %p\n", a);
    //printf("--------In main loop value %d---------------------\n",*a);
   // printAllPageDebug();
    //exit(1);
  /*      while(1)
        {
            //printf("\nthis is sum %d\n", sum);
            puts("--------In main loop---------------------\n");
            //printf("--------In main loop value: %d---------------------\n", b);
            //exit(1);
           // printf("--------In main loop value %d---------------------\n",*a);
            //exit(1);
            sleep(1);
            
        }  */
    //pthread_join(threadA, NULL);
 /*    mypthread_mutex_init(&mutex, NULL); */
   
    //twoThreadTest();
    /* int *b = malloc(sizeof(int));
    *b = 100;
    printf("\nValue for main %d\n", *b);
    free(b);
    printf("In main\n"); */
    
   /*  pthread_t threadA;
    pthread_t threadB;
    pthread_t threadC;
    pthread_t threadD;
    
    printf("mutex address in main: %p\n", &mutex);
    int returnVal = pthread_create(&threadA, NULL, (void*)printA, (void*)'a');
    printf("%d", returnVal);
    
    puts("In main again");
    //pthread_join(threadA, NULL);


    returnVal = pthread_create(&threadB, NULL, (void*)printA, (void*)'b');
    
    //printf("%d\n", returnVal);
  

    //printf("total: %d \n", total);

    returnVal = pthread_create(&threadC, NULL, (void*)printA, (void*)'c');
    //printf("%d", returnVal);

    //puts("In main"); 


    returnVal = pthread_create(&threadD, NULL, (void*)printA, (void*)'d');
    //returnVal = mypthread_create(&threadC, NULL, (void*)printA, (void*)'c');
    //printf("%d", returnVal);
    //return;
    //sleep(1);
    
    pthread_join(threadA, &total);
    printf("Total after: %d\n", total);
    pthread_join(threadB, &total);
    printf("Total after: %d\n", total);
    pthread_join(threadC, &total);
    printf("Total after: %d\n", total);
    pthread_join(threadD, &total);
    printf("Total after: %d\n", total);   */
     
        
        //sleep(5);
        /*  while(1)
        {
            //printf("\nthis is sum %d\n", sum);
            sleep(1);
            //puts("--------In main loop---------------------");
        }  */
        //sleep(5);

    //printMetaData();

    //free(&total); 
    
    return 0;
}
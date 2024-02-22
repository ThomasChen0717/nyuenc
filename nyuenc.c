//References:  https://man7.org/linux/man-pages/man2/mmap.2.html. https://man7.org/linux/man-pages/man2/openat.2.html, https://pubs.opengroup.org/onlinepubs/009696699/functions/fstat.html, https://man7.org/linux/man-pages/man3/pthread_create.3.html,https://linux.die.net/man/3/pthread_cond_wait, https://linux.die.net/man/3/pthread_cond_signal, https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutex_lock.html, 
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define max_size 100000000
#define CHUNK 4096
int size;
int totalTask = 0;
int nextTask = 0;
int available = 0;
char *f_head;
int finishedTaskInd = 0;
unsigned char temp[2];


typedef struct{
    char *data;
    int size;
    unsigned char *encoded_data;
    int index;
    int completed;
} Task;
Task taskQueue[2500000];
Task completedQueue[2500000];
unsigned char *encoded_chunks[250000];
int queue_num = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t completed_cond = PTHREAD_COND_INITIALIZER;
char* readfile(char* argv[], int starting_index);
int encodefile(char* input, unsigned char* output, int size);
void *worker(); 
Task getNextTask();


void *worker(){
    while(1){
        pthread_mutex_lock(&mutex);
        while(available == 0){
            pthread_cond_wait(&task_cond, &mutex);
        }
        Task task = getNextTask();
        pthread_mutex_unlock(&mutex);
        int encoded_size = encodefile(task.data, task.encoded_data, task.size); 
        pthread_mutex_lock(&mutex);
        task.size = encoded_size;
        task.completed = 1;
        completedQueue[task.index] = task;
        pthread_cond_signal(&completed_cond);
        pthread_mutex_unlock(&mutex);
    }
}

int main(int argc, char *argv[]){
    (void)argc;
    int opt;
    int argument = -1;
    int file_index = -1;
    while((opt = getopt(argc, argv, "j:")) != -1){
        switch(opt) {
            case 'j':
                file_index = optind;
                argument = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Must provide an argument");
                exit(1);
        }
    }
    if(file_index != -1){
        pthread_t *workers = malloc(sizeof(pthread_t) * argument);  
        for(int i = 0; i < argument; i++){
            pthread_create(&workers[i], NULL, worker, NULL);
        }
        for(int i = file_index; i < argc; i++){
            f_head = readfile(argv, i); 
            int numTask = size % CHUNK != 0 ? size/CHUNK + 1: size/CHUNK;
            totalTask += numTask;
            for(int j = 0; j < numTask; j++){
                Task task;
                task.data = f_head + j * CHUNK;
                task.size = size - j * CHUNK >= CHUNK ? CHUNK: size - j * CHUNK;
                encoded_chunks[j] = malloc(sizeof(unsigned char) * CHUNK);
                task.encoded_data = encoded_chunks[j];
                task.completed = 0;
                task.index = queue_num;
                taskQueue[queue_num] = task;
                queue_num++;
                pthread_mutex_lock(&mutex);
                available++;
                pthread_cond_signal(&task_cond);
                pthread_mutex_unlock(&mutex);
            }
        }
        while (finishedTaskInd < totalTask) {
            pthread_mutex_lock(&mutex);
            while(completedQueue[finishedTaskInd].completed == 0){
                pthread_cond_wait(&completed_cond, &mutex);
            }
            pthread_mutex_unlock(&mutex);
            while(completedQueue[finishedTaskInd].completed == 1){
                if(finishedTaskInd == 0 && totalTask == 1){
                    fwrite(completedQueue[finishedTaskInd].encoded_data, sizeof(unsigned char), completedQueue[finishedTaskInd].size, stdout);
                }
                else if(finishedTaskInd == 0){
                    temp[0] = completedQueue[finishedTaskInd].encoded_data[completedQueue[finishedTaskInd].size - 2];
                    temp[1] = completedQueue[finishedTaskInd].encoded_data[completedQueue[finishedTaskInd].size - 1];
                    fwrite(completedQueue[finishedTaskInd].encoded_data, sizeof(unsigned char), completedQueue[finishedTaskInd].size - 2, stdout);
                }
                else if(finishedTaskInd == totalTask - 1){
                    if(completedQueue[finishedTaskInd].encoded_data[0] == temp[0]){
                        completedQueue[finishedTaskInd].encoded_data[1] += temp[1];
                    }
                    else{
                        fwrite(temp, sizeof(unsigned char), 2, stdout);
                    }
                    fwrite(completedQueue[finishedTaskInd].encoded_data, sizeof(unsigned char), completedQueue[finishedTaskInd].size, stdout);
                }
                else{
                    if(completedQueue[finishedTaskInd].encoded_data[0] == temp[0]){
                        completedQueue[finishedTaskInd].encoded_data[1] += temp[1];
                    }
                    else{
                        fwrite(temp, sizeof(unsigned char), 2, stdout);
                    }
                    temp[0] = completedQueue[finishedTaskInd].encoded_data[completedQueue[finishedTaskInd].size - 2];
                    temp[1] = completedQueue[finishedTaskInd].encoded_data[completedQueue[finishedTaskInd].size - 1];
                    fwrite(completedQueue[finishedTaskInd].encoded_data, sizeof(unsigned char), completedQueue[finishedTaskInd].size - 2, stdout);
                }
                finishedTaskInd++;
            }
        }
        munmap(f_head, size);
        free(workers);
    }
    else{
       unsigned char **encoded_f = malloc(sizeof(unsigned char*) * (argc - 1));
        int fsize[argc - 1]; 
        for(int i = 1; i < argc; i++){
            f_head = readfile(argv, i);
            encoded_f[i-1] = malloc(sizeof(unsigned char) * max_size);
            fsize[i-1] = encodefile(f_head, encoded_f[i-1], size);
            munmap(f_head, size);
        }
        for(int i = 0; i < argc - 1; i++){
            if(argc == 2){
                fwrite(encoded_f[i], sizeof(unsigned char), fsize[i], stdout);
            }
            else if(i == 0){
                temp[0] = encoded_f[i][fsize[i] - 2];
                temp[1] = encoded_f[i][fsize[i] - 1];
                fwrite(encoded_f[i], sizeof(unsigned char), fsize[i] - 2, stdout);
            }
            else if(i == argc - 2){
                if(encoded_f[i][0] == temp[0]){
                    encoded_f[i][1] += temp[1];
                }
                else{
                    fwrite(temp, sizeof(unsigned char), 2, stdout);
                }
                fwrite(encoded_f[i], sizeof(unsigned char), fsize[i], stdout);
            }
            else{
                if(encoded_f[i][0] == temp[0]){
                    encoded_f[i][1] += temp[1];
                }
                else{
                    fwrite(temp, sizeof(unsigned char), 2, stdout);
                }
                temp[0] = encoded_f[i][fsize[i] - 2];
                temp[1] = encoded_f[i][fsize[i] - 1];
                fwrite(encoded_f[i], sizeof(unsigned char), fsize[i] - 2, stdout);
            }
        }
        free(encoded_f);
    }
}

char* readfile(char* argv[], int index){
    int fd = open(argv[index], O_RDONLY);
    if (fd == -1){
        fprintf(stderr, "No such file");
        exit(0);
    }
    struct stat sb;
    if (fstat(fd, &sb) == -1){
        fprintf(stderr, "Error getting size");
        exit(0);
    }
    size = sb.st_size;

    char *addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED){
        fprintf(stderr, "Error mapping");
        exit(0);
    }
    close(fd);
    return addr;
}

int encodefile(char* input, unsigned char* output, int size){
    unsigned int count = 1;
    int unique_char = 0;
    char prev_char = input[0];
    for(int i = 1; i < size; i++){
        if(input[i] == prev_char){
            count++;
        }
        else{
            output[unique_char] = prev_char;
            output[unique_char + 1] = count;
            unique_char += 2;
            count = 1;
            prev_char = input[i];
        }
    }
    output[unique_char] = prev_char;
    output[unique_char + 1] = count;
    unique_char += 2;
    return unique_char;
}

Task getNextTask(){
    available--;
    return taskQueue[nextTask++];
}




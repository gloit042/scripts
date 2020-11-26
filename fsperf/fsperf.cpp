#include <thread>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <getopt.h>
#include <numeric>
#include <atomic>
#include <cstdlib>
using std::chrono::steady_clock;
using namespace std::literals::string_literals;
#define TOTAL_B 107374182400
// #define BUF_SIZE 1073741824
#define BUF_SIZE 262144
#define FILE_SIZE 4294967296
#define CLOCK_TYPE CLOCK_MONOTONIC
#define MAX_THREADS 20
typedef ssize_t (*func)(int,void *,size_t);
std::atomic_bool running;
long long io_sum[MAX_THREADS];
double elapse[MAX_THREADS];
const char *tplt = "<TEMPLATE-%d>";

void worker(int is_read, int bs, int id) {
    char *buf = (char *)malloc(BUF_SIZE);
    char filename[20];
    sprintf(filename, tplt, id);
    unsigned long long io = 0;
    unsigned idx = 0;
    struct timespec start;
    struct timespec end;
    long long secs = 0;
    long long nano_secs = 0;
    func f;
    while (!running.load(std::memory_order_acquire));
    if (is_read) {
        int fd = open(filename, O_DIRECT | O_SYNC);
        while (io <= TOTAL_B && running.load(std::memory_order_acquire)) {
            clock_gettime(CLOCK_TYPE, &start);
            unsigned long long res = 0;
            idx = 0;
            while (idx < BUF_SIZE) {
                auto ret = read(fd, buf + idx, bs);
                if (ret == 0) {
                    break;
                } else {
                    res += ret;
                }
                idx += bs;
            }
            clock_gettime(CLOCK_TYPE, &end);
            auto tv_sec = end.tv_sec - start.tv_sec;
            auto tv_nsec = end.tv_nsec - start.tv_nsec;
            nano_secs += tv_nsec;
            secs += tv_sec + nano_secs / 1000000000;
            nano_secs %= 1000000000;
            io += res;
            if (io % 4294967296 == 0) {
              lseek(fd, 0, SEEK_SET);
            }
        }
    } else {
        for (int i = 0;i < BUF_SIZE;++i) {
            buf[i] = rand() % 256;
        }
        int fd = open(filename, O_DIRECT | O_SYNC | O_RDWR);
        while (io <= TOTAL_B && running.load(std::memory_order_acquire)) {
            clock_gettime(CLOCK_TYPE, &start);
            auto res = write(fd, buf + idx, bs);
            clock_gettime(CLOCK_TYPE, &end);
            auto tv_sec = end.tv_sec - start.tv_sec;
            auto tv_nsec = end.tv_nsec - start.tv_nsec;
            nano_secs += tv_nsec;
            secs += tv_sec + nano_secs / 1000000000;
            nano_secs %= 1000000000;
            io += res;
            if (io % 4294967296 == 0) {
                lseek(fd, 0, SEEK_SET);
            }
            idx = (idx + bs) % BUF_SIZE;
        }
    }
    io_sum[id] = io;
    elapse[id] = secs + nano_secs / 1000000000.;
    std::atomic_thread_fence(std::memory_order_release);
    free(buf);
}

int main(int argc, char **argv) {
    int c;
    int is_read = 0;
    int thread_num = 1;
    long bs = 4 * 1024;
    bs = std::atoi(argv[2]);
    auto timeout = std::chrono::seconds(30);
    while ((c = getopt(argc, argv, ":rwt:b:")) != -1) {
        switch (c) {
            case 'r': is_read = 1; break;
            case 'w': is_read = 0; break;
            case 't':
                thread_num = std::atoi(optarg);
                if (thread_num >= 20 || thread_num <= 0) {
                    printf("thread numbers should be within [0, 20]!\n");
                    return -1;
                }
                break;
            case 'b':
                bs = std::atoi(optarg);
                break;
            case ':':
                printf("Option needs arg!\n");
                return -1;
            case '?':
                printf("Unknown option!\n");
                return -1;
            default:
                printf("Internal error: %c\n", c);
                return -1;
        }
    }
    std::vector<std::thread> workers;
    running.store(false, std::memory_order_release);
    for (int i = 0;i < thread_num;++i) {
        workers.push_back(std::thread(worker, is_read, bs, i));
    }
    long long io_total = 0;
    double elapse_total = 0;
    running.store(true, std::memory_order_release);
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        sleep(1);
    }
    running.store(false, std::memory_order_release);
    for (int i = 0;i < thread_num;++i) {
        workers[i].join();
        std::atomic_thread_fence(std::memory_order_acquire);
        io_total += io_sum[i];
        elapse_total += elapse[i];
    }
    double average = io_total / 1024 / 1024 / elapse_total * thread_num;

    std::cout << "threads: " << thread_num << " iosize:" << bs << " Average: " << average << "MiB/s" << " Latency: " << elapse_total / (io_total / bs) << std::endl;
    return 0;
}

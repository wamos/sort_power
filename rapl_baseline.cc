#include <vector>
#include <limits> 
#include <chrono>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include "workthread.h" 

inline uint64_t getNanoSecond(struct timespec tp){
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (1000000000) * (uint64_t)tp.tv_sec + tp.tv_nsec;
}

int main(int argc, char** argv) {
	std::string work_type(argv[1]);
	int duration = atoi(argv[2]);
	std::cout  << duration << "\n";
	int num_threads = atoi(argv[3]);
	std::cout  << num_threads << "\n";

	if(work_type == "basic"){
	  std::cout << "CPU Test" << "\n";
	  volatile uint32_t counter=0;
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(0, &cpuset);
      int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      struct timespec ts1,ts2;
      uint32_t cpu_start, cpu_end;
      cpu_start = getNanoSecond(ts1);
      while (counter < std::numeric_limits<uint32_t>::max()) {
        counter = counter + 1;
      }
      cpu_end = getNanoSecond(ts2);
      //std::cout << "counter:" << counter << "\n";
      std::cout << "CPU Time:" << +(cpu_end - cpu_start) << "\n";
	}
	else if(work_type == "mem"){
	  std::cout << "Memory Test" << "\n";
      //int duration = work.num1;
      //int size = work.num2;
      int size = 1000000000; // 1G
      int num  = 5; // useless
      std::vector<WorkThread> works;
      for(int i=0; i < num_threads; i++){
        WorkThread wth(i, num, size, "mem");
        works.push_back(std::move(wth));
      }

      //std::cout << "Start CPUs" << "\n";
      for(int i=0; i < num_threads; i++){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        works[i].startWork();
        int rc = pthread_setaffinity_np(works[i].getThreadHandle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }
      }

      std::this_thread::sleep_for(std::chrono::seconds(duration));

      //std::cout << "Stop CPUs" << "\n";
      for(int i=0; i < num_threads; i++){
        //std::cout << i << "\n";
        works[i].stopWork();
      }
  	}
    else { //if(work_type == "CPU")
      std::cout << "Multiple Cores" << "\n";
      std::vector<WorkThread> works;
      int num  = 5;
      int size = 0; 
      for(int i=0; i < num_threads; i++){
        WorkThread wth(i, num, size, "cpu");
        works.push_back(std::move(wth));
      }

      std::cout << "Start CPUs" << "\n";
      for(int i=0; i < num_threads; i++){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        works[i].startWork();
        int rc = pthread_setaffinity_np(works[i].getThreadHandle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }
      }

      std::this_thread::sleep_for(std::chrono::seconds(duration));

      std::cout << "Stop CPUs" << "\n";
      for(int i=0; i < num_threads; i++){
        works[i].stopWork();
      }
    }




	return 0;
}	
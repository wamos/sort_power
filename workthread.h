#include <iostream>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>
#include <limits> 
#include <random>
#include <chrono>
#include <stdint.h>
//#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


#define ROR8(val)               \
{                       \
    uint8_t tmp = val;          \
    const uint8_t bit0 = (tmp & 1) << 7;    \
    tmp >>= 1;              \
    tmp |= bit0;                \
    val = tmp;              \
}

class WorkThread {
public:
  WorkThread(int id, int num_, int size_, std::string type)
  : tid(id) 
  , num(num_)
  , size(size_)
  , work_type(type)
  , stop_flag(false)
  , counter(0)
  {
    fileio_buffer = new char[size];
  }

  WorkThread(WorkThread&& other) //noexcept 
  : work_thread(std::move(other).work_thread){
    //std::cout << "WorkThread move ctor" << "\n";
    *this = std::move(other);
  }

  WorkThread& operator= (WorkThread&& other){ //noexcept{
    //std::cout << "WorkThread move assign" << "\n";
    if(this!=&other){ // prevent self-move
      work_thread = std::move(other.work_thread);
      fileio_buffer = other.fileio_buffer;
      other.fileio_buffer=nullptr;

      tid = other.tid;  
      num = other.num;
      stop_flag = other.stop_flag;
      size = other.size;
      work_type = other.work_type;
      counter = other.counter;

      other.stop_flag=true;
      other.counter = 0;
      other.num = 0;
      other.size = 0;
    }
    return *this;
  }

  ~WorkThread(){
    std::cout << "WorkThread dtor" << "\n";
    delete fileio_buffer;
    stop_flag = true;
    if(work_thread.joinable())
      work_thread.join();
  }

  void startWork(){
    stop_flag = false;
    std::cout << "WorkThread "<< tid <<" starts on "<< work_type << "\n";
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if(work_type == "cpu"){
      work_thread = std::thread(&WorkThread::doCPUWork, this);
    }
    else if(work_type == "mem"){
      work_thread = std::thread(&WorkThread::stress_vm_flip, this);
    }
    else if(work_type == "file"){
      work_thread = std::thread(&WorkThread::doFileWork, this);
    }
    else{
      std::cout << "unknown type of work"<< work_type << ", nothing I can do\n";
    }
  }

  void stopWork(){
    std::cout << "WorkThread "<< tid <<" enter stop work" << "\n";
    stop_flag = true;
    work_thread.join();
    std::cout << "WorkThread "<< tid <<" join" << "\n";
  }

  void doCPUWork(){
    //while (stop_flag == false) {
    volatile uint32_t cpu_counter = 0;
    std::cout << "doCPUWork"<< tid <<"starts" << "\n";
    while(1){
        if(cpu_counter < std::numeric_limits<uint32_t>::max())
          cpu_counter = cpu_counter + 1;
        else {
          cpu_counter = 0;
          std::cout << "doCPUWork"<< tid <<"rolls over" << "\n";
        }

        if(stop_flag == true) 
          break;
    }
    std::cout << "doCPUWork"<< tid <<"stops" << "\n";
  }

  bool getStopFlag(){
    return stop_flag;
  } 

  // the flip from stress-vm
  // https://kernel.ubuntu.com/~cking/stress-ng/
  void stress_vm_flip(){
    std::cout << "stress vm flip starts" << "\n";
    uint8_t* buf = new uint8_t[size];
    const size_t sz = (size_t) size;
    volatile uint8_t *ptr;
    uint8_t *buf_end = buf + sz;
    uint8_t bit = 0x03;
    const size_t chunk_sz = sizeof(*ptr) * 8;
    for (ptr = buf; ptr < buf_end; ptr += chunk_sz) {
        uint8_t val = 3; //mwc8();
        *(ptr + 0) = val;
        ROR8(val);
        *(ptr + 1) = val;
        ROR8(val);
        *(ptr + 2) = val;
        ROR8(val);
        *(ptr + 3) = val;
        ROR8(val);
        *(ptr + 4) = val;
        ROR8(val);
        *(ptr + 5) = val;
        ROR8(val);
        *(ptr + 6) = val;
        ROR8(val);
        *(ptr + 7) = val;
    }
    //(void)mincore_touch_pages(buf, sz);

    for (int i = 0; i < 8; i++) {
        std::cout << "flip iter:" <<  i << "\n";
        ROR8(bit);
        for (ptr = buf; ptr < buf_end; ptr += chunk_sz) {
            *(ptr + 0) ^= bit;
            *(ptr + 1) ^= bit;
            *(ptr + 2) ^= bit;
            *(ptr + 3) ^= bit;
            *(ptr + 4) ^= bit;
            *(ptr + 5) ^= bit;
            *(ptr + 6) ^= bit;
            *(ptr + 7) ^= bit;
        }
        //(void)mincore_touch_pages(buf, sz);
    }

    delete buf;

  }

  void doMemWork(){
    // use std::vector reserve (size)
    std::vector<int> vec;
    vec.reserve(size);
    for(int i=0; i < size; i++){
      vec.push_back(1);
    }
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int> dis(1, size);

    uint64_t max = (uint64_t)num;
    //while(stop_flag==false){
    while (counter < max) {
        int index= dis(gen);
        vec[index/size] = index;
        counter = counter + 1;
    } 
  }

  void doFileWork(){
    const char* infilestr=infilename.c_str();
    int input_fd = open(infilestr, O_RDONLY);
    readFile(input_fd,fileio_buffer,size);

    const char* outfilestr = outfilename.c_str();
    int output_fd = open(outfilestr, O_WRONLY | O_CREAT, 0600);
    writeFile(output_fd,fileio_buffer,size);
  }

  std::thread::native_handle_type getThreadHandle(){
    return work_thread.native_handle();
  }

  inline uint64_t getNanoSecond(struct timespec tp){
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (1000000000) * (uint64_t)tp.tv_sec + tp.tv_nsec;
  }

  void setInputFile(std::string name){
    infilename = name;
  }

private:
  ssize_t writeFile(int fd, char* buffer, uint64_t buf_size){
      ssize_t totalBytes = 0;
      ssize_t write_size  = buf_size;
      char* writeBuffer;
      ssize_t numBytes;
      struct timespec ts1,ts2;
      while(write_size > 0){
          writeBuffer = (char*) (buffer + totalBytes);
          //write_start = getNanoSecond(ts1);
          numBytes = write(fd, writeBuffer, write_size);
          //rwrite_end = getNanoSecond(ts2);

          if(numBytes > 0){
              totalBytes = totalBytes + numBytes;
              write_size = write_size - numBytes;           
          }
          else{
              //printf("write() error %d: %s", errno, strerror(errno)); 
              std::cout << "nb:" << numBytes << "\n";        
          }
          
      }
      if(totalBytes > 0){
          std::cout << "write bytes:" << totalBytes << "\n";
          return totalBytes;
      }
      else{
          return 0;
      }
  }

  ssize_t readFile(int fd, char* buffer, uint64_t buf_size){
    ssize_t totalBytes = 0;
    ssize_t read_size  = buf_size;
    char* readBuffer;
    ssize_t numBytes;
    struct timespec ts1,ts2;
    //uint64_t read_start, read_end, read_all;
    while(read_size > 0){
        readBuffer = (char*) (buffer + totalBytes);
        //read_start = getNanoSecond(ts1);
        numBytes = read(fd, readBuffer, read_size);
        //read_end = getNanoSecond(ts2);

        if(numBytes > 0){
            totalBytes = totalBytes + numBytes;
            read_size = read_size - numBytes;
            //read_all = read_all + (read_end - read_start);              
        }    
    }
    if(totalBytes > 0){
        std::cout << "read bytes:" << totalBytes << "\n";
        return totalBytes;
    }
    else{
        return 0;
    }
  }  

  std::thread work_thread;
  uint64_t counter;
  int tid;  
  int num;
  int size;
  bool stop_flag = false;
  char* fileio_buffer;
  std::string outfilename = "/tmp/output.data";
  std::string infilename = "/tmp/10gb.input";
  std::string work_type = "JustWork";
};

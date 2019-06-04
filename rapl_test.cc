#include <iostream>
#include <ctime>
#include <inttypes.h>
#include "rapl_reader.h"   
#include <thread>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#define SEC 10
// g++ -std=c++11 test_rapl.cpp -o test_rapl -lm
// sh run.sh 20 8 cpu

int main(int argc, char** argv){
  std::string namestr(argv[1]);
  int duration = atoi(argv[2]);	
  struct timespec ts1,ts2;
  uint64_t read_start, read_end, timestamp;
  std::ofstream outfile;
  RAPLReader rapl_reader(0,0);

  timestamp = rapl_reader.getNanoSecond(ts1)/1000000000;
  outfile.open("/tmp/"+namestr+".txt");
  rapl_reader.setupRAPL();
  //read_start = rapl_reader.getNanoSecond(ts1);
  std::time_t t = std::time(0);
  std::cout << "start:" << t << "\n";
  double pre_pkg0 = 0;
  double pre_ram0 = 0;	
  double pre_pkg1 = 0;
  double pre_ram1 = 0;	
  for(int i = 0; i <= duration; i++){
    std::cout << std::time(0) << "\n";
    rapl_reader.runRAPL();
    auto pkg0 = rapl_reader.getEnergy(0,i);
	auto pkg1 = rapl_reader.getEnergy(1,i);
    if(i ==0){
      pre_pkg0 = std::get<0>(pkg0);
      pre_pkg1 = std::get<0>(pkg1);		
	  pre_ram0 = std::get<2>(pkg0);
	  pre_ram1 = std::get<2>(pkg1);	
      std::this_thread::sleep_for(std::chrono::seconds(1));
      timestamp+=1;
      continue;
    }
    else{
      //printf("Core-0 %f\n", std::get<0>(pkg0) - pre_pkg0);
      //std::cout << +(timestamp) << ","<< std::get<0>(pkg0) - pre_pkg0 << "," << std::get<2>(pkg0) - pre_ram0 << "\n";
      outfile << +(timestamp) << ","<< std::get<0>(pkg0) - pre_pkg0 << "," << std::get<2>(pkg0) - pre_ram0 << "," << std::get<0>(pkg1) - pre_pkg1 << "," << std::get<2>(pkg1) - pre_ram1 <<"\n";
      pre_pkg0 = std::get<0>(pkg0);
	  pre_pkg1 = std::get<0>(pkg1);
	  pre_ram0 = std::get<2>(pkg0);
	  pre_ram1 = std::get<2>(pkg1);
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    timestamp+=1;
  }
  t = std::time(0);
  std::cout << "end:" << t << "\n";
  outfile.close();	
  

 /* 
  auto package0_start = rapl_reader.getEnergy(0,0);
  auto package1_start = rapl_reader.getEnergy(1,0);
  auto package0_end   = rapl_reader.getEnergy(0,1);
  auto package1_end   = rapl_reader.getEnergy(1,1);

  std::cout << "package 0: \n"
            << "Package: " << std::get<0>(package0_end) - std::get<0>(package0_start) << "\n"
            << "PP0:     " << std::get<1>(package0_end) - std::get<1>(package0_start) << "\n"
            << "DRAM:    " << std::get<2>(package0_end) - std::get<2>(package0_start) << "\n";

  std::cout << "package 1: \n"
            << "Package: " << std::get<0>(package1_end) - std::get<0>(package1_start) << "\n"
            << "PP0:     " << std::get<1>(package1_end) - std::get<1>(package1_start) << "\n"
            << "DRAM:    " << std::get<2>(package1_end) - std::get<2>(package1_start) << "\n";
           
*/
 return 0;
}

/* Read the RAPL registers on recent (>sandybridge) Intel processors	*/
/*									*/
/* There are currently three ways to do this:				*/
/*	1. Read the MSRs directly with /dev/cpu/??/msr			*/
/*	2. Use the perf_event_open() interface				*/
/*	3. Read the values from the sysfs powercap interface		*/
/*									*/
/* MSR Code originally based on a (never made it upstream) linux-kernel	*/
/*	RAPL driver by Zhang Rui <rui.zhang@intel.com>			*/
/*	https://lkml.org/lkml/2011/5/26/93				*/
/* Additional contributions by:						*/
/*	Romain Dolbeau -- romain @ dolbeau.org				*/
/*									*/
/* For raw MSR access the /dev/cpu/??/msr driver must be enabled and	*/
/*	permissions set to allow read access.				*/
/*	You might need to "modprobe msr" before it will work.		*/
/*									*/
/* perf_event_open() support requires at least Linux 3.14 and to have	*/
/*	/proc/sys/kernel/perf_event_paranoid < 1			*/
/*									*/
/* the sysfs powercap interface got into the kernel in 			*/
/*	2d281d8196e38dd (3.13)						*/
/*									*/
/* Compile with:   gcc -O2 -Wall -o rapl-read rapl-read.c -lm		*/
/*									*/
/* Vince Weaver -- vincent.weaver @ maine.edu -- 11 September 2015	*/
/*									*/

#ifndef RAPL_READER_H
#define RAPL_READER_H

// C headers
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

// C++ headers
#include <tuple>
#include <vector>

//sys, linux headers
#include <sys/syscall.h>
#include <linux/perf_event.h>

#include "rapl_config.h"

class RAPLReader{
public:
	RAPLReader(int cores, int packages)
	:total_cores(cores)
	,total_packages(packages){
		energy_vec.reserve(10000); // for max length 60 sec that rapl counters won't overflow
	}

	std::tuple<double, double, double> getEnergy(int package_num, int timeslot){
		int index = 0;
		if(total_packages ==2){
			index = timeslot*2 + package_num; // sec0: 0 1, sec1: 0 1,
		}
		else{
			index = timeslot;
		}

		return energy_vec[index];
	}

	void setupRAPL(){
		cpu_model=detect_cpu();
		detect_packages();
		check_rapl_components(cpu_model);
		for(int i = 0; i < total_packages; i++) {	
			rapl_info_perpackage(i);
		}
		std::cout << "total_packages: " << total_packages << "\n";
	}

	void runRAPL(){
		int core=0;
		//printf("\n");
		// even index for package 0 in energy_vec
		// odd  index for package 1 in energy_vec
		for(int j = 0; j < total_packages; j++) {	
			auto tup = rapl_msr_perpackage(core, cpu_model, j);
			energy_vec.push_back(tup); 
		}
	}

	inline uint64_t getNanoSecond(struct timespec tp){
    	clock_gettime(CLOCK_REALTIME, &tp);
    	return (1000000000) * (uint64_t)tp.tv_sec + tp.tv_nsec;
  	}

private:
	int cpu_model;
	int total_cores=0;
	int total_packages=0;
	int package_map[MAX_PACKAGES];
	// common case on our machine: CPU_BROADWELL_EP, CPU_HASWELL_EP, CPU_SKYLAKE_X
	int pp0_avail=1;
	int pp1_avail=0;
	int dram_avail=1;
	/*CPU_BROADWELL_EP, CPU_HASWELL_EP, CPU_SKYLAKE_X*/
	int different_units=1;
	int psys_avail=0;
	//end case
	std::vector<std::tuple<double, double, double>> energy_vec;
	double package_energy[MAX_PACKAGES];
	double dram_energy[MAX_PACKAGES];
	double pp0_energy[MAX_PACKAGES];
	double power_units;
	double time_units;
	double cpu_energy_units[MAX_PACKAGES];
	double dram_energy_units[MAX_PACKAGES];
	double thermal_spec_power;
	double minimum_power;
	double maximum_power;
	double time_window;

	long long read_msr(int fd, int which) {
		uint64_t data;
		if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
			perror("rdmsr:pread");
			exit(127);
		}

		return (long long) data;
	}	


	int open_msr(int core) {
		char msr_filename[BUFSIZ];
		int fd;

		sprintf(msr_filename, "/dev/cpu/%d/msr", core);
		fd = open(msr_filename, O_RDONLY);
		if ( fd < 0 ) {
			if ( errno == ENXIO ) {
				fprintf(stderr, "rdmsr: No CPU %d\n", core);
				exit(2);
			} else if ( errno == EIO ) {
				fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
						core);
				exit(3);
			} else {
				perror("rdmsr:open");
				fprintf(stderr,"Trying to open %s\n",msr_filename);
				exit(127);
			}
		}

		return fd;
	}

	int detect_packages() {
		char filename[BUFSIZ];
		FILE *fff;
		int package;
		int i;
		for(i=0;i<MAX_PACKAGES;i++){
			package_map[i]=-1;
		}

		printf("\t");
		for(i=0;i<MAX_CPUS;i++) {
			sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
			fff=fopen(filename,"r");
			if (fff==NULL) break;
			fscanf(fff,"%d",&package);
			printf("%d (%d)",i,package);
			if (i%8==7) printf("\n\t"); else printf(", ");
			fclose(fff);

			if (package_map[package]==-1) {
				total_packages++;
				package_map[package]=i;
			}

		}

		printf("\n");
		total_cores=i;
		printf("\tDetected %d cores in %d packages\n\n", total_cores,total_packages);
		return 0;
	}

	int detect_cpu() {
		FILE *fff;

		int family,model=-1;
		char buffer[BUFSIZ],*result;
		char vendor[BUFSIZ];

		fff=fopen("/proc/cpuinfo","r");
		if (fff==NULL) return -1;

		while(1) {
			result=fgets(buffer,BUFSIZ,fff);
			if (result==NULL) break;

			if (!strncmp(result,"vendor_id",8)) {
				sscanf(result,"%*s%*s%s",vendor);

				if (strncmp(vendor,"GenuineIntel",12)) {
					printf("%s not an Intel chip\n",vendor);
					return -1;
				}
			}

			if (!strncmp(result,"cpu family",10)) {
				sscanf(result,"%*s%*s%*s%d",&family);
				if (family!=6) {
					printf("Wrong CPU family %d\n",family);
					return -1;
				}
			}

			if (!strncmp(result,"model",5)) {
				sscanf(result,"%*s%*s%d",&model);
			}

		}

		fclose(fff);

		printf("Found ");

		switch(model) {
			case CPU_SANDYBRIDGE:
				printf("Sandybridge");
				break;
			case CPU_SANDYBRIDGE_EP:
				printf("Sandybridge-EP");
				break;
			case CPU_IVYBRIDGE:
				printf("Ivybridge");
				break;
			case CPU_IVYBRIDGE_EP:
				printf("Ivybridge-EP");
				break;
			case CPU_HASWELL:
			case CPU_HASWELL_ULT:
			case CPU_HASWELL_GT3E:
				printf("Haswell");
				break;
			case CPU_HASWELL_EP:
				printf("Haswell-EP");
				break;
			case CPU_BROADWELL:
			case CPU_BROADWELL_GT3E:
				printf("Broadwell");
				break;
			case CPU_BROADWELL_EP:
				printf("Broadwell-EP");
				break;
			case CPU_SKYLAKE:
			case CPU_SKYLAKE_HS:
				printf("Skylake");
				break;
			case CPU_SKYLAKE_X:
				printf("Skylake-X");
				break;
			case CPU_KABYLAKE:
			case CPU_KABYLAKE_MOBILE:
				printf("Kaby Lake");
				break;
			case CPU_KNIGHTS_LANDING:
				printf("Knight's Landing");
				break;
			case CPU_KNIGHTS_MILL:
				printf("Knight's Mill");
				break;
			case CPU_ATOM_GOLDMONT:
			case CPU_ATOM_GEMINI_LAKE:
			case CPU_ATOM_DENVERTON:
				printf("Atom");
				break;
			default:
				printf("Unsupported model %d\n",model);
				model=-1;
				break;
		}

		printf(" Processor type\n");

		return model;
	}

	void check_rapl_components(int cpu_model){
		if (cpu_model<0) {
			printf("\tUnsupported CPU model %d\n",cpu_model);
			//return -1;
		}

		switch(cpu_model) {

			case CPU_SANDYBRIDGE_EP:
			case CPU_IVYBRIDGE_EP:
				pp0_avail=1;
				pp1_avail=0;
				dram_avail=1;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_HASWELL_EP:
			case CPU_BROADWELL_EP:
			case CPU_SKYLAKE_X:
				pp0_avail=1;
				pp1_avail=0;
				dram_avail=1;
				different_units=1;
				psys_avail=0;
				break;

			case CPU_KNIGHTS_LANDING:
			case CPU_KNIGHTS_MILL:
				pp0_avail=0;
				pp1_avail=0;
				dram_avail=1;
				different_units=1;
				psys_avail=0;
				break;

			case CPU_SANDYBRIDGE:
			case CPU_IVYBRIDGE:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=0;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_HASWELL:
			case CPU_HASWELL_ULT:
			case CPU_HASWELL_GT3E:
			case CPU_BROADWELL:
			case CPU_BROADWELL_GT3E:
			case CPU_ATOM_GOLDMONT:
			case CPU_ATOM_GEMINI_LAKE:
			case CPU_ATOM_DENVERTON:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=1;
				different_units=0;
				psys_avail=0;
				break;

			case CPU_SKYLAKE:
			case CPU_SKYLAKE_HS:
			case CPU_KABYLAKE:
			case CPU_KABYLAKE_MOBILE:
				pp0_avail=1;
				pp1_avail=1;
				dram_avail=1;
				different_units=0;
				psys_avail=1;
				break;
		}

	}

	void rapl_info_perpackage(int index){
		int fd;
		long long result;
		printf("\tListing paramaters for package #%d\n",index);

		fd=open_msr(package_map[index]);

		/* Calculate the units used */
		result=read_msr(fd,MSR_RAPL_POWER_UNIT);

		power_units=pow(0.5,(double)(result&0xf));
		cpu_energy_units[index]=pow(0.5,(double)((result>>8)&0x1f));
		time_units=pow(0.5,(double)((result>>16)&0xf));

		/* On Haswell EP and Knights Landing */
		/* The DRAM units differ from the CPU ones */
		if (different_units) {
			dram_energy_units[index]=pow(0.5,(double)16);
			printf("DRAM: Using %lf instead of %lf\n",
				dram_energy_units[index],cpu_energy_units[index]);
		}
		else {
			dram_energy_units[index]=cpu_energy_units[index];
		}

		printf("\t\tPower units = %.3fW\n",power_units);
		printf("\t\tCPU Energy units = %.8fJ\n",cpu_energy_units[index]);
		printf("\t\tDRAM Energy units = %.8fJ\n",dram_energy_units[index]);
		printf("\t\tTime units = %.8fs\n",time_units);
		printf("\n");

		/* Show package power info */
		result=read_msr(fd,MSR_PKG_POWER_INFO);
		thermal_spec_power=power_units*(double)(result&0x7fff);
		printf("\t\tPackage thermal spec: %.3fW\n",thermal_spec_power);
		minimum_power=power_units*(double)((result>>16)&0x7fff);
		printf("\t\tPackage minimum power: %.3fW\n",minimum_power);
		maximum_power=power_units*(double)((result>>32)&0x7fff);
		printf("\t\tPackage maximum power: %.3fW\n",maximum_power);
		time_window=time_units*(double)((result>>48)&0x7fff);
		printf("\t\tPackage maximum time window: %.6fs\n",time_window);

		/* Show package power limit */
		result=read_msr(fd, MSR_PKG_RAPL_POWER_LIMIT);
		printf("\t\tPackage power limits are %s\n", (result >> 63) ? "locked" : "unlocked");
		double pkg_power_limit_1 = power_units*(double)((result>>0)&0x7FFF);
		double pkg_time_window_1 = time_units*(double)((result>>17)&0x007F);
		printf("\t\tPackage power limit #1: %.3fW for %.6fs (%s, %s)\n",
			pkg_power_limit_1, pkg_time_window_1,
			(result & (1LL<<15)) ? "enabled" : "disabled",
			(result & (1LL<<16)) ? "clamped" : "not_clamped");
		double pkg_power_limit_2 = power_units*(double)((result>>32)&0x7FFF);
		double pkg_time_window_2 = time_units*(double)((result>>49)&0x007F);
		printf("\t\tPackage power limit #2: %.3fW for %.6fs (%s, %s)\n", 
			pkg_power_limit_2, pkg_time_window_2,
			(result & (1LL<<47)) ? "enabled" : "disabled",
			(result & (1LL<<48)) ? "clamped" : "not_clamped");

		close(fd);

	}

	std::tuple<double, double, double> rapl_msr_perpackage(int core, int cpu_model, int index) {
		// rapl_msr
		int fd;
		long long result;
		fd=open_msr(package_map[index]);

		/* Package Energy */
		result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
		package_energy[index]=(double)result*cpu_energy_units[index];

		/* PP0 energy */
		/* Not available on Knights* */
		/* Always returns zero on Haswell-EP? */
		if (pp0_avail) {
			result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
			pp0_energy[index]=(double)result*cpu_energy_units[index];
		}

		/* Updated documentation (but not the Vol3B) says Haswell and	*/
		/* Broadwell have DRAM support too				*/
		if (dram_avail) {
			result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
			dram_energy[index]=(double)result*dram_energy_units[index];
		}
		close(fd);
		auto energy_tup =std::make_tuple(package_energy[index], pp0_energy[index], dram_energy[index]);
		return energy_tup;
	}	

};	

#endif //RAPL_READ_H

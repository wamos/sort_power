	# https://preshing.com/20170522/learn-cmakes-scripting-language-in-15-minutes/
	cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
	project(rapl_project CXX)
	#message("L1 Data Cache Size: ${L1DSZ}") 
	#message("Number of Cores   : ${CORES}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -lrt -pthread -O2")
	add_executable(rapl_baseline rapl_baseline.cc)
	add_executable(rapl_measure  rapl_test.cc) 

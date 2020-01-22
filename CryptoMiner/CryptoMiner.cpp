// CryptoMiner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <picosha2.h>
#include <ctime>
#include <thread>
#include <mutex>
#include <chrono>
#include <mpi.h>
#include <HTTPRequest.hpp>


#define MASTER 0
#define TAG 1

std::mutex m;

std::string convertToString(char* a, int size) 
{ 
    int i;
    std::string s = ""; 
    for (i = 0; i < size; i++) { 
        s = s + a[i]; 
    } 
    return s; 
} 

std::string generateRandomString(const int length) {

	static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	std::string word;
	
	for (int i = 0; i < length; ++i) {
		word += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return word;
}

void mine(int nonce, int &count, int count_limit, int nonce_increment, std::string &returnString) {
	while (true && count < count_limit) {

		// region Get timestamp
		std::string random;
		time_t now = time(nullptr);
		struct tm timeinfo;
		gmtime_s(&timeinfo, &now);
		char timestamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
		strftime(timestamp, sizeof timestamp, "%FT%TZ", &timeinfo);
		// endregion
		
		std::vector<unsigned char> hash(picosha2::k_digest_size);
		random = generateRandomString(16) + std::to_string(nonce) + timestamp;

		// region Hash block
		picosha2::hash256(random.begin(), random.end(), hash.begin(), hash.end());
		// endregion


		// Check if block hash matches required parameters
		if (picosha2::bytes_to_hex_string(hash.begin(), hash.end()).substr(0, 3) == "000") {

			m.lock();
			if (count >= count_limit) {
				m.unlock();
				break;
			}


			std::string jsonConstruct = "{'timestamp': '" + std::string(std::begin(timestamp), std::end(timestamp)) + "','data': '" + random + "','hash': '" + picosha2::bytes_to_hex_string(hash.begin(), hash.end()) + "'},";
			
			// JSON output
			std::cout << jsonConstruct << std::endl;
			(returnString) += jsonConstruct;
			
			// Temporary because there are no HTTP api calls at the moment but the hashing works
			count++;
			m.unlock();
		}

		nonce+=nonce_increment;
	}
}

std::string startWork(int max_threads, int &count, int count_limit){
	std::string returnString = "";
	std::thread* thread;
	thread = new std::thread[max_threads - 1];

	for (int i = 0; i < max_threads - 1; i++) {
		thread[i] = std::thread(mine, i, std::ref(count), count_limit, max_threads, std::ref(returnString));	// std::ref() - that's how you pass parameter by reference when creating a thread
	}

	mine(max_threads-1, count, count_limit, max_threads, returnString);	// main thread is also doing the work

	for (int i = 0; i < max_threads - 1; i++) {
		thread[i].join();	// wait for all the threads
	}

	return returnString;
	// std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();	// time start for testing purposes
	
	// Creating ('max_threads'-1) threads. 1 less because the main thread should also be doing the work. Therefore we have: 1 main thread + ('max_threads'-1) side threads = 'max_threads' threads.
	
	//std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();	// time end for testing purposes
	//std::cout << "Time : " << std::chrono::duration_cast<std::chrono::milliseconds> (end - begin).count() << "[ms]" << std::endl; // elapsed time for testing purposes
}

int main() {
	
	int nonce = 0, count = 0, count_limit = 5; // count_limit - number of blocks that we want to get mined
	//int max_threads = std::thread::hardware_concurrency(); // hardware_concurrency() - Returns number of concurrent threads supported
	int max_threads = 2, world_rank, world_size; // 2 for testing purposes

	/*
	Inicializacija MPI
	MPI_THREAD_FUNNELED: If the process is multithreaded, only the thread that called MPI_Init_thread will make MPI calls.
	*/
	MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank); //get rank
	MPI_Comm_size(MPI_COMM_WORLD, &world_size); //get number of elements

	std::string* data = new std::string[world_size];
	http::Request request("http://postman-echo.com/post"); // set request
		
	if (world_rank == MASTER) {
		data[world_size - 1] = startWork(max_threads, count, count_limit);
		int max = world_size - 1;

		/*
		pridobimo vse podatke podprocesov
		*/
		for(int i = 0; i < max; i++){
			int size, SOURCE = i + 1;
			MPI_Recv(&size, 1, MPI_INT, SOURCE, TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE); //get size of incoming array

			char *tmp = new char[size];
			MPI_Recv(tmp, size, MPI_CHAR, SOURCE, TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE); //get char array

			data[i] = convertToString(tmp, size);

			delete[] tmp;
		}

		
		try {
			const http::Response postResponse = request.send("POST", std::string(data->begin(), data->end()), { "Content-Type: application/json" });

			std::cout << "|| REQUEST RESPONSE: " << std::string(postResponse.body.begin(), postResponse.body.end()) << std::endl;
		}
		catch (const std::exception & e) {

			std::cerr << "REQUEST FAILED: " << e.what() << std::endl;
		}
		
		
	} else {
		std::string data = startWork(max_threads, count, count_limit);
		MPI_Send(reinterpret_cast<void*>(data.length()), 1, MPI_INT, MASTER, TAG, MPI_COMM_WORLD); //send size of incoming array
		MPI_Send(data.c_str(), data.size(), MPI_CHAR, MASTER, TAG, MPI_COMM_WORLD); //send char array
	}

	MPI_Finalize();

	delete[] data;

	return 0; //success
}
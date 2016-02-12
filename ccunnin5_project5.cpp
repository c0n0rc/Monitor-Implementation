/*
Name: Conor Cunningham
Lab: Project 5
Compiling instructions: g++ -std=c++11 -lpthreads ccunnin5_project5.cpp

Deviations from Design Document:
Misunderstood project document - only file being used is the output file. Database is two time variables defined in main.
Monitor is a global struct with its own functions and variables.
Included a mutex rather than a semaphore for writing to the output file.
*/

#define _CRT_SECURE_NO_WARNINGS 
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>
#include <mutex> 
#ifndef WIN32
#include <sys/times.h>
#include <unistd.h>
#else
#include <windows.h> 
#endif       
using namespace std;

struct timeb timestruct;

//monitor - has its own variables that are protected from threads
typedef struct monitor_critical_section {
 	pthread_mutex_t mut_lock;
    pthread_cond_t begin_write, begin_read;
	int s, writers_waiting, readers_waiting;
	unsigned short m;
	string output;
	stringstream ss;
	monitor_critical_section() {				//constructor to set all values to 0
		s = 0;
		m = 0;
		writers_waiting = 0;
		readers_waiting = 0;
		pthread_mutex_init(&mut_lock, NULL);	
  		pthread_cond_init(&begin_write, NULL);	
		pthread_cond_init(&begin_read, NULL);	
	}
	~monitor_critical_section(){
		pthread_mutex_destroy(&mut_lock);
		pthread_cond_destroy(&begin_write);
		pthread_cond_destroy(&begin_read);	
	}

	string read() {								//monitor allows thread to read time values within monitor
		pthread_mutex_lock(&mut_lock);
		if (writers_waiting != 0) pthread_cond_wait(&begin_read, &mut_lock);
		readers_waiting++;
		ss << s << ":" << m;
		ss >> output;
		ss.clear();
		ss.str(std::string());
		readers_waiting--;
		pthread_cond_signal(&begin_write);
		pthread_mutex_unlock(&mut_lock);
		return output;
	}
	void write(int &sss, unsigned short &mmm) { //monitor sets the values, writer thread only passes these values in
		pthread_mutex_lock(&mut_lock);
		writers_waiting++;
		if (readers_waiting != 0) pthread_cond_wait(&begin_write, &mut_lock);
		s = sss;
		m = mmm;
		writers_waiting--;
		if (writers_waiting == 0) pthread_cond_broadcast(&begin_read); 
		pthread_cond_signal(&begin_write);
		pthread_mutex_unlock(&mut_lock);
	}
} monitor;

monitor *mon = new monitor;

//struct to pass information to threads
typedef struct information {
	fstream *output_file;
	int id,
		r_delay,
		w_delay,
		s;
	unsigned short m;
	information(){
		id = 0;
		r_delay = 0;
		w_delay = 0;
		s = 0;
		m = 0;
	}
} info;

void* reader(void *data);
void* writer(void *data);

mutex mtx;       					    // mutex for critical section

int main(int argc, char const *argv[]) {
	fstream fileOutput;
	string line;
	char r_input[255];					//buffers for scanf
	char w_input[255];					//buffers for scanf
	char R_input[255];					//buffers for scanf
	char W_input[255];					//buffers for scanf
	int ret, exit;
	int R = 0,							//reader's delay to restart
		W = 0,							//writer's delay to restart
		r = 0,							//number of readers
		w = 0,							//number of writers
		sss = 0;
	unsigned short mmm = 0;

	fileOutput.open("output.txt", fstream::out);		//file for thread output
	if (!fileOutput) {
		perror("Error opening output file for reading");
		return 1;
	}

	printf("Enter the number of readers: ");
	fgets(r_input, sizeof(r_input), stdin);
	size_t len = strlen(r_input) - 1; 			//removes newline from input
	if (r_input[len] == '\n') r_input[len] = '\0';

	printf("Enter the number of writers: ");
	fgets(w_input, sizeof(w_input), stdin);
	len = strlen(w_input) - 1; 					//removes newline from input
	if (w_input[len] == '\n') w_input[len] = '\0';

	printf("Enter delay for reader to restart (ms): ");
	fgets(R_input, sizeof(R_input), stdin);
	len = strlen(R_input) - 1; 					//removes newline from input
	if (R_input[len] == '\n') R_input[len] = '\0';

	printf("Enter delay for writer to restart (ms): ");
	fgets(W_input, sizeof(W_input), stdin);
	len = strlen(W_input) - 1; 					//removes newline from input
	if (W_input[len] == '\n') W_input[len] = '\0';

	//============== turn fgets buffers into ints // 
	stringstream ss;
	for (unsigned i = 0; i < sizeof(r_input) / sizeof(r_input[0]); ++i) { ss << r_input[i]; }
	ss >> r;
	ss.clear();
	ss.str(std::string());

	for (unsigned i = 0; i < sizeof(w_input) / sizeof(w_input[0]); ++i) { ss << w_input[i]; }
	ss >> w;
	ss.clear();
	ss.str(std::string());

	for (unsigned i = 0; i < sizeof(R_input) / sizeof(R_input[0]); ++i) { ss << R_input[i]; }
	ss >> R;
	ss.clear();
	ss.str(std::string());

	for (unsigned i = 0; i < sizeof(W_input) / sizeof(W_input[0]); ++i) { ss << W_input[i]; }
	ss >> W;
	ss.clear();
	ss.str(std::string());
	//==============//

	pthread_t* thr_id = new pthread_t[r + w];
	int num_readers = 0;
	int num_writers = 0;
	for (int i = 0; i < (r + w); i++) {

		//this struct is passed to every thread
		info *data = new info;
		data->output_file = &fileOutput;
		data->r_delay = R;
		data->w_delay = W;
		data->s = sss;
		data->m = mmm;

		if (num_readers < r) {
			data->id = num_readers;
			ret = pthread_create(&thr_id[i], NULL, &reader, (void*)data);
			num_readers++;
		}
		else {
			data->id = num_writers;
			ret = pthread_create(&thr_id[i], NULL, &writer, (void*)data);
			num_writers++;
		}
		if (ret != 0) {
			perror("Error: pthread_create() failed\n");
			free(data);
			return 1;
		}
	}

	for (int k = 0; k < (r + w); k++){
		pthread_join(thr_id[k], NULL);
	}
	fileOutput.close();
	
	//output results
	ifstream input;
	input.open("output.txt");		
	if (!input) {
		perror("Error opening output file for reading");
		return 1;
	}
	if (input) {
	    while (getline(input, line)){
	        cout << line << '\n';
	    }
	}

	printf("PRESS ANY KEY AND ENTER TO EXIT PROGRAM: \n");
	scanf("%x", &exit);
	input.close();
	delete[] thr_id;
	return 0;
}

void *reader(void *data) {
	//get data from struct
	info *thread_data = (info*)data;
	fstream *output = thread_data->output_file;
	int id = thread_data->id;
	int delay = thread_data->r_delay;
	int access_data = 10;
	//access monitor
	do {
		string finished_time = mon->read(); //call monitor function
		mtx.lock();
		(*output) << ">>> DB value read =: " << finished_time << " by reader number: " << id << endl;
		// cout << ">>> DB value read =: " << finished_time << " by reader number: " << id << endl;
		mtx.unlock();
		access_data--;

		//delay
		#ifndef WIN32
				usleep(delay * 1000);
		#else
				Sleep(delay);
		#endif

	} while (access_data > 0);
	delete thread_data;
	return 0;
}

void *writer(void *data) {
	//get data from struct
	info *thread_data = (info*)data;
	fstream *output = thread_data->output_file;
	int id = thread_data->id;
	int delay = thread_data->w_delay;
	int access_data = 10;
	stringstream ss;
	//access monitor
	do {
		//=========== get time the writer thread tries to access the monitor //
		ftime(&timestruct);
		time_t secs = timestruct.time;
		ss << secs;
		thread_data->s = atoi(ss.str().substr( ss.str().size() - 3, ss.str().size() ).c_str());
		thread_data->m = timestruct.millitm;
		ss.clear();
		ss.str(std::string());
		//=========== //
		mon->write(thread_data->s, thread_data->m);	//pass the current seconds and milliseconds to monitor
		mtx.lock();
		(*output) << "*** DB value set to: " << thread_data->s << ":" << thread_data->m << " by writer number: " << id << endl;
		// cout << "*** DB value set to: " << thread_data->s << ":" << thread_data->m << " by writer number: " << id << endl;		
		mtx.unlock();
		access_data--;

		//delay
		#ifndef WIN32
			usleep(delay * 1000);
		#else
			Sleep(delay);
		#endif

	} while (access_data > 0);
	delete thread_data;
	return 0;
}

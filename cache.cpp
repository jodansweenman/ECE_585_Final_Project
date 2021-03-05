#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

enum Ops {
	READ = 0,							// L1 cache read
	WRITE = 1,							// L1 cache write
	FETCH = 2,							// L1 instruction fetch
	INVAL = 3,							// Invalidate command from L2 cache
	SNOOP = 4,							// Data request to L2 cache
	RESET = 8,							// Reset cache and clear stats
	PRINT = 9							// Print the contents of the cache
};

class Cache {
	unsigned int tag;					// Tag bits
	unsigned int LRU;					// LRU bits
	char MESI;							// MESI bits
	unsigned char data[64];				// 64 bytes of data
	unsigned int address;				// Address
};

class Cache_stats {
	// Data cache
	unsigned int data_cache_hit;		// Data cache hit count
	unsigned int data_cache_miss;		// Data cache miss count
	unsigned int data_cache_read;		// Data cache read count
	unsigned int data_cache_write;		// Data cache write count
	float data_ratio;					// Data hit/miss ratio
	
	// Instruction cache
	unsigned int inst_cache_hit;		// Instruction cache hit count
	unsigned int inst_cache_miss;		// Instruction cache miss count
	unsigned int inst_cache_read;		// Instruction cache read count
	float inst_ratio;					// Instruction hit/miss ratio
};

// Function declarations
int cache_read(unsigned int addr);
int cache_write(unsigned int addr);
int instruction_fetch(unsigned int addr);
int invalidate_command(unsigned int addr);
int snooping(unsigned int addr);
void reset_cache();
void print_cache();
int file_parser(char *filename);

// Instantiation of data and instruction caches
Cache data_cache[8];
Cache instruction_cache[4];

// Instantiation of stats tracker
Cache_stats stats;

/**************************************************************************************

*				MAIN CACHE CONTROLLER

**************************************************************************************/

int main(int argc, char** argv) {
	
	
	// Check for command line test file
	if (argc != 2) {
		cout << "\n\t ERROR: No input file provided.\n";
		exit(1);
	}
	
	// Initialize the cache at the beginning
	//reset_cache();
	
	char *test_file = argv[1];
	
	if (file_parser(test_file)) {
		cout << "\n\t ERROR: Parsing File Failed";
	}
	
	
	cout <<"\n\n\t Testing Completed: Closing Program... \n\n\n";
	
	return 0;
}

/**************************************************************************************

*				CACHE SUBFUNCTIONS

**************************************************************************************/
/* Test file text parser
 * Parses text data in the format of <n FFFFFFFF>
 * Where n is the operation number and FFFFFFFF is the address.
 * Calls appropriate operation based on parsing result
 *
 * Input: string of .txt test file
 * Output: pass = 0, fail != 0
 */
 
 int file_parser(char *filename) {
 	
 	unsigned int operation;				// Operation parsed from input
 	unsigned int address;				// Address parsed from input
 	FILE *fp;							// .txt test file pointer
 	
 	if(!(fp = fopen(filename, "r"))) {
 		cout << "\n\t ERROR: File Cannot Open" << endl;
	 }
 	
 	while (fscanf(fp, "%d %x", &operation, &address) != EOF) {
 		switch(operation) {
 			case READ :
 				if(cache_read(address)) {
 					cout << "\n\t ERROR: L1 Data Cache Read" << endl;
				 }
				break;	
				
			case WRITE :
				if(cache_read(address)) {
					cout << "\n\t ERROR: L1 Data Cache Write" << endl;
				}
				break;
			
			case FETCH :
				if(instruction_fetch(address)) {
					cout << "\n\t ERROR: L1 Instruction Cache Fetch" << endl;
				}
				break;
				
			case INVAL :
				if(invalidate_command(address)) {
					cout << "\n\t ERROR: L2 Cache Invalidate Command" << endl;
				}
				break;
			
			case SNOOP :
				if(snooping(addr)) {
					cout << "\n\t ERROR: L2 Snoop Data Request" << endl;
				}
				break;
			
			case RESET :
				reset_cache();
				break;
			
			case PRINT :
				print_cache();
				break;
				
			default :
				cout << "\n\t ERROR: Invalid Command" << endl;
				return -1;
		 }
 		
	 }
	 
	 fclose(fp);
	 
	 return 0;
 	
 }
 
 /* Cache Reset function
 * This function resets the cache controller and clears statistics
 *
 * Input: Void
 * Output: Void
 */
 
 void reset_cache() {
 	
	 int i;
	 
	 cout << "Resetting the Cache Controller and Clearing Stats..." << endl;
	 
	 // Clearing the data cache
	 for (i = 0; i < 8; i ++) {
	 	data_cache[i].tag = 0;
	 	data_cache[i].LRU = 0;
	 	data_cache[i].MESI = "I";
	 }
	 
	 // Clearing the instruction cache
	 for (i = 0; i < 4; i ++) {
	 	instruction_cache[i].tag = 0;
	 	instruction_cache[i].LRU = 0;
	 	instruction_cache[i].MESI = "I";
	 }
	 
	 // Resetting all stats for data cache
	 stats.data_cache_hit = 0;
	 stats.data_cache_miss = 0;
	 stats.data_cache_read = 0;
	 stats.data_cache_write = 0;
	 stats.data_ratio = 0.0;
	 
	 // Resetting all stats for instruction cache
	 stats.inst_cache_hit = 0;
	 stats.inst_cache_miss = 0;
	 stats.inst_cache_read = 0;
	 stats.inst_ratio = 0.0;
	 
	 return;
 	
 }

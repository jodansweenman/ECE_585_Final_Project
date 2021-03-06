#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

enum Ops {
	READ = 0,									// L1 cache read
	WRITE = 1,									// L1 cache write
	FETCH = 2,									// L1 instruction fetch
	INVAL = 3,									// Invalidate command from L2 cache
	SNOOP = 4,									// Data request to L2 cache
	RESET = 8,									// Reset cache and clear stats
	PRINT = 9									// Print the contents of the cache
};

class Cache {
	public:
		unsigned int tag;						// Tag bits
		unsigned int LRU;						// LRU bits
		char MESI;								// MESI bits
		unsigned char data[64];					// 64 bytes of data
		unsigned int address;					// Address
};

class Cache_stats {
	public:
		// Data cache
		unsigned int data_cache_hit;			// Data cache hit count
		unsigned int data_cache_miss;			// Data cache miss count
		unsigned int data_cache_read;			// Data cache read count
		unsigned int data_cache_write;			// Data cache write count
		float data_ratio;						// Data hit/miss ratio
		
		// Instruction cache
		unsigned int inst_cache_hit;			// Instruction cache hit count
		unsigned int inst_cache_miss;			// Instruction cache miss count
		unsigned int inst_cache_read;			// Instruction cache read count
		float inst_ratio;						// Instruction hit/miss ratio
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
int data_tag_match(unsigned int tag);
int instruction_tag_match(unsigned int tag);
int data_LRU_search();
int instruction_LRU_search();
void data_LRU_update(unsigned int cache_way);
void instruction_LRU_update(unsigned int cache_way);

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
 	
 	unsigned int operation;						// Operation parsed from input
 	unsigned int address;						// Address parsed from input
 	FILE *fp;									// .txt test file pointer
 	
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
				if(snooping(address)) {
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
 
  /* L1 Cache Read function
 * This function will attempt to read a line from the cache
 * On a cache miss, the LRU member is evicted if the cache is full
 *
 * Input: Address to read from the cache
 * Output: pass = 0, fail != 0
 */
 
 int cache_read(unsigned int addr) {
 	
 	unsigned int tag = addr >> (6 + 14);		// tag = address >> (byte + set), where our byte amount is 6 bits and the set is 14
 	int cache_way = -1;							// Cache way in the cache set
 	
 	// Check for an empty set in the cache line
 	for (int i = 0; cache_way < 0 && i < 8; ++i) {
 		// Check for an empty set
 		if (data_cache[i].tag == 0) {
 			cache_way = i;
		}
	}
	
	// Check for empty space and place if empty
	if (cache_way >=0) {
		data_cache[cache_way].tag = tag;
		data_cache[cache_way].MESI = 'E';
		// LRU Data Update
		data_LRU_update(cache_way);
		data_cache[cache_way].LRU = 0;
		data_cache[cache_way].address = addr;
		stats.data_cache_miss++;
	}
	// If no gap search for hit/miss
	else {
		cache_way = data_tag_match(tag);				// Search for a missing tag
		
		if (cache_way < 0)	{						// Miss
			stats.data_cache_miss++;
			// Check for a line with an invalid state to evict
			// Checking for invalid MESI data
			for (int j = 0; j < 8; ++j) {
				if(data_cache[j].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				cache_way = data_LRU_search();
				if (cache_way >=0) {
					data_cache[cache_way].tag = tag;
					data_cache[cache_way].MESI = 'E';
					data_LRU_update(cache_way);
					data_cache[cache_way].address = addr;
				}
				else {
					cout << "LRU data is invalid" << endl;
					return -1;
				}
			}
			else { 								// Else, the invalid member is evicted
				data_cache[cache_way].tag = tag;
				data_cache[cache_way].MESI = 'E';
				data_LRU_update(cache_way);
				data_cache[cache_way].address = addr;
			}
		}
		else {									// Data Hit
			stats.data_cache_hit++;
			switch (data_cache[cache_way].MESI) {
				case 'M' :
					data_cache[cache_way].tag = tag;
					data_cache[cache_way].MESI = 'M';
					data_LRU_update(cache_way);
					data_cache[cache_way].address = addr;
					break;
				
				case 'E' :
					data_cache[cache_way].tag = tag;
					data_cache[cache_way].MESI = 'E';
					data_LRU_update(cache_way);
					data_cache[cache_way].address = addr;
					break;
				
				case 'S' :
					data_cache[cache_way].tag = tag;
					data_cache[cache_way].MESI = 'S';
					data_LRU_update(cache_way);
					data_cache[cache_way].address = addr;
					break;
					
				case 'I' :
					data_cache[cache_way].tag = tag;
					data_cache[cache_way].MESI = 'S';
					data_LRU_update(cache_way);
					data_cache[cache_way].address = addr;
					break;
			}
		}
	}
	return 0;
 }
 
int cache_write(unsigned int addr) {
	
}

int instruction_fetch(unsigned int addr) {
	
}

  /* L2 Invalidate Command function
 * This function will simulate an L2 Invalidate command
 * This will have the MESI bit of the input address set to 'I'
 *
 * Input: Address to invalidate
 * Output: pass = 0, fail != 0
 */
int invalidate_command(unsigned int addr) {
	
	unsigned int tag = addr >> (6 + 14); 		// tag = address >> (byte + set), where our byte amount is 6 bits and the set is 14
	
	// Search data cache for a matching tag
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i].tag == tag) {
			switch (data_cache[i].MESI) {
				case 'M':
					data_cache[i].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'E':
					data_cache[i].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'S':
					data_cache[i].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'I':
					return 0;					// Do nothing as state is already invalid
					
				default:
					return -1;					// Non-MESI state recorded. ERROR
			}
		}
	}
	return 0;	
}

  /* L2 Snooping function
 * This function will simulate an L2 Snoop
 * This function assumes that L2 is telling L1 that the address needs to be invalidated
 * Therefore the MESI bit will be set to 'I'
 *
 * Input: Address to "snoop"
 * Output: pass = 0, fail != 0
 */
int snooping(unsigned int addr) {
	
	unsigned int tag = addr >> (6 + 14); 		// tag = address >> (byte + set), where our byte amount is 6 bits and the set is 14
	
	for (int i = 0; i < 8; ++i) {
		if(data_cache[i].tag == tag) {
			switch (data_cache[i].MESI) {
				case 'M' :
					data_cache[i].MESI = 'I';	// Changes the MESI bit to Invalid
					break;
				
				case 'E' :
					data_cache[i].MESI = 'I';	// Changes the MESI bit to Invalid
					break;
				
				case 'S' :
					data_cache[i].MESI = 'I';	// Changes the MESI bit to Invalid
					break;
				
				case 'I' :
					return 0;					// Do nothing as state is already invalid
				
				default :
					return -1;					// Non-MESI state recorded. ERROR
			}
		}
	}
	
}
 
 /* Cache Reset function
 * This function resets the cache controller and clears statistics
 *
 * Input: Void
 * Output: Void
 */
 
 void reset_cache() {
 	
	 cout << "Resetting the Cache Controller and Clearing Stats..." << endl;
	 
	 // Clearing the data cache
	 for (int i = 0; i < 8; ++i) {
	 	data_cache[i].tag = 0;
	 	data_cache[i].LRU = 0;
	 	data_cache[i].MESI = 'I';
	 }
	 
	 // Clearing the instruction cache
	 for (int j = 0; j < 4; ++j) {
	 	instruction_cache[j].tag = 0;
	 	instruction_cache[j].LRU = 0;
	 	instruction_cache[j].MESI = 'I';
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
void print_cache() {
	
}

void data_LRU_update(unsigned int cache_way){
	int LRU_current = data_cache[cache_way].LRU;
	
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i].LRU <= LRU_current) {
			data_cache[i].LRU++;
		}
	}
	data_cache[cache_way].LRU = 0;
}

void instruction_LRU_update(unsigned int cache_way){
	
}

int data_tag_match(unsigned int tag) {
	
	int i =0;
	// Check for matching tags;
	while (data_cache[i].tag != tag) {
		i++;
		if (i > 7) {
			return -1;
		}
	}
	return i;
	
}
int instruction_tag_match(unsigned int tag) {
	
}
int data_LRU_search() {
	for (int i =0; i < 8; ++i) {
		if (data_cache[i].LRU = 0x7) {
			return i;
		}
	}
	return -1;
}
int instruction_LRU_search() {
	
}

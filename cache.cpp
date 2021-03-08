#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#define BYTE 6
#define SET 14
#define TAG 12
#define BYTEMASK 0x0000003F
#define SETMASK 0x000FFFFF

using namespace std;


enum Ops {
	READ = 0,											// L1 cache read
	WRITE = 1,											// L1 cache write
	FETCH = 2,											// L1 instruction fetch
	INVAL = 3,											// Invalidate command from L2 cache
	SNOOP = 4,											// Data request to L2 cache
	RESET = 8,											// Reset cache and clear stats
	PRINT = 9											// Print the contents of the cache
};

class Cache {
	public:
		unsigned int tag;								// Tag bits
		unsigned int set;								// Set bits
		unsigned int LRU;								// LRU bits
		char MESI = 'I';								// MESI bits
		unsigned int address;							// Address
};

class Cache_stats {
	public:
		// Data cache
		unsigned int data_cache_hit;					// Data cache hit count
		unsigned int data_cache_miss;					// Data cache miss count
		unsigned int data_cache_read;					// Data cache read count
		unsigned int data_cache_write;					// Data cache write count
		float data_ratio;								// Data hit/miss ratio
		
		// Instruction cache
		unsigned int inst_cache_hit;					// Instruction cache hit count
		unsigned int inst_cache_miss;					// Instruction cache miss count
		unsigned int inst_cache_read;					// Instruction cache read count
		float inst_ratio;								// Instruction hit/miss ratio
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
int data_tag_match(unsigned int tag, unsigned int set);
int instruction_tag_match(unsigned int tag, unsigned int set);
int data_LRU_search(int set);
int instruction_LRU_search(int set);
void data_LRU_update(unsigned int cache_way, unsigned int cache_set);
void instruction_LRU_update(unsigned int cache_way, unsigned int cache_set);

// Instantiation of data and instruction caches
Cache data_cache[8][16384];
Cache instruction_cache[4][16384];

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
	reset_cache();
	
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
 	
 	unsigned int operation;								// Operation parsed from input
 	unsigned int address;								// Address parsed from input
 	FILE *fp;											// .txt test file pointer
 	
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
 
/* L1 Data Cache Read function
 * This function will attempt to read a line from the data cache
 * On a cache miss, the LRU member is evicted if the data cache is full
 *
 * Input: Address to read from the cache
 * Output: pass = 0, fail != 0
 */
 int cache_read(unsigned int addr) {
 	
 	unsigned int tag = addr >> (BYTE + SET);			// tag = address >> (byte + set)
 	unsigned int set = (addr & SETMASK) >> BYTE;		// set = (address & setmask) >> byte
 	int cache_way = -1;									// Cache way in the cache set
 	
 	// Check for an empty set in the cache line
 	for (int i = 0; cache_way < 0 && i < 8; ++i) {
 		if (data_cache[i][set].tag == 0) {
 				cache_way = i;
		}
	}
	
	// Check for empty space and place if empty
	if (cache_way >= 0) {
		data_cache[cache_way][set].tag = tag;
		data_cache[cache_way][set].set = set;
		data_cache[cache_way][set].MESI = 'E';
		data_cache[cache_way][set].address = addr;
		stats.data_cache_miss++;
		// LRU Data Update
		data_LRU_update(cache_way,set);
	}
	// If no gap search for hit/miss
	else {
		cache_way = data_tag_match(tag, set);			// Search for a missing tag
		
		if (cache_way < 0)	{			// Miss
			stats.data_cache_miss++;
			// Check for a line with an invalid state to evict
			// Checking for invalid MESI data
			for (int j = 0; j < 8; ++j) {
				if(data_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				cache_way = data_LRU_search(set);
				if (cache_way >=0) {
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'E';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
				}
				else {
					cout << "LRU data is invalid" << endl;
					return -1;
				}
			}
			else { 										// Else, the invalid member is evicted
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'E';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set);
			}
		}
		else {											// Data Hit
			stats.data_cache_hit++;
			switch (data_cache[cache_way][set].MESI) {
				case 'M' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'M';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
				
				case 'E' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'S';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
				
				case 'S' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'S';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
					
				case 'I' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'S';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
			}
		}
	}
	return 0;
 }
 
/* L1 Data Cache Write function
 * This function will attempt to write an address from the data cache
 * On a cache miss, the LRU member is evicted if the data cache is a miss
 *
 * Input: Address to read from the cache
 * Output: pass = 0, fail != 0
 */
int cache_write(unsigned int addr) {
	
	unsigned int tag = addr >> (BYTE + SET);			// tag = address >> (byte + set)
 	unsigned int set = (addr & SETMASK) >> BYTE;		// set = (address & setmask) >> byte
 	int cache_way = -1;									// Cache way in the cache set
 	
 	stats.data_cache_write++;
 	
 	// Checkf  for an empty set in the cache line
	for (int i = 0; cache_way < 0 && i < 8; ++i) {
 		if (data_cache[i][set].tag == 0) {
 				cache_way = i;
		}
	}
	if (cache_way >= 0) {
		data_cache[cache_way][set].tag = tag;
		data_cache[cache_way][set].set = set;
		data_cache[cache_way][set].MESI = 'M';
		data_cache[cache_way][set].address = addr;
		stats.data_cache_miss++;
		// LRU Data Update
		data_LRU_update(cache_way,set);
	}
	else {
		cache_way = data_tag_match(tag, set);			// Search for a missing tag
		
		if (cache_way < 0)	{			// Miss
			stats.data_cache_miss++;
			// Check for a line with an invalid state to evict
			// Checking for invalid MESI data
			for (int j = 0; j < 8; ++j) {
				if(data_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				cache_way = data_LRU_search(set);
				if (cache_way >=0 && set >=0) {
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'M';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
				}
				else {
					cout << "LRU data is invalid" << endl;
					return -1;
				}
			}
			else { 										// Else, the invalid member is evicted
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'M';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set);
			}
		}
		else {											// Data Hit
			stats.data_cache_hit++;
			switch (data_cache[cache_way][set].MESI) {
				case 'M' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'M';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
				
				case 'E' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'M';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
				
				case 'S' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'E';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
					
				case 'I' :
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'E';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set);
					break;
			}
		}
	}
}

/* L1 Instruction Cache Read function
 * This function will attempt to read a line from the instruction cache
 * On a cache miss, the LRU member is evicted if the instruction cache is full
 *
 * Input: Address to read from the cache
 * Output: pass = 0, fail != 0
 */
int instruction_fetch(unsigned int addr) {
	
	unsigned int tag = addr >> (BYTE + SET);			// tag = address >> (byte + set)
 	unsigned int set = (addr & SETMASK) >> BYTE;		// set = (address & setmask) >> byte
 	int cache_way = -1;									// Cache way in the cache set
 	
 	stats.inst_cache_read++;
 	
 	// Check  for an empty set in the cache line
	for (int i = 0; cache_way < 0 && i < 4; ++i) {
 		if (instruction_cache[i][set].tag == 0) {
			cache_way = i;
		}
	}
 	
 	// Check for empty space and place if empty
	if (cache_way >= 0) {
		instruction_cache[cache_way][set].tag = tag;
		instruction_cache[cache_way][set].set = set;
		instruction_cache[cache_way][set].MESI = 'E';
		instruction_cache[cache_way][set].address = addr;
		stats.inst_cache_miss++;
		// LRU instruction Update
		instruction_LRU_update(cache_way,set);
	}
	// If no gap search for hit/miss
	else {
		cache_way = instruction_tag_match(tag, set);	// Search for a missing tag
		
		if (cache_way < 0 || set < 0)	{				// Miss
			stats.inst_cache_miss++;
			// Check for a line with an invalid state to evict
			// Checking for invalid MESI data
			for (int j = 0; j < 4; ++j) {
				if(instruction_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0 || set < 0) {
				cache_way = instruction_LRU_search(set);
				if (cache_way >=0) {
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'E';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set);
				}
				else {
					cout << "LRU data is invalid" << endl;
					return -1;
				}
			}
			else { 										// Else, the invalid member is evicted
				instruction_cache[cache_way][set].tag = tag;
				instruction_cache[cache_way][set].set = set;
				instruction_cache[cache_way][set].MESI = 'E';
				instruction_cache[cache_way][set].address = addr;
				instruction_LRU_update(cache_way,set);
			}
		}
		else {											// instruction Hit
			stats.inst_cache_hit++;
			switch (instruction_cache[cache_way][set].MESI) {
				case 'M' :
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'M';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set);
					break;
				
				case 'E' :
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'S';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set);
					break;
				
				case 'S' :
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'S';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set);
					break;
					
				case 'I' :
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'S';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set);
					break;
			}
		}
	}
	return 0;
 	
	
}

  /* L2 Invalidate Command function
 * This function will simulate an L2 Invalidate command
 * This will have the MESI bit of the input address set to 'I'
 *
 * Input: Address to invalidate
 * Output: pass = 0, fail != 0
 */
int invalidate_command(unsigned int addr) {
	
	unsigned int tag = addr >> (BYTE + SET);			// tag = address >> (byte + set)
	unsigned int set = (addr & SETMASK) >> BYTE;		// set = (address & setmask) >> byte
	
	
	// Search data cache for a matching tag
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i][set].tag == tag) {
			switch (data_cache[i][set].MESI) {
				case 'M':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'E':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'S':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'I':
					return 0;						// Do nothing as state is already invalid
					
				default:
					return -1;						// Non-MESI state recorded. ERROR
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
	
	unsigned int tag = addr >> (BYTE + SET);			// tag = address >> (byte + set)
	unsigned int set = (addr & SETMASK) >> BYTE;		// set = (address & setmask) >> byte
	
	// Search data cache for a matching tag
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i][set].tag == tag) {
			switch (data_cache[i][set].MESI) {
				case 'M':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'E':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'S':
					data_cache[i][set].MESI = 'I';	// Change MESI bit to Invalid
					break;
					
				case 'I':
					return 0;						// Do nothing as state is already invalid
					
				default:
					return -1;						// Non-MESI state recorded. ERROR
			}
		}	
	}
	return 0;
	
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
	 	for (int j = 0; j < 16384; ++j) {
			data_cache[i][j].tag = 0;
		 	data_cache[i][j].set = 0;
			data_cache[i][j].LRU = 0;
		 	data_cache[i][j].MESI = 'I';
		 }
	 }
	 
	 // Clearing the instruction cache
	 for (int k = 0; k < 4; ++k) {
	 	for (int l = 0; l < 16384; ++l) {
	 		instruction_cache[k][l].tag = 0;
	 		instruction_cache[k][l].set = 0;
	 		instruction_cache[k][l].LRU = 0;
	 		instruction_cache[k][l].MESI = 'I';
		 }
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
 
/* Print Cache function
 * This function print information about the caches and their statistics
 *
 * Input: Void
 * Output: Void
 */
void print_cache() {
	
}

/* Data Cache LRU Update function
 * This function updates the data cache LRU
 *
 * Input: cache_way and cache_set values
 * Output: Void
 */
void data_LRU_update(unsigned int cache_way, unsigned int cache_set) {
	int LRU_current = data_cache[cache_way][cache_set].LRU;
	
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i][cache_set].LRU >= LRU_current) {
			data_cache[i][cache_set].LRU--;
		}	
	}
	data_cache[cache_way][cache_set].LRU = 0x7;
}

/* Instruction Cache LRU Update function
 * This function updates the data cache LRU
 *
 * Input: cache_way and cache_set values
 * Output: Void
 */
void instruction_LRU_update(unsigned int cache_way, unsigned int cache_set) {
	int LRU_current = instruction_cache[cache_way][cache_set].LRU;
	
	for (int i = 0; i < 4; ++i) {
		if (instruction_cache[i][cache_set].LRU >= LRU_current) {
			instruction_cache[i][cache_set].LRU--;
		}
	}
	instruction_cache[cache_way][cache_set].LRU = 0x3;
	
}

/* Data Tag matching function
 * This function compares two data cache tags for equality
 *
 * Input: Tag, Set
 * Output: Void
 */
int data_tag_match(unsigned int tag, unsigned int set) {
	
	int i = 0;
	// Check for matching tags;
	while (data_cache[i][set].tag != tag) {
		i++;
		if (i > 7) {
			return -1;
		}
	}
	return i;
}

/* Instruction Tag matching function
 * This function compares two instruction cache tags for equality
 *
 * Input: Tag, set
 * Output: Void
 */
int instruction_tag_match(unsigned int tag, unsigned int set) {
	
	int i = 0;
	// Check for matching tags;
	while (data_cache[i][set].tag != tag) {
		i++;
		if (i > 3) {
			return -1;
		}
	}
	return i;
	
}

/* Data LRU Evict Search function
 * This function finds the member of the cache to resets the cache controller and clears statistics
 *
 * Input: Void
 * Output: Void
 */
int data_LRU_search(int set) {
	for (int i = 0; i < 8; ++i) {
		if (data_cache[i][set].LRU == 0) {
			return i;
		}
	}
	return -1;
}

/* Cache Reset function
 * This function resets the cache controller and clears statistics
 *
 * Input: Void
 * Output: Void
 */
int instruction_LRU_search(int set) {
	for (int i = 0; i < 4; ++i) {
		if (data_cache[i][set].LRU == 0) {
			return i;
		}
	}
	return -1;
}

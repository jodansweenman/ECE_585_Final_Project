/**************************************************************************************
* cache.cpp
*
* Authors: Josiah Sweeney, Melinda Van, Nguyen
*
*
*
*
*
*

***************************************************************************************/




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

// Global Mode Instantiation
unsigned int mode = 3;									// Start at invalid state

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
void data_LRU_update(unsigned int cache_way, unsigned int cache_set, unsigned int flag);
void instruction_LRU_update(unsigned int cache_way, unsigned int cache_set, unsigned int flag);

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
	
	// Reading in file name from command line
	char *test_file = argv[1];
	
	// Select a mode
	cout << "\n\t Select Mode" << endl;
	cout << "Mode 0: Summary of usage statistics and print commands only" << endl;
	cout << "Mode 1: Information from Mode 0 with messages to L2 in addition" << endl;
	cout << "Mode 2: Information from previous modes with information for every cache hit" << endl;
	
	do {
		cout << "\nEnter Mode: ";
		scanf("%u", &mode);
	}
	while(mode > 2);
	
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
				if(cache_write(address)) {
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
				break;
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
 	unsigned int empty_cache = 0;						// Flag for empty cache
 	int cache_way = -1;									// Cache way in the cache set
 	
 	stats.data_cache_read++;
 	
 	// Search for a matching tag
 	cache_way = data_tag_match(tag, set);	
	 
	if(cache_way >= 0) {							// Data Cache Hit
		stats.data_cache_hit++;
		// Debug Mode Message
		if(mode == 2) {
			cout << "Data Cache Hit" << endl;
		}
		switch (data_cache[cache_way][set].MESI) {
			case 'M' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'M';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'E' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'S';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'S' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'S';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
				
			case 'I' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'S';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
		}
	}
	else {												// Data Cache Miss
		stats.data_cache_miss++;
		
		// Simulate L2 Cache Read
		if (mode > 0) {
			cout << "Data Cache Miss: Read from L2 " << hex << addr << " [Data]" << endl;
		}
		
		// Check for an empty set in the cache line
 		for (int i = 0; cache_way < 0 && i < 8; ++i) {
 			if (data_cache[i][set].tag == 0) {
 				cache_way = i;
 				empty_cache = 1;
			}
		}
		
		// Place if empty
		if (cache_way >= 0) {
			data_cache[cache_way][set].tag = tag;
			data_cache[cache_way][set].set = set;
			data_cache[cache_way][set].MESI = 'E';
			data_cache[cache_way][set].address = addr;
			// LRU Data Update
			data_LRU_update(cache_way,set,empty_cache);
		}
		else {
			// Check for a line with invalid MESI data to evict
			for (int j = 0; j < 8; ++j) {
				if(data_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				// Search for smallest LRU
				cache_way = data_LRU_search(set);
				if (cache_way >=0) {
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'E';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set,empty_cache);
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
				data_LRU_update(cache_way,set,empty_cache);
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
 	unsigned int empty_cache = 0;						// Flag for empty cache
 	int cache_way = -1;									// Cache way in the cache set
 	
 	stats.data_cache_write++;
 	
 	// Search for a matching tag
 	cache_way = data_tag_match(tag, set);	
	 
	if(cache_way >= 0) {								// Data Cache Hit
		
		stats.data_cache_hit++;
		
		// Debug Message
		if (mode == 2) {
			cout << "Data Cache Hit" << endl;
		}
		
		switch (data_cache[cache_way][set].MESI) {
			case 'M' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'M';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'E' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'M';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'S' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'E';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
				
			case 'I' :
				data_cache[cache_way][set].tag = tag;
				data_cache[cache_way][set].set = set;
				data_cache[cache_way][set].MESI = 'E';
				data_cache[cache_way][set].address = addr;
				data_LRU_update(cache_way,set,empty_cache);
				break;
		}
	}
	else {												// Data Cache Miss
		stats.data_cache_miss++;
		
		// Simulate L2 cache write-through message
		if (mode > 0) {
			cout << "Data Cache Miss: Write to L2 " << hex << addr << " [Write-Through]" << endl;
		}
		
		// Check for an empty set in the cache line
 		for (int i = 0; cache_way < 0 && i < 8; ++i) {
 			if (data_cache[i][set].tag == 0) {
 				cache_way = i;
 				empty_cache = 1;
			}
		}
		
		// Place if empty
		if (cache_way >= 0) {
			data_cache[cache_way][set].tag = tag;
			data_cache[cache_way][set].set = set;
			data_cache[cache_way][set].MESI = 'M';
			data_cache[cache_way][set].address = addr;
			// LRU Data Update
			data_LRU_update(cache_way,set,empty_cache);
		}
		else {
			// Simulate L2 Cache RFO message
			if (mode > 0) {
				cout << "Read for Ownership from L2 " << hex << addr << endl;
			}
			
			// Check for a line with invalid MESI data to evict
			for (int j = 0; j < 8; ++j) {
				if(data_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				// Search for smallest LRU
				cache_way = data_LRU_search(set);
				if (cache_way >=0) {
					data_cache[cache_way][set].tag = tag;
					data_cache[cache_way][set].set = set;
					data_cache[cache_way][set].MESI = 'M';
					data_cache[cache_way][set].address = addr;
					data_LRU_update(cache_way,set,empty_cache);
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
				data_LRU_update(cache_way,set,empty_cache);
			}
			
		}	
	}
	return 0;
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
 	unsigned int empty_cache = 0;						// Flag for empty cache
 	int cache_way = -1;									// Cache way in the cache set
 	
 	stats.inst_cache_read++;
 	
 	// Search for a matching tag
 	cache_way = instruction_tag_match(tag, set);	// Search for a missing tag
 	
 	if(cache_way >= 0) {								// Instruction Cache Hit
 		stats.inst_cache_hit++;	
 		
 		// Debug Mode Message
 		if (mode == 2) {
 			cout << "Instruction Cache Hit" << endl;
		}
 		switch (instruction_cache[cache_way][set].MESI) {
			case 'M' :
				instruction_cache[cache_way][set].tag = tag;
				instruction_cache[cache_way][set].set = set;
				instruction_cache[cache_way][set].MESI = 'M';
				instruction_cache[cache_way][set].address = addr;
				instruction_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'E' :
				instruction_cache[cache_way][set].tag = tag;
				instruction_cache[cache_way][set].set = set;
				instruction_cache[cache_way][set].MESI = 'S';
				instruction_cache[cache_way][set].address = addr;
				instruction_LRU_update(cache_way,set,empty_cache);
				break;
			
			case 'S' :
				instruction_cache[cache_way][set].tag = tag;
				instruction_cache[cache_way][set].set = set;
				instruction_cache[cache_way][set].MESI = 'S';
				instruction_cache[cache_way][set].address = addr;
				instruction_LRU_update(cache_way,set,empty_cache);
				break;
				
			case 'I' :
				instruction_cache[cache_way][set].tag = tag;
				instruction_cache[cache_way][set].set = set;
				instruction_cache[cache_way][set].MESI = 'S';
				instruction_cache[cache_way][set].address = addr;
				instruction_LRU_update(cache_way,set,empty_cache);
				break;
		}
	}
	else {												// Instruction Cache Miss
		stats.inst_cache_miss++;
		
		// Simulate L2 Instruction Cache Read
		if(mode > 0) {
			cout << "Instruction Cache Miss: Read frin L2 " << hex << addr << " [Instruction]" << endl;
		}
		// Check  for an empty set in the cache line
		for (int i = 0; cache_way < 0 && i < 4; ++i) {
	 		if (instruction_cache[i][set].tag == 0) {
				cache_way = i;
				empty_cache = 1;
			}
		}
		if (cache_way >= 0) {
			instruction_cache[cache_way][set].tag = tag;
			instruction_cache[cache_way][set].set = set;
			instruction_cache[cache_way][set].MESI = 'E';
			instruction_cache[cache_way][set].address = addr;
			// LRU instruction Update
			instruction_LRU_update(cache_way,set,empty_cache);
		}
		else {
			// Check for a line with invalid MESI data to evict
			for (int j = 0; j < 4; ++j) {
				if(instruction_cache[j][set].MESI == 'I') {
					cache_way = j;
				}
				else {
					cache_way = -1;
				}
			}
			if (cache_way < 0) {
				cache_way = instruction_LRU_search(set);
				if (cache_way >=0) {
					instruction_cache[cache_way][set].tag = tag;
					instruction_cache[cache_way][set].set = set;
					instruction_cache[cache_way][set].MESI = 'E';
					instruction_cache[cache_way][set].address = addr;
					instruction_LRU_update(cache_way,set,empty_cache);
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
				instruction_LRU_update(cache_way,set,empty_cache);
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
					data_cache[i][set].MESI = 'I';		// Change MESI bit to Invalid
					break;
					
				case 'E':
					data_cache[i][set].MESI = 'I';		// Change MESI bit to Invalid
					break;
					
				case 'S':
					data_cache[i][set].MESI = 'I';		// Change MESI bit to Invalid
					break;
					
				case 'I':
					return 0;							// Do nothing as state is already invalid
					
				default:
					return -1;							// Non-MESI state recorded. ERROR
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
		 	data_cache[i][j].address = 0;
		 }
	 }
	 
	 
	 // Clearing the instruction cache
	 for (int k = 0; k < 4; ++k) {
	 	for (int l = 0; l < 16384; ++l) {
	 		instruction_cache[k][l].tag = 0;
	 		instruction_cache[k][l].set = 0;
	 		instruction_cache[k][l].LRU = 0;
	 		instruction_cache[k][l].MESI = 'I';
	 		instruction_cache[k][l].address = 0;
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
	
	// Setting flag for printing
	int set_flag = 0;
	
	// Update values for cache hits/misses ratio
	stats.data_ratio = float(stats.data_cache_hit)/float(stats.data_cache_miss);
	stats.inst_ratio = float(stats.inst_cache_hit)/float(stats.inst_cache_miss);
	
	// Printing information for Data Cache
	cout << "\n\n\t\t\tDATA CACHE" << endl;	
	for (int i = 0; i < 16384; ++i) {
		for (int j = 0; j < 8; ++j) {
			if(data_cache[j][i].address > 0) {
				if(set_flag == 0){
					cout << "Set No: " << hex << i << endl;
					set_flag = 1;
				}
				cout << "Way No: " << dec << j << endl;
				cout << "Address: " << hex << data_cache[j][i].address << " Tag: " << data_cache[j][i].tag << " Set: " 
					<< data_cache[j][i].set << " LRU: " << data_cache[j][i].LRU << " MESI State: " << data_cache[j][i].MESI << endl;
			}
		}
		set_flag = 0;
	}
	
	// Printing information for Instruction Cache
	cout << "\n\n\t\t\tINSTRUCTION CACHE" << endl;	
	for (int k = 0; k < 16384; ++k) {
		for (int l = 0; l < 4; ++l) {
			if(instruction_cache[l][k].address > 0) {
				if(set_flag == 0){
					cout << "Set No: " << hex << k << endl;
					set_flag = 1;
				}
				cout << "Way No: " << dec << l << endl;
				cout << "Address: " << hex << instruction_cache[l][k].address << " Tag: " << instruction_cache[l][k].tag << " Set: " << instruction_cache[l][k].set << " LRU: " << instruction_cache[l][k].LRU << " MESI State: " << instruction_cache[l][k].MESI << endl;
			}
		}
		set_flag = 0;
	}
	
	// Printing Cache Statistics
	cout << "\n\n\t\t\tCACHE STATISTICS" << endl;
	if(stats.data_cache_miss == 0){
		cout << "No Data Cache Transactions Occured" << endl;
	}
	else {
		cout << "\nDATA CACHE:" << endl;
		cout << "Data Cache Hits: " << dec << stats.data_cache_hit << "\nData Cache Misses: " << stats.data_cache_miss << "\nData Cache Hit/Miss Ratio: " 
			<< stats.data_ratio << "\nData Cache Reads: " << stats.data_cache_read << "\nData Cache Writes: " << stats.data_cache_write << endl;
	}
	
	if(stats.inst_cache_miss == 0) {
		cout << "No Instruction Cache Transactions Occured" << endl;
	}
	else {
		cout << "\nINSTRUCTION CACHE:" << endl;
		cout << "Instruction Cache Hits: " << stats.inst_cache_hit << "\nInstruction Cache Misses: " << stats.inst_cache_miss << "\nInstruction Cache Hit/Miss Ratio: " 
			<< stats.inst_ratio << "\nInstruction Cache Reads: " << stats.inst_cache_read << endl;
	}		
}

/* Data Cache LRU Update function
 * This function updates the data cache LRU
 *
 * Input: cache_way and cache_set values, flag for whether there is empty space in the cache
 * Output: Void. Updates LRU values for a given set in the global data cache class
 */
void data_LRU_update(unsigned int cache_way, unsigned int cache_set, unsigned int flag) {
	int LRU_current = data_cache[cache_way][cache_set].LRU;

	if(LRU_current == 0x0){
		if(flag == 1){
			for (int i = 0; i < cache_way; ++i){
				--data_cache[i][cache_set].LRU;
			}
		}
		else {
			for (int i = 0; i < 8; ++i){
				--data_cache[i][cache_set].LRU;
			}
		}
	}
	else {
		for (int i = 0; i < 8; ++i) {
			if(LRU_current > data_cache[i][cache_set].LRU) {
				data_cache[i][cache_set].LRU = data_cache[i][cache_set].LRU;
			}
			else
			{
				--data_cache[i][cache_set].LRU;
			}
		}
	}
	data_cache[cache_way][cache_set].LRU = 0x7;
}

/* Instruction Cache LRU Update function
 * This function updates the data cache LRU
 *
 * Input: cache_way and cache_set values, flag for whether there is empty space in the cache
 * Output: Void. Updates LRU values for a given set in the global instruction cache class
 */
void instruction_LRU_update(unsigned int cache_way, unsigned int cache_set, unsigned int flag) {
	int LRU_current = instruction_cache[cache_way][cache_set].LRU;
	
	if(LRU_current == 0x0){
		if(flag == 1){
			for (int i = 0; i < cache_way; ++i){
				--instruction_cache[i][cache_set].LRU;
			}
		}
		else {
			for (int i = 0; i < 4; ++i){
				--instruction_cache[i][cache_set].LRU;
			}
		}
	}
	
	else {
		for (int i = 0; i < 4; ++i) {
			if(LRU_current >= instruction_cache[i][cache_set].LRU) {
				instruction_cache[i][cache_set].LRU = instruction_cache[i][cache_set].LRU;
			}
			else
			{
				--instruction_cache[i][cache_set].LRU;
			}
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
	while (instruction_cache[i][set].tag != tag) {
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
 * Input: Set
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
 * Input: Set
 * Output: Void
 */
int instruction_LRU_search(int set) {
	for (int i = 0; i < 4; ++i) {
		if (instruction_cache[i][set].LRU == 0) {
			return i;
		}
	}
	return -1;
}


#ifndef TRACE_ITEM_H
#define TRACE_ITEM_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
// #include <arpa/inet.h>
#include <winsock2.h>
#include <stdbool.h>


// this is tpts
enum trace_item_type {
	ti_NOP = 0,
	ti_RTYPE,
	ti_ITYPE,
	ti_LOAD,
	ti_STORE,
	ti_BRANCH,
	ti_JTYPE,
	ti_SPECIAL,
	ti_JRTYPE,
	ti_DONE,
	ti_SQUASHED
};

struct trace_item {
	unsigned char type;			// see above
	unsigned char sReg_a;		// 1st operand
	unsigned char sReg_b;		// 2nd operand
	unsigned char dReg;			// dest. operand
	unsigned int PC;			// program counter
	unsigned int Addr;			// mem. address
};

struct branch_predictor_item {
	unsigned int tag;			// the instruction address
	unsigned char prediction;	// predictor (can be 1 or two bit depending on usage)
};

#endif

#define TRACE_BUFSIZE 1024*1024
#define BRANCH_PREDICT_TABLE_SIZE 64

static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item *trace_buf;

int is_big_endian(void) {
	union {
		uint32_t i;
		char c[4];
	} bint = { 0x01020304 };

	return bint.c[0] == 1;
}

uint32_t my_ntohl(uint32_t x) {
	u_char *s = (u_char *)&x;
	return (uint32_t)(s[3] << 24 | s[2] << 16 | s[1] << 8 | s[0]);
}

void trace_init() {
	trace_buf = malloc(sizeof(struct trace_item) * TRACE_BUFSIZE);

	if (!trace_buf) {
		fprintf(stdout, "** trace_buf not allocated\n");
		exit(-1);
	}

	trace_buf_ptr = 0;
	trace_buf_end = 0;
}

void trace_uninit() {
	free(trace_buf);
	fclose(trace_fd);
}

int trace_get_item(struct trace_item **item) {
	int n_items;

	if (trace_buf_ptr == trace_buf_end) {	/* if no more unprocessed items in the trace buffer, get new data  */
		n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
		if (!n_items) return 0;				/* if no more items in the file, we are done */

		trace_buf_ptr = 0;
		trace_buf_end = n_items;			/* n_items were read and placed in trace buffer */
	}

	*item = &trace_buf[trace_buf_ptr];	/* read a new trace item for processing */
	trace_buf_ptr++;

	if (is_big_endian()) {
		(*item)->PC = my_ntohl((*item)->PC);
		(*item)->Addr = my_ntohl((*item)->Addr);
	}

	return 1;
}

void print_cycle(struct trace_item *item, int cycle_number) {
	switch(item->type) {
		case ti_NOP:
			printf("[cycle %d] NOP:\n",cycle_number) ;
			break;
		case ti_RTYPE:
			printf("[cycle %d] RTYPE:",cycle_number) ;
			printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", item->PC, item->sReg_a, item->sReg_b, item->dReg);
			break;
		case ti_ITYPE:
			printf("[cycle %d] ITYPE:",cycle_number) ;
			printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", item->PC, item->sReg_a, item->dReg, item->Addr);
			break;
		case ti_LOAD:
			printf("[cycle %d] LOAD:",cycle_number) ;			
			printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", item->PC, item->sReg_a, item->dReg, item->Addr);
			break;
		case ti_STORE:
			printf("[cycle %d] STORE:",cycle_number) ;			
			printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", item->PC, item->sReg_a, item->sReg_b, item->Addr);
			break;
		case ti_BRANCH:
			printf("[cycle %d] BRANCH:",cycle_number) ;
			printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", item->PC, item->sReg_a, item->sReg_b, item->Addr);
			break;
		case ti_JTYPE:
			printf("[cycle %d] JTYPE:",cycle_number) ;
			printf(" (PC: %x)(addr: %x)\n", item->PC,item->Addr);
			break;
		case ti_SPECIAL:
			printf("[cycle %d] SPECIAL:",cycle_number) ;				
			break;
		case ti_JRTYPE:
			printf("[cycle %d] JRTYPE:",cycle_number) ;
			printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", item->PC, item->dReg, item->Addr);
			break;
		case ti_DONE:
			printf("[cycle %d] DONE:\n",cycle_number) ;
			break;
		case ti_SQUASHED:
			printf("[cycle %d] SQUASHED:\n",cycle_number) ;
			break;
	}
}

char* get_trace_item_type(struct trace_item *item) {
	switch(item->type) {
		case ti_NOP:
			return "NOP     ";
		case ti_RTYPE:
			return "RTYPE   ";
		case ti_ITYPE:
			return "ITYPE   ";
		case ti_LOAD:
			return "LOAD    ";
		case ti_STORE:
			return "STORE   ";
		case ti_BRANCH:
			return "BRANCH  ";
		case ti_JTYPE:
			return "JTYPE   ";
		case ti_SPECIAL:
			return "SPECIAL ";	
		case ti_JRTYPE:
			return "JRTYPE  ";
		case ti_DONE:
			return "DONE    ";
		case ti_SQUASHED:
			return "SQUASHED";
	}
}

void print_pipeline(struct trace_item *entry, 
					struct trace_item *IF1,
					struct trace_item *IF2,
					struct trace_item *ID,
					struct trace_item *EX1,
					struct trace_item *EX2,
					struct trace_item *MEM1,
					struct trace_item *MEM2,
					struct trace_item *WB,
					int cycle_number) {
	printf("cycle ---------- ---------- ---------- ---------- ---------- ---------- ---------- ---------- ----------\n");
	printf("%5d|   entry  |   IF1    |   IF2    |    ID    |   EX1    |   EX2    |   MEM1   |   MEM2   |    WB    |\n", cycle_number);
	printf("type | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", get_trace_item_type(entry), get_trace_item_type(IF1), get_trace_item_type(IF2), get_trace_item_type(ID), get_trace_item_type(EX1), get_trace_item_type(EX2), get_trace_item_type(MEM1), get_trace_item_type(MEM2), get_trace_item_type(WB));
	printf("aReg |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |\n", entry->sReg_a, IF1->sReg_a, IF2->sReg_a, ID->sReg_a, EX1->sReg_a, EX2->sReg_a, MEM1->sReg_a, MEM2->sReg_a, WB->sReg_a);
	printf("bReg |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |\n", entry->sReg_b, IF1->sReg_b, IF2->sReg_b, ID->sReg_b, EX1->sReg_b, EX2->sReg_b, MEM1->sReg_b, MEM2->sReg_b, WB->sReg_b);
	printf("dReg |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |    %3d   |\n", entry->dReg, IF1->dReg, IF2->dReg, ID->dReg, EX1->dReg, EX2->dReg, MEM1->dReg, MEM2->dReg, WB->dReg);
	printf("PC   | %6x   | %6x   | %6x   | %6x   | %6x   | %6x   | %6x   | %6x   | %6x   |\n", entry->PC, IF1->PC, IF2->PC, ID->PC, EX1->PC, EX2->PC, MEM1->PC, MEM2->PC, WB->PC);
	printf("Addr | %8x | %8x | %8x | %8x | %8x | %8x | %8x | %8x | %8x |\n", entry->Addr, IF1->Addr, IF2->Addr, ID->Addr, EX1->Addr, EX2->Addr, MEM1->Addr, MEM2->Addr, WB->Addr);
	printf("      ---------- ---------- ---------- ---------- ---------- ---------- ---------- ---------- ----------\n");
}

void print_branch_table(struct branch_predictor_item *table){
	int entries = 4;
	
	for (int i = 0; i < BRANCH_PREDICT_TABLE_SIZE; i+=BRANCH_PREDICT_TABLE_SIZE/entries) {
		printf("| idx | tag      | prediction ");
	}
	printf("\n");
	for (int i = 0; i < BRANCH_PREDICT_TABLE_SIZE; i+=BRANCH_PREDICT_TABLE_SIZE/entries) {
		printf("|-----|----------|------------");
	}
	printf("\n");

	for (int i = 0; i < BRANCH_PREDICT_TABLE_SIZE/entries; i++) {
		for (int j = 0; j < BRANCH_PREDICT_TABLE_SIZE; j+=BRANCH_PREDICT_TABLE_SIZE/entries) {
			printf("| %3d | %8x | %10d ", i+j, table[i+j].tag, table[i+j].prediction);
		}
		printf("\n");
	}

	for (int i = 0; i < BRANCH_PREDICT_TABLE_SIZE; i+=BRANCH_PREDICT_TABLE_SIZE/entries) {
		printf(" -----------------------------");
	}
	printf("\n");
}

// implements the hash function, returns hashed value (bits 9-4 of address)
unsigned int get_hash(unsigned int addr) {
	// get bits 9-0 (512 = 2^9)
	addr = addr%512;

	// bits 9-4 returned as an unisnged integer
	addr = addr >> 4;

	return addr;
}
/**************************************************************/
/* CS/COE 1541				 			
	 just compile with gcc -o pipeline pipeline.c			
	 and execute using							
	 ./pipeline	/afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0	
***************************************************************/

#include "CPU.h" 

void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char **argv) {
	signal(SIGSEGV, handler);   // install our handler

	struct trace_item *tr_entry = NULL;
	struct trace_item *tr_WB = NULL;
	struct trace_item *tr_MEM2 = NULL;
	struct trace_item *tr_MEM1 = NULL;
	struct trace_item *tr_EX2 = NULL;
	struct trace_item *tr_EX1 = NULL;
	struct trace_item *tr_ID = NULL;
	struct trace_item *tr_IF2 = NULL;
	struct trace_item *tr_IF1 = NULL;
	size_t size;
	bool stall = false;
	char *trace_file_name;
	int trace_view_on = 0;
	
	unsigned char t_type = 0;
	unsigned char t_sReg_a= 0;
	unsigned char t_sReg_b= 0;
	unsigned char t_dReg= 0;
	unsigned int t_PC = 0;
	unsigned int t_Addr = 0;

	unsigned int cycle_number = 1;

	if (argc == 1) {
		fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
		fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
		exit(0);
	}
		
	trace_file_name = argv[1];
	if (argc == 3) trace_view_on = atoi(argv[2]) ;

	fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

	trace_fd = fopen(trace_file_name, "rb");

	if (!trace_fd) {
		fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
		exit(0);
	}

	trace_init();

	// assuming trace will have valid first instruction
	size = trace_get_item(&tr_entry);

	while(1) {
		// increment cycle count
		cycle_number++;

		// exit, print if trace is on
		if (trace_view_on && *tr_WB != NULL) {
			switch(tr_WB->type) {
				case ti_NOP:
					printf("[cycle %d] NOP:\n",cycle_number) ;
					break;
				case ti_RTYPE:
					printf("[cycle %d] RTYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", tr_WB->PC, tr_WB->sReg_a, tr_WB->sReg_b, tr_WB->dReg);
					break;
				case ti_ITYPE:
					printf("[cycle %d] ITYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_WB->PC, tr_WB->sReg_a, tr_WB->dReg, tr_WB->Addr);
					break;
				case ti_LOAD:
					printf("[cycle %d] LOAD:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_WB->PC, tr_WB->sReg_a, tr_WB->dReg, tr_WB->Addr);
					break;
				case ti_STORE:
					printf("[cycle %d] STORE:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_WB->PC, tr_WB->sReg_a, tr_WB->sReg_b, tr_WB->Addr);
					break;
				case ti_BRANCH:
					printf("[cycle %d] BRANCH:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_WB->PC, tr_WB->sReg_a, tr_WB->sReg_b, tr_WB->Addr);
					break;
				case ti_JTYPE:
					printf("[cycle %d] JTYPE:",cycle_number) ;
					printf(" (PC: %x)(addr: %x)\n", tr_WB->PC,tr_WB->Addr);
					break;
				case ti_SPECIAL:
					printf("[cycle %d] SPECIAL:",cycle_number) ;				
					break;
				case ti_JRTYPE:
					printf("[cycle %d] JRTYPE:",cycle_number) ;
					printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", tr_WB->PC, tr_WB->dReg, tr_WB->Addr);
					break;
			}
		}

		// WB
		tr_WB = tr_MEM2;

		// MEM2
		tr_MEM2 = tr_MEM1;

		// MEM1
		tr_MEM1 = tr_EX2;

		// EX2
		tr_EX2 = tr_EX1;

		// EX1
		/* Data hazard: a
			If an instruction in EX1 will write into a register X while the
			instruction in ID is reading from register X, then the instruction
			in ID (and subsequent instructions) should stall for one cycle to
			allow forwarding of the result from EX2/MEM1 to ID/EX1 (in the
			following cycle). A no-op is injected in EX1/EX2.
		*/
		// something will be in EX1, and the destination registers are the same
		if (cycle_number >= 4 && tr_EX1->dReg == tr_ID->dReg) {
			// check that instruction writes to a register
			switch(tr_EX1->type) {
				case ti_RTYPE:
				case ti_ITYPE:
				case ti_LOAD:
					tr_EX1->type = ti_NOP;
					continue;
			}
		}

		/* Structural hazards: 
			if the instruction at WB is trying to write into the register file
			while the instruction at ID is trying to read form the register
			file, priority is given to the instruction at WB. The instructions
			at IF1, IF2 and ID are stalled for one cycle while the instruction
			at WB is using the register file.
		*/
		if (cycle_number >= 8) {
			switch(tr_WB->type) {
				case ti_RTYPE:
				case ti_ITYPE:
				case ti_LOAD:
					tr_EX1->type = ti_NOP;
					continue;
			}
		}
		tr_EX1 = tr_ID;

		// ID
		tr_ID = tr_IF2;

		// IF2
		tr_IF2 = tr_IF1;

		// IF1
		tr_IF1 = tr_entry;

		// get new entry
		if(!stall) {
			if (size) {
				size = trace_get_item(&tr_entry);
			} else {
				tr_entry->type = ti_DONE;
			}
		}

		if (!size && tr_WB->type == ti_DONE) {	 /* no more instructions (trace_items) to simulate */
			printf("+ Simulation terminates at cycle : %u\n", cycle_number);
			break;
		} else {	/* parse the next instruction to simulate */
			t_type = tr_entry->type;
			t_sReg_a = tr_entry->sReg_a;
			t_sReg_b = tr_entry->sReg_b;
			t_dReg = tr_entry->dReg;
			t_PC = tr_entry->PC;
			t_Addr = tr_entry->Addr;
		}
	}

	trace_uninit();

	exit(0);
}



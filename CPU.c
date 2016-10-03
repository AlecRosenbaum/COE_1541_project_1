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

	struct trace_item *tr_entry;
	struct trace_item *tr_WB;
	struct trace_item *tr_MEM2;
	struct trace_item *tr_MEM1;
	struct trace_item *tr_EX2;
	struct trace_item *tr_EX1;
	struct trace_item *tr_ID;
	struct trace_item *tr_IF2;
	struct trace_item *tr_IF1;
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
		
		// exit, print if trace is on
		if (trace_view_on) {
			switch(tr_WB->type) {
				case ti_NOP:
					printf("[cycle %d] NOP:",cycle_number) ;
					break;
				case ti_RTYPE:
					printf("[cycle %d] RTYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->dReg);
					break;
				case ti_ITYPE:
					printf("[cycle %d] ITYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
					break;
				case ti_LOAD:
					printf("[cycle %d] LOAD:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
					break;
				case ti_STORE:
					printf("[cycle %d] STORE:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
					break;
				case ti_BRANCH:
					printf("[cycle %d] BRANCH:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
					break;
				case ti_JTYPE:
					printf("[cycle %d] JTYPE:",cycle_number) ;
					printf(" (PC: %x)(addr: %x)\n", tr_entry->PC,tr_entry->Addr);
					break;
				case ti_SPECIAL:
					printf("[cycle %d] SPECIAL:",cycle_number) ;				
					break;
				case ti_JRTYPE:
					printf("[cycle %d] JRTYPE:",cycle_number) ;
					printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", tr_entry->PC, tr_entry->dReg, tr_entry->Addr);
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

		// SIMULATION OF A SINGLE CYCLE cpu IS TRIVIAL - EACH INSTRUCTION IS EXECUTED
		// IN ONE CYCLE

		if (trace_view_on) { /* print the executed instruction if trace_view_on=1 */
			switch(tr_entry->type) {
				case ti_NOP:
					printf("[cycle %d] NOP:",cycle_number) ;
					break;
				case ti_RTYPE:
					printf("[cycle %d] RTYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->dReg);
					break;
				case ti_ITYPE:
					printf("[cycle %d] ITYPE:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
					break;
				case ti_LOAD:
					printf("[cycle %d] LOAD:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
					break;
				case ti_STORE:
					printf("[cycle %d] STORE:",cycle_number) ;			
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
					break;
				case ti_BRANCH:
					printf("[cycle %d] BRANCH:",cycle_number) ;
					printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
					break;
				case ti_JTYPE:
					printf("[cycle %d] JTYPE:",cycle_number) ;
					printf(" (PC: %x)(addr: %x)\n", tr_entry->PC,tr_entry->Addr);
					break;
				case ti_SPECIAL:
					printf("[cycle %d] SPECIAL:",cycle_number) ;				
					break;
				case ti_JRTYPE:
					printf("[cycle %d] JRTYPE:",cycle_number) ;
					printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", tr_entry->PC, tr_entry->dReg, tr_entry->Addr);
					break;
			}
		}
		
		// increment cycle count
		cycle_number++;
	}

	trace_uninit();

	exit(0);
}



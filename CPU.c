/**************************************************************/
/* CS/COE 1541				 			
	 just compile with gcc -o pipeline pipeline.c			
	 and execute using							
	 ./pipeline	/afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0	
***************************************************************/

#include "CPU.h" 

int main(int argc, char **argv) {
	// some custom instructions so we don't overwrite existing ones
	struct trace_item *NO_OP;
	NO_OP = malloc(sizeof(struct trace_item));
	NO_OP->type = ti_NOP;
	struct trace_item *DONE;
	DONE = malloc(sizeof(struct trace_item));
	DONE->type = ti_DONE;
	struct trace_item *SQUASHED;
	SQUASHED = malloc(sizeof(struct trace_item));
	SQUASHED->type = ti_SQUASHED;
	
	struct trace_item *tr_entry = NO_OP;
	struct trace_item *tr_WB = NO_OP;
	struct trace_item *tr_MEM2 = NO_OP;
	struct trace_item *tr_MEM1 = NO_OP;
	struct trace_item *tr_EX2 = NO_OP;
	struct trace_item *tr_EX1 = NO_OP;
	struct trace_item *tr_ID = NO_OP;
	struct trace_item *tr_IF2 = NO_OP;
	struct trace_item *tr_IF1 = NO_OP;
	
	size_t size;
	bool stall = false;
	char *trace_file_name;
	int trace_view_on = 0;
	int branch_prediction_method = 0;
	
	unsigned char t_type = 0;
	unsigned char t_sReg_a= 0;
	unsigned char t_sReg_b= 0;
	unsigned char t_dReg= 0;
	unsigned int t_PC = 0;
	unsigned int t_Addr = 0;

	unsigned int cycle_number = 1;

	if (argc == 1) {
		fprintf(stdout, "\nUSAGE: <trace_file> <branch_prediction_method (default 0)> <trace_view_switch (default off)>\n");
		fprintf(stdout, "\n(branch_prediction_method)\n\t0- not taken\n\t1- 1-bit branch-predictor\n\t2- 2-bit branch predictor\n\n");
		fprintf(stdout, "\n(trace_view_switch)\n\t1- enable trace view\n\t2- disable trace view.\n\n");
		exit(0);
	}
		
	trace_file_name = argv[1];
	if (argc >= 3) branch_prediction_method = atoi(argv[2]);
	if (argc == 4) trace_view_on = atoi(argv[3]);

	printf("branch_prediction_method: %d\ntrace_view: %d\nargc: %d\n", branch_prediction_method, trace_view_on, argc);

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
		if (trace_view_on) {
			// print_cycle(tr_WB, cycle_number);
			print_pipeline(tr_entry, tr_IF1, tr_IF2, tr_ID, tr_EX1, tr_EX2, tr_MEM1, tr_MEM2, tr_WB, cycle_number);
		}
		if (!size && tr_WB->type == ti_DONE) {	 /* no more instructions (trace_items) to simulate */
			printf("+ Simulation terminates at cycle : %u\n", cycle_number);
			break;
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
		if (tr_EX1 != NULL && tr_EX1->dReg != 255 && (tr_EX1->dReg == tr_ID->sReg_a || tr_EX1->dReg == tr_ID->sReg_b)) {
			// check that instruction writes to a register
			switch(tr_EX1->type) {
				case ti_RTYPE:
				case ti_ITYPE:
				case ti_LOAD:
					tr_EX1 = NO_OP;
					printf("DATA HAZARD a\n");
					continue;
			}
		}
		/* Data hazard: b
			If an instruction in EX2 is a load instruction which will write
			into a register X while the instruction in ID is reading from
			register X, then the instruction in ID (and subsequent
			instructions) should stall since it does not have the correct
			content of register X. A no-op is injected in EX1/EX2.
		*/
		// what was in EX2 has already moved into MEM1, so test that stage
		if (tr_MEM1 != NULL && tr_MEM1->dReg != 255) {
			if(tr_MEM1->type == ti_LOAD) {
				if( tr_MEM1->dReg == tr_ID->sReg_a || tr_MEM1->dReg == tr_ID->sReg_b) {
					tr_EX1 = NO_OP;
					printf("DATA HAZARD b\n");
					continue;
				}
			}
		}

		/* Data hazard: c
		If an instruction in MEM1 is a load instruction which will write into a
		register X while the instruction in ID is reading from register X, then
		the instruction in ID (and subsequent instructions) should stall to 
		allow forwarding of the result from MEM2/WB to ID/EX1 (in the following
		cycle). A no-op is injected in EX1/EX2.
		*/
		// what was in MEM1 has already moved into MEM2, so test that stage
		if (tr_MEM2 != NULL && tr_MEM2->dReg != 255) {
			if(tr_MEM2->type == ti_LOAD) {
				if( tr_MEM2->dReg == tr_ID->sReg_a || tr_MEM2->dReg == tr_ID->sReg_b) {
					tr_EX1 = NO_OP;
					printf("DATA HAZARD c\n");
					continue;
				}
			}
		}

		/* Structural hazards: 
			if the instruction at WB is trying to write into the register file
			while the instruction at ID is trying to read from the register
			file, priority is given to the instruction at WB. The instructions
			at IF1, IF2 and ID are stalled for one cycle while the instruction
			at WB is using the register file.
		*/
		if (tr_WB != NULL && tr_ID != DONE) {
			switch(tr_WB->type) {
				case ti_RTYPE:
				case ti_ITYPE:
				case ti_LOAD:
					tr_EX1 = NO_OP;
					printf("STRUCTURAL HAZARD a\n");
					continue;
			}
		}
		tr_EX1 = tr_ID;

		// ID
		tr_ID = tr_IF2;

		// IF2
		tr_IF2 = tr_IF1;
		// determine if a branch in IF1 is taken, store result into dReg
		if (tr_IF1 != NULL && tr_IF1->type == ti_BRANCH) {
			if (tr_IF1->PC + 4 == tr_entry->PC) {
				// branch not taken
				tr_IF1->dReg = 0;
			} else {
				// branch taken
				tr_IF1->dReg = 1;
			}
		}

		// if ex2 is a branch
		if (tr_EX2 != NULL && tr_EX2->type == ti_BRANCH ) {
			// determine if branch was taken
			switch(branch_prediction_method) {
				case 0:
					printf("branch_prediction_method = 0!\n");
					if (tr_EX2->dReg == 1) {
						// add no-op to front, start next cycle
						tr_IF1 = SQUASHED;
						continue;
					}
					break;
				case 1:
					// prediction: 1-bit branch-predictor
					// TODO
					printf("branch_prediction_method = 1!\n");
					break;
				case 2:
					// prediction: 2-bit branch-predictor 
					// TODO
					printf("branch_prediction_method = 2!\n");
					break;
			}
		}

		// if ex1 is a branch
		if (tr_EX1 != NULL && tr_EX1->type == ti_BRANCH ) {
			// determine if branch was taken
			switch(branch_prediction_method) {
				case 0:
					printf("branch_prediction_method = 0!\n");
					if (tr_EX1->dReg == 1) {
						// add no-op to front, start next cycle
						tr_IF1 = SQUASHED;
						continue;
					}
					break;
				case 1:
					// prediction: 1-bit branch-predictor
					// TODO
					printf("branch_prediction_method = 1!\n");
					break;
				case 2:
					// prediction: 2-bit branch-predictor 
					// TODO
					printf("branch_prediction_method = 2!\n");
					break;
			}
		}

		// if id is a branch
		if (tr_ID != NULL && tr_ID->type == ti_BRANCH ) {
			// determine if branch was taken
			switch(branch_prediction_method) {
				case 0:
					printf("branch_prediction_method = 0!\n");
					if (tr_ID->dReg == 1) {
						// add no-op to front, start next cycle
						tr_IF1 = SQUASHED;
						continue;
					}
					break;
				case 1:
					// prediction: 1-bit branch-predictor
					// TODO
					printf("branch_prediction_method = 1!\n");
					break;
				case 2:
					// prediction: 2-bit branch-predictor 
					// TODO
					printf("branch_prediction_method = 2!\n");
					break;
			}
		}

		// if IF2 is a branch
		if (tr_IF2 != NULL && tr_IF2->type == ti_BRANCH ) {
			// determine if branch was taken
			switch(branch_prediction_method) {
				case 0:
					printf("branch_prediction_method = 0!\n");
					if (tr_IF2->dReg == 1) {
						// add no-op to front, start next cycle
						tr_IF1 = SQUASHED;
						continue;
					}
					break;
				case 1:
					// prediction: 1-bit branch-predictor
					// TODO
					printf("branch_prediction_method = 1!\n");
					break;
				case 2:
					// prediction: 2-bit branch-predictor 
					// TODO
					printf("branch_prediction_method = 2!\n");
					break;
			}
		}

		// if IF1 is a branch
		if (tr_IF1 != NULL && tr_IF1->type == ti_BRANCH ) {
			// determine if branch was taken
			switch(branch_prediction_method) {
				case 0:
					printf("branch_prediction_method = 0!\n");
					if (tr_IF1->dReg == 1) {
						// add no-op to front, start next cycle
						tr_IF1 = SQUASHED;
						continue;
					}
					break;
				case 1:
					// prediction: 1-bit branch-predictor
					// TODO
					printf("branch_prediction_method = 1!\n");
					break;
				case 2:
					// prediction: 2-bit branch-predictor 
					// TODO
					printf("branch_prediction_method = 2!\n");
					break;
			}
		}

			// // prediction: not taken
			// if(branch_prediction_method == 0) {
			// 	// if branch was taken
			// 	// if (/* condition */) {
			// 	// 	// insert no-op at entry point
			// 	// }
			// 	// otherwise it's fine
			// }
			// // prediction: 1-bit branch-predictor
			// if(branch_prediction_method == 1) {
			// 	printf("branch_prediction_method = 1!\n");
			// }
			// if(branch_prediction_method == 2) {
			// 	printf("branch_prediction_method = 2!\n");
			// }

		// IF1
		tr_IF1 = tr_entry;


		// get new entry
		if(!stall) {
			// if there is nothing left to read, trace_get_item doesn't
			// overwrite tr_entry, so we give it a default DONE value
			tr_entry = DONE;
			if (size) {
				size = trace_get_item(&tr_entry);
			}
		}


	}

	trace_uninit();

	free(NO_OP);
	free(DONE);

	exit(0);
}



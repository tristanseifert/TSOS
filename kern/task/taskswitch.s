.globl task_switch

###############################################################################
# Performs a context switch, assuming that a task_state_t struct has been
# pushed onto the stack.
###############################################################################
task_switch:
	
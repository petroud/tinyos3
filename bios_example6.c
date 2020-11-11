#include <bios.h>
#include <stdio.h>


/*A simulation program for core booting process 
based on interrupts and core barriers*/

/* interrupt handler */
void alarm_handler()
{
	printf("\n⟶  ALARM for core %u\n",cpu_core_id);
}

/*Core boot function*/
void bootfunc(){
  int alarm_time = cpu_core_id+2;

	fprintf(stderr, "⟶  Core %u gets an alarm at %d sec.\n", cpu_core_id, alarm_time);
  
  cpu_core_barrier_sync();
	cpu_interrupt_handler(ALARM, alarm_handler);
  /* Reset the timer.
			Note: setting this to a very small value may 
			deadlock, if the timer expires before cpu_core_halt() 
			is called. 
	*/
	bios_set_timer(1000000 * alarm_time);

  fprintf(stderr, "⟶  Core %u is halted.\n", cpu_core_id);
	cpu_core_halt();
	fprintf(stderr, " ↳ Core %u woke up\n", cpu_core_id);  
}

/*Boot message printing function*/
void boot_msg(){
    printf("------------------------------------------------------------------\n\n");
    printf(" _______  _                 ____    _____        ____      __   __\n"
 "|__   __|(_)               / __ \\  / ____|      |___ \\     \\ \\ / / \n"
   "   | |    _  _ __   _   _ | |  | || (___          __) |     \\ V / \n"
   "   | |   | || '_ \\ | | | || |  | | \\___ \\        |__ <       > < \n" 
   "   | |   | || | | || |_| || |__| | ____) |       ___) |     / . \\ \n" 
   "   |_|   |_||_| |_| \\__, | \\____/ |_____/       |____/     /_/ \\_\\ \n"
   "                     __/ |                                        \n"
   "                    |___/                                         \n\n");
    printf("-- A version of TinyOS made by a team of students ----------------\n\n");
    printf("⟶  Boot process initialized, system setting up...\n");
    printf("⟶  Assigning wake up times...!\n\n");
}


int main()
{
  boot_msg();
  
  /* Run simulation with 4 cores */
	vm_boot(bootfunc, 4, 0);

  return 0;
}

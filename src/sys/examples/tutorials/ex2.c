#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: ex2.c,v 1.4 1998/04/15 15:08:51 bsmith Exp curfman $";
#endif

/* Usage:  mpirun ex2 [-help] [all PETSc options] */

static char help[] = "This is an introductory PETSc example that illustrates\n\
the use of synchronized print statements.\n\n";

/*T
   Concepts: Introduction to PETSc;
   Routines: PetscInitialize(); PetscPrintf(); PetscSynchronizedPrintf();
   Routines: PetscSynchronizedFlush(); PetscFinalize();
   Processors: n
T*/
 
#include "petsc.h"
int main(int argc,char **argv)
{
  int ierr, rank, size;

  /*
    Every PETSc routine should begin with the PetscInitialize() routine.
    argc, argv - These command line arguments are taken to extract the options
                 supplied to PETSc and options supplied to MPI.
    help       - When PETSc executable is invoked with the option -help, 
                 it prints the various options that can be applied at 
                 runtime.  The user can use the "help" variable place
                 additional help messages in this printout.
  */
  ierr = PetscInitialize(&argc,&argv,(char *)0,help); CHKERRA(ierr);

  /* 
     The following MPI calls return the number of processes
     being used and the rank of this process in the group.
   */
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size); CHKERRA(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank); CHKERRA(ierr);

  /* 
     Here we would like to print only one message that represents
     all the processes in the group.  We use PetscPrintf() with the 
     communicator PETSC_COMM_WORLD.  Thus, only one message is
     printed representng PETSC_COMM_WORLD, i.e., all the processors.
  */
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Number of processors = %d, rank = %d\n",size,rank); CHKERRA(ierr);
  /* 
     Here we would like to print info from each process such that
     output from process "n" appears after output from process "n-1".
     To do this we use a combination of PetscSynchronizedPrintf() and
     PetscSynchronizedFlush() with the communicator PETSC_COMM_WORLD.
     All the processes print the message, one after another. 
     PetscSynchronizedFlush() indicates that the current process in the
     given communicator has concluded printing, so that the next process
     in the communicator can begin printing to the screen.
     */
  ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"[%d] Synchronized Hello World.\n",rank); CHKERRA(ierr);
  ierr = PetscSynchronizedPrintf(PETSC_COMM_WORLD,"[%d] Synchronized Hello World - Part II.\n",rank); CHKERRA(ierr);
  ierr = PetscSynchronizedFlush(PETSC_COMM_WORLD); CHKERRA(ierr);
  /*
    Here a barrier is used to separate the two states.
  */
  ierr = MPI_Barrier(PETSC_COMM_WORLD); CHKERRA(ierr);

  /*
    Here we simply use PetscPrintf() with the communicator PETSC_COMM_SELF
    (where each process is considered separately).  Thus, this time the
    output from different processes does not appear in any particular order.
  */
  ierr = PetscPrintf(PETSC_COMM_SELF,"[%d] Jumbled Hello World\n",rank); CHKERRA(ierr);

  /*
     Always call PetscFinalize() before exiting a program.  This routine
       - finalizes the PETSc libraries as well as MPI
       - provides summary and diagnostic information if certain runtime
         options are chosen (e.g., -log_summary).  See PetscFinalize()
     manpage for more information.
  */
  ierr = PetscFinalize(); CHKERRA(ierr);
  return 0;
}

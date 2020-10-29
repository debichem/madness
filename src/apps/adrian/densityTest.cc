

#include <stdlib.h>

#include "TDDFT.h"  // All response functions/objects enter through this
#include "adrian/density.h"
#include "adrian/global_functions.h"
#include "adrian/property_functions.h"

#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_SYS_STAT_H) && \
    defined(HAVE_UNISTD_H)
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
static inline int file_exists(const char* inpname) {
  struct stat buffer;
  int rc = stat(inpname, &buffer);
  return (rc == 0);
}
#endif

int main(int argc, char** argv) {
  // Initialize MADNESS mpi
  initialize(argc, argv);
  World world(SafeMPI::COMM_WORLD);
  startup(world, argc, argv);

  // This makes a default input file name of 'input'
  const char* input = "input";

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      input = argv[i];
      break;
    }
  }

  if (!file_exists(input)) throw "input file not found";
  // first step is to read the input for Rparams and Gparams
  std::shared_ptr<std::istream> shared_input =
      std::make_shared<std::ifstream>(input);
  ResponseParameters Rparams;
  GroundParameters Gparams;

  if (world.rank() == 0) {
    if (shared_input->fail())
      MADNESS_EXCEPTION("Response failed to open input stream", 0);
    // Welcome user (future ASCII art of Robert goes here)
    print("\n   Preparing to solve the TDHF equations.\n");
    // Read input files
    Rparams.read(*shared_input);
    // Print out what was read in
  }
  Gparams.read(world, Rparams.archive);
  if (world.rank() == 0) {
    Gparams.print_params();
    print_molecule(world, Gparams);
  }
  // if a proerty calculation set the number of states
  if (Rparams.property) {
    Rparams.SetNumberOfStates(Gparams.molecule);
  }

  // print params
  if (world.rank() == 0) {
    Rparams.print_params();
  }
  // Broadcast to all other nodes

  // Create the TDHF object

  FirstOrderDensity densityTest(world, Rparams, Gparams);
  //
  densityTest.PlotResponseDensity(world);
  densityTest.PrintDensityInformation();

  if (Rparams.property) {  //

    Tensor<double> alpha = densityTest.ComputeSecondOrderPropertyTensor(world);
    PrintSecondOrderAnalysis(world, alpha, densityTest.GetFrequencyOmega(),
                             Rparams);
  }

  world.gop.fence();
  finalize();

  return 0;
}

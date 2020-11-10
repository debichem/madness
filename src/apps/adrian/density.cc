
#include "adrian/density.h"

#include <ResponseFunction2.h>
#include <TDDFT.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../../madness/mra/funcplot.h"
#include "adrian/global_functions.h"
#include "adrian/property.h"

typedef Tensor<double> TensorT;
typedef Function<double, 3> FunctionT;
typedef std::shared_ptr<FunctionFunctorInterface<double, 3>> FunctorT;
typedef FunctionFactory<double, 3> FactoryT;
typedef Vector<double, 3> CoordinateT;
typedef std::vector<real_function_3d> VectorFunction3DT;

// base class for a density
// operator used to create it
// homogeneous sol----x and y functions
// particular sol --- depends on lower order functions used to create it
// it also needs an xc functional
// The Rparams and Gparmas used to create the density
//
FirstOrderDensity::FirstOrderDensity(ResponseParameters Rparams,
                                     GroundParameters Gparams) {
  this->Rparams = Rparams;
  this->Gparams = Gparams;
}
void FirstOrderDensity::ComputeDensity(World &world) {
  // right now everything uses copy
  property = Rparams.response_type;

  TDHF calc(world, Rparams, Gparams);
  if (calc.Rparams.property) {
    calc.ComputeFrequencyResponse(world, property);
  } else {
    calc.solve(world);
  }
  // omega is determined by the type of calculation
  // property calculation at single frequency
  // excited stat calculation at multipe frequencies
  omega = calc.GetFrequencyOmega();
  property_operator = calc.GetPropertyObject();

  x = calc.GetResponseFunctions("x");
  y = calc.GetResponseFunctions("y");

  P = calc.GetPVector();
  Q = calc.GetQVector();

  num_response_states = x.size();
  num_ground_states = x[0].size();
  // get the response densities for our states
  rho_omega = calc.transition_density(world, Gparams.orbitals, x, y);
  if (Rparams.save_density) {
    SaveDensity(world, Rparams.save_density_file);
  }
}

int FirstOrderDensity::GetNumberResponseStates() { return num_response_states; }
int FirstOrderDensity::GetNumberGroundStates() { return num_ground_states; }
VectorFunction3DT FirstOrderDensity::GetDensityVector() { return rho_omega; }
const Molecule FirstOrderDensity::GetMolecule() { return Gparams.molecule; }
TensorT FirstOrderDensity::GetFrequencyOmega() { return omega; }
ResponseParameters FirstOrderDensity::GetResponseParameters() {
  return Rparams;
}

void FirstOrderDensity::PrintDensityInformation() {
  // print
  //
  print("Response Density Information");
  print(property, " response at", omega(0, 0), "frequency using ", Rparams.xc,
        " exchange functional");
  print("Number of Response States : ", num_response_states);
  print("Number of Ground States : ", num_ground_states);
}

void FirstOrderDensity::PlotResponseDensity(World &world) {
  // Doing line plots along each axis
  if (world.rank() == 0) print("\n\nStarting plots");
  coord_3d lo, hi;
  char plotname[500];
  double Lp = std::min(Gparams.L, 24.0);
  if (world.rank() == 0) print("x:");
  // x axis
  lo[0] = 0.0;
  lo[1] = 0.0;
  lo[2] = 0.0;
  hi[0] = Lp;
  hi[1] = 0.0;
  hi[2] = 0.0;

  for (int i = 0; i < num_response_states; i++) {
    std::snprintf(plotname, sizeof(plotname),
                  "plot_transition_density_%d_%d_x.plt",
                  FunctionDefaults<3>::get_k(), i);
    plot_line(plotname, 5001, lo, hi, rho_omega[i]);
  }
}
Tensor<double> FirstOrderDensity::ComputeSecondOrderPropertyTensor(
    World &world) {
  Tensor<double> H;
  // do some printing before we compute so we know what we are working with
  vector<real_function_3d> p_density =
      zero_functions<double, 3>(world, P.size());
  vector<real_function_3d> q_density =
      zero_functions<double, 3>(world, Q.size());
  // Two ways to compute... -2()
  for (size_t i = 0; i < P.size(); i++) {
    for (size_t j = 0; j < P[0].size(); j++) {
      p_density[i] += P[i][j];
      q_density[i] += Q[i][j];
    }
  }

  vector<real_function_3d> Pert_density =
      zero_functions<double, 3>(world, P.size());
  for (size_t i = 0; i < P.size(); i++) {
    Pert_density[i] = p_density[i] + q_density[i];
  }

  for (size_t i = 0; i < property_operator.operator_vector.size(); i++) {
    if (world.rank() == 0) {
      print("property operator vector i = ", i,
            "norm = ", property_operator.operator_vector[i].norm2());
    }
  }

  for (int i = 0; i < num_response_states; i++) {
    print("norm of rho i", rho_omega[i].norm2());
  }

  H = matrix_inner(world, rho_omega, Pert_density, true);
  H = -H;
  for (int i = 0; i < num_response_states; i++) {
    for (int j = 0; j < property_operator.num_operators; j++) {
      print("norm of H  i: ", i, " j: ", j, " = ", H(i, j));
    }
  }
  return H;
}

void FirstOrderDensity::PrintSecondOrderAnalysis(
    World &world, const Tensor<double> alpha_tensor) {
  Tensor<double> V, epolar;
  syev(alpha_tensor, V, epolar);
  double Dpolar_average = 0.0;
  double Dpolar_iso = 0.0;
  for (unsigned int i = 0; i < 3; ++i)
    Dpolar_average = Dpolar_average + epolar[i];
  Dpolar_average = Dpolar_average / 3.0;
  Dpolar_iso =
      sqrt(.5) * sqrt(std::pow(alpha_tensor(0, 0) - alpha_tensor(1, 1), 2) +
                      std::pow(alpha_tensor(1, 1) - alpha_tensor(2, 2), 2) +
                      std::pow(alpha_tensor(2, 2) - alpha_tensor(0, 0), 2));

  int num_states = Rparams.states;

  if (world.rank() == 0) {
    print("\nTotal Dynamic Polarizability Tensor");
    printf("\nFrequency  = %.6f a.u.\n\n", omega(0, 0));
    // printf("\nWavelength = %.6f a.u.\n\n", Rparams.omega * ???);
    print(alpha_tensor);
    printf("\tEigenvalues = ");
    printf("\t %.6f \t %.6f \t %.6f \n", epolar[0], epolar[1], epolar[2]);
    printf("\tIsotropic   = \t %.6f \n", Dpolar_average);
    printf("\tAnisotropic = \t %.6f \n", Dpolar_iso);
    printf("\n");

    for (long i = 0; i < num_states; i++) {
      print(epolar[i]);
    }
  }
}
void FirstOrderDensity::SaveDensity(World &world, std::string name) {
  // Archive to write everything to
  archive::ParallelOutputArchive ar(world, name.c_str(), 1);
  // Just going to enforce 1 io server

  ar &property;
  ar &omega;
  ar &num_response_states;
  ar &num_ground_states;
  // Save response functions x and y
  // x first
  for (int i = 0; i < num_response_states; i++) {
    for (int j = 0; j < num_ground_states; j++) {
      ar &x[i][j];
    }
  }

  // y second
  for (int i = 0; i < num_response_states; i++) {
    for (int j = 0; j < num_ground_states; j++) {
      ar &y[i][j];
    }
  }
  for (int i = 0; i < num_response_states; i++) {
    ar &rho_omega[i];
  }
  for (int i = 0; i < property_operator.num_operators; i++) {
    ar &property_operator.operator_vector[i];
  }

  for (int i = 0; i < num_response_states; i++) {
    for (int j = 0; j < num_ground_states; j++) {
      ar &P[i][j];
    }
  }
  for (int i = 0; i < num_response_states; i++) {
    for (int j = 0; j < num_ground_states; j++) {
      ar &Q[i][j];
    }
  }
}
// Load a response calculation
void FirstOrderDensity::LoadDensity(World &world, std::string name,
                                    ResponseParameters Rparams,
                                    GroundParameters Gparams) {
  // create XCF Object
  xcf.initialize(Rparams.xc, false, world, true);

  archive::ParallelInputArchive ar(world, name.c_str());
  // Reading in, in this order;

  ar &property;

  if (property.compare("dipole") == 0) {
    if (world.rank() == 0) print("creating dipole property operator");
    this->property_operator = Property(world, "dipole");
  } else if (property.compare("nuclear") == 0) {
    if (world.rank() == 0) print("creating nuclear property operator");
    this->property_operator = Property(world, "nuclear", Gparams.molecule);
  }
  print("property:", property);

  ar &omega;
  print("omega:", omega);
  ar &num_response_states;
  print("num_response_states:", num_response_states);
  ar &num_ground_states;
  print("num_ground_states:", num_ground_states);

  this->x = ResponseFunction(world, num_response_states, num_ground_states);
  this->y = ResponseFunction(world, num_response_states, num_ground_states);

  this->P = ResponseFunction(world, num_response_states, num_ground_states);
  this->Q = ResponseFunction(world, num_response_states, num_ground_states);

  for (int i = 0; i < Rparams.states; i++) {
    for (unsigned int j = 0; j < Gparams.num_orbitals; j++) {
      ar &x[i][j];
      print("norm of x ", x[i][j].norm2());
    }
  }
  world.gop.fence();

  for (int i = 0; i < Rparams.states; i++) {
    for (unsigned int j = 0; j < Gparams.num_orbitals; j++) {
      ar &y[i][j];
      print("norm of y ", y[i][j].norm2());
    }
  }

  world.gop.fence();
  this->rho_omega = zero_functions<double, 3>(world, num_response_states);
  for (int i = 0; i < num_response_states; i++) {
    ar &rho_omega[i];
    print("norm of rho_omega ", rho_omega[i].norm2());
  }

  for (int i = 0; i < property_operator.num_operators; i++) {
    print("norm of operator before ",
          property_operator.operator_vector[i].norm2());
    ar &property_operator.operator_vector[i];
    print("norm of operator after",
          property_operator.operator_vector[i].norm2());
  }

  for (int i = 0; i < Rparams.states; i++) {
    for (unsigned int j = 0; j < Gparams.num_orbitals; j++) {
      ar &P[i][j];
      print("norm of P ", P[i][j].norm2());
    }
  }
  world.gop.fence();

  for (int i = 0; i < Rparams.states; i++) {
    for (unsigned int j = 0; j < Gparams.num_orbitals; j++) {
      ar &Q[i][j];
      print("norm of y ", Q[i][j].norm2());
    }
  }

  world.gop.fence();
}

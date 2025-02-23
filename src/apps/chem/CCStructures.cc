/*
 * CCStructures.cc
 *
 *  Created on: Jan 4, 2017
 *      Author: kottmanj
 */

#include"CCStructures.h"

namespace madness {

void
CCMessenger::output(const std::string& msg) const {
    if (scientific) std::cout << std::scientific;
    else std::cout << std::fixed;

    std::cout << std::setprecision(output_prec);
    if (world.rank() == 0) std::cout << msg << std::endl;
}

void
CCMessenger::section(const std::string& msg) const {
    if (world.rank() == 0) {
        std::cout << "\n" << std::setw(msg.size() + 10) << std::setfill('*') << "\n";
        std::cout << std::setfill(' ');
        output(msg);
        std::cout << std::setw(msg.size() + 10) << std::setfill('*') << "\n\n";
        std::cout << std::setfill(' ');
    }
}

void
CCMessenger::subsection(const std::string& msg) const {
    if (world.rank() == 0) {
        std::cout << "\n" << std::setw(msg.size() + 5) << std::setfill('-') << "\n";
        std::cout << std::setfill(' ');
        output(msg);
        std::cout << std::setw(msg.size() + 5) << std::setfill('-') << "\n";
        std::cout << std::setfill(' ');
    }
}

void
CCMessenger::warning(const std::string& msg) const {
    std::string tmp = "!!!!!WARNING:" + msg + "!!!!!!";
    output(tmp);
    warnings.push_back(msg);
}

void
CCTimer::info(const bool debug, const double norm) {
    if (debug == true) {
        update_time();
        std::string s_norm = "";
        if (norm != 12345.6789) s_norm = ", ||result||=" + std::to_string(norm);

        if (world.rank() == 0) {
            std::cout << std::setfill(' ') << std::scientific << std::setprecision(2)
                      << "Timer: " << time_wall << " (Wall), " << time_cpu << " (CPU)" << s_norm
                      << ", (" + operation + ")" << "\n";
        }
    }
}

void
CCFunction::info(World& world, const std::string& msg) const {
    if (world.rank() == 0) {
        std::cout << "Information about 3D function: " << name() << " " << msg << std::endl;
        std::cout << std::setw(10) << std::setfill(' ') << std::setw(50) << " |f|    : " << function.norm2()
                  << std::endl;
        std::cout << std::setw(10) << std::setfill(' ') << std::setw(50) << " |error|: " << current_error << std::endl;
    }
}

std::string
CCFunction::name() const {
    if (type == HOLE) {
        return "phi" + stringify(i);
    } else if (type == PARTICLE) {
        return "tau" + stringify(i);
    } else if (type == MIXED) {
        return "t" + stringify(i);
    } else if (type == RESPONSE) {
        return "x" + stringify(i);
    } else {
        return "function" + stringify(i);
    }
}

madness::CC_vecfunction
CC_vecfunction::copy() const {
    std::vector<CCFunction> vn;
    for (auto x : functions) {
        const CCFunction fn(madness::copy(x.second.function), x.second.i, x.second.type);
        vn.push_back(fn);
    }
    CC_vecfunction result(vn, type);
    result.irrep = irrep;
    return result;
}

std::string
CC_vecfunction::name() const {
    if (type == PARTICLE) return "tau";
    else if (type == HOLE) return "phi";
    else if (type == MIXED) return "t";
    else if (type == RESPONSE) {
        if (excitation < 0) MADNESS_EXCEPTION("EXCITATION VECTOR HAS NO NUMBER ASSIGNED!", 1);
        return std::to_string(excitation) + "_" + "x";
    } else return "UNKNOWN";
}

void
CC_vecfunction::print_size(const std::string& msg) const {
    if (functions.size() == 0) {
        std::cout << "CC_vecfunction " << msg << " is empty\n";
    } else {
        std::string msg2;
        if (msg == "!?not assigned!?") msg2 = "";
        else msg2 = "_(" + msg + ")";

        for (auto x : functions) {
            x.second.function.print_size(x.second.name() + msg2);
        }
    }
}

madness::CCPairFunction
CCPairFunction::operator=(CCPairFunction& other) {
    MADNESS_ASSERT(type == other.type);
    a = other.a;
    b = other.b;
    op = other.op;
    x = other.x;
    y = other.y;
    u = other.u;
    return *this;
}

madness::CCPairFunction
CCPairFunction::copy() const {
    if (type == PT_FULL) {
        return CCPairFunction(world, madness::copy(u));
    } else if (type == PT_DECOMPOSED) {
        return CCPairFunction(world, madness::copy(world, a), madness::copy(world, b));
    } else if (type == PT_OP_DECOMPOSED) {
        return CCPairFunction(world, op, CCFunction(madness::copy(x.function), x.i, x.type),
                              CCFunction(madness::copy(y.function), y.i, y.type));
    } else MADNESS_EXCEPTION("Unknown type", 1)

    ;
}

madness::CCPairFunction
CCPairFunction::invert_sign() {
    if (type == PT_FULL) {
        real_function_6d uc = madness::copy(u);
        uc.scale(-1.0);
        u = uc;
    } else if (type == PT_DECOMPOSED) {
        vector_real_function_3d ac = madness::copy(world, a);
        scale(world, ac, -1.0);
        a = ac;
    } else if (type == PT_OP_DECOMPOSED) {
        real_function_3d tmp = madness::copy(x.function);
        tmp.scale(-1.0);
        x.function = tmp;
    } else MADNESS_EXCEPTION("wrong type in CCPairFunction invert_sign", 1)

    ;
    return *this;
}

void
CCPairFunction::print_size() const {
    if (type == PT_FULL) {
        u.print_size(name());
    } else if (type == PT_DECOMPOSED) {
        madness::print_size(world, a, "a from " + name());
        madness::print_size(world, b, "b from " + name());
    } else if (type == PT_OP_DECOMPOSED) {
        x.function.print_size(x.name() + " from " + name());
        y.function.print_size(y.name() + " from " + name());
    } else MADNESS_EXCEPTION("Unknown type in CCPairFunction, print_size", 1)

    ;
}

double
CCPairFunction::make_xy_u(const CCFunction& xx, const CCFunction& yy) const {
    double result = 0.0;
    switch (type) {
        default: MADNESS_EXCEPTION("Undefined enum", 1);
        case PT_FULL: {
            real_function_6d ij = CompositeFactory<double, 6, 3>(world).particle1(madness::copy(xx.function)).particle2(
                    madness::copy(yy.function));
            result = inner(u, ij);
        }
            break;
        case PT_DECOMPOSED: {
            for (size_t i = 0; i < a.size(); i++)
                result += (xx.function.inner(a[i])) * (yy.function.inner(b[i]));
        }
            break;
        case PT_OP_DECOMPOSED: {
            result = yy.function.inner((*op)(xx, x) * y.function);
        }
            break;
    }
    return result;
}

void
CCPair::info() const {
    if (constant_part.world().rank() == 0) {
        std::cout << "\nInformation about electron pair: " << name() << "\n";
    }
    constant_part.print_size("ConstantPart");
    for (size_t k = 0; k < functions.size(); k++)
        functions[k].print_size();
    if (constant_part.world().rank() == 0) {
        std::cout << "\n";
    }
}

madness::vector_real_function_3d
CCIntermediatePotentials::operator()(const CC_vecfunction& f, const PotentialType& type) const {
    output("Getting " + assign_name(type) + " for " + f.name());
    vector_real_function_3d result;
    if (type == POT_singles_ and (f.type == PARTICLE or f.type == MIXED)) return current_singles_potential_gs_;
    else if (type == POT_singles_ and f.type == RESPONSE) return current_singles_potential_ex_;
    else if (type == POT_s2b_ and f.type == PARTICLE) return current_s2b_potential_gs_;
    else if (type == POT_s2b_ and f.type == RESPONSE) return current_s2b_potential_ex_;
    else if (type == POT_s2c_ and f.type == PARTICLE) return current_s2c_potential_gs_;
    else if (type == POT_s2c_ and f.type == RESPONSE) return current_s2c_potential_ex_;
    else if (f.type == HOLE) {
        output(assign_name(type) + " is zero for HOLE states");
        result = zero_functions<double, 3>(world, f.size());
    } else {
        output("ERROR: Potential was not supposed to be stored");
        MADNESS_EXCEPTION("Potential was not supposed to be stored", 1);
    }

    if (result.empty()) output("!!!WARNING: Potential is empty!!!");

    return result;
}

madness::real_function_3d
CCIntermediatePotentials::operator()(const CCFunction& f, const PotentialType& type) const {
    output("Getting " + assign_name(type) + " for " + f.name());
    real_function_3d result = real_factory_3d(world);
    if (type == POT_singles_ and (f.type == PARTICLE or f.type == MIXED))
        return current_singles_potential_gs_[f.i - parameters.freeze()];
    else if (type == POT_singles_ and f.type == RESPONSE) return current_singles_potential_ex_[f.i - parameters.freeze()];
    else if (type == POT_s2b_ and f.type == PARTICLE) return current_s2b_potential_gs_[f.i - parameters.freeze()];
    else if (type == POT_s2b_ and f.type == RESPONSE) return current_s2b_potential_ex_[f.i - parameters.freeze()];
    else if (type == POT_s2c_ and f.type == PARTICLE) return current_s2c_potential_gs_[f.i - parameters.freeze()];
    else if (type == POT_s2c_ and f.type == RESPONSE) return current_s2c_potential_ex_[f.i - parameters.freeze()];
    else if (f.type == HOLE) output(assign_name(type) + " is zero for HOLE states");
    else MADNESS_EXCEPTION("Potential was not supposed to be stored", 1)

    ;
    if (result.norm2() < FunctionDefaults<3>::get_thresh())
        output("WARNING: Potential seems to be zero ||V||=" + std::to_string(double(result.norm2())));

    return result;
}

void
CCIntermediatePotentials::insert(const vector_real_function_3d& potential, const CC_vecfunction& f,
                                 const PotentialType& type) {
    output("Storing potential: " + assign_name(type) + " for " + f.name());
    MADNESS_ASSERT(!potential.empty());
    if (type == POT_singles_ && (f.type == PARTICLE || f.type == MIXED)) current_singles_potential_gs_ = potential;
    else if (type == POT_singles_ && f.type == RESPONSE) current_singles_potential_ex_ = potential;
    else if (type == POT_s2b_ && f.type == PARTICLE) current_s2b_potential_gs_ = potential;
    else if (type == POT_s2b_ && f.type == RESPONSE) current_s2b_potential_ex_ = potential;
    else if (type == POT_s2c_ && f.type == PARTICLE) {
        current_s2c_potential_gs_ = potential;
    } else if (type == POT_s2c_ && f.type == RESPONSE) {
        current_s2c_potential_ex_ = potential;
    }
}

void CCParameters::set_derived_values() {
    if (not kain()) set_derived_value("kain_subspace",0);

    // set all parameters that were not explicitly given
    set_derived_value("tight_thresh_6d",thresh_6D()*0.1);
    set_derived_value("thresh_3d",thresh_6D()*0.01);
    set_derived_value("tight_thresh_3d",thresh_3D()*0.1);
//    if (thresh_operators == uninitialized) thresh_operators = 1.e-6;
//    if (thresh_operators_3D == uninitialized) thresh_operators_3D = thresh_operators;
//    if (thresh_operators_6D == uninitialized) thresh_operators_6D = thresh_operators;
//    if (thresh_bsh_3D == uninitialized) thresh_bsh_3D = thresh_operators_3D;
//    if (thresh_bsh_6D == uninitialized) thresh_bsh_6D = thresh_operators_6D;
//    if (thresh_poisson == uninitialized) thresh_poisson = thresh_operators_3D;
//    if (thresh_f12 == uninitialized) thresh_f12 = thresh_operators_3D;
    set_derived_value("thresh_ue",tight_thresh_6D());
    set_derived_value("dconv_6d",thresh_6D());
    set_derived_value("dconv_3d",thresh_6D());
    set_derived_value("econv",0.1*dconv_6D());
    set_derived_value("econv_pairs",econv());

    set_derived_value("no_compute_gs",no_compute());
    set_derived_value("no_compute_mp2",no_compute() and no_compute_gs());
    set_derived_value("no_compute_cc2",no_compute() and no_compute_gs());
    set_derived_value("no_compute_cispd",no_compute() and no_compute_response());
    set_derived_value("no_compute_response",no_compute());
    set_derived_value("restart",no_compute() == true and restart() == false);

    if (thresh_3D() < 1.1e-1) set_derived_value("output_prec",std::size_t(3));
    if (thresh_3D() < 1.1e-2) set_derived_value("output_prec",std::size_t(4));
    if (thresh_3D() < 1.1e-3) set_derived_value("output_prec",std::size_t(5));
    if (thresh_3D() < 1.1e-4) set_derived_value("output_prec",std::size_t(6));
    if (thresh_3D() < 1.1e-5) set_derived_value("output_prec",std::size_t(7));
    if (thresh_3D() < 1.1e-6) set_derived_value("output_prec",std::size_t(8));
    std::cout.precision(output_prec());
}

void CCParameters::information(World& world) const {
    if (world.rank()==0) {
        print("cc2","end");
        if (calc_type() != CT_LRCCS and calc_type() != CT_TDHF) {
            std::cout << "The Ansatz for the Pair functions |tau_ij> is: ";
            if (QtAnsatz()) std::cout << "(Qt)f12|titj> and response: (Qt)f12(|tixj> + |xitj>) - (OxQt + QtOx)f12|titj>";
            else std::cout << "Qf12|titj> and response: Qf12(|xitj> + |tixj>)" << std::endl;
        }
    }
}

void CCParameters::sanity_check(World& world) const {
    size_t warnings = 0;
    if (FunctionDefaults<3>::get_thresh() > 0.01 * FunctionDefaults<6>::get_thresh())
        warnings += warning(world, "3D Thresh is too low, should be 0.01*6D_thresh");
    if (FunctionDefaults<3>::get_thresh() > 0.1 * FunctionDefaults<6>::get_thresh())
        warnings += warning(world, "3D Thresh is way too low, should be 0.01*6D_thresh");
    if (FunctionDefaults<3>::get_cell_min_width() != FunctionDefaults<6>::get_cell_min_width())
        warnings += warning(world, "3D and 6D Cell sizes differ");
    if (FunctionDefaults<3>::get_k() != FunctionDefaults<6>::get_k())
        warnings += warning(world, "k-values of 3D and 6D differ ");
    if (FunctionDefaults<3>::get_truncate_mode() != 3) warnings += warning(world, "3D Truncate mode is not 3");
    if (FunctionDefaults<6>::get_truncate_mode() != 3) warnings += warning(world, "6D Truncate mode is not 3");
    if (dconv_3D() < FunctionDefaults<3>::get_thresh())
        warnings += warning(world, "Demanded higher convergence than threshold for 3D");
    if (dconv_6D() < FunctionDefaults<6>::get_thresh())
        warnings += warning(world, "Demanded higher convergence than threshold for 6D");
    if (thresh_3D() != FunctionDefaults<3>::get_thresh())
        warnings += warning(world, "3D thresh set unequal 3D thresh demanded");
    if (thresh_6D() != FunctionDefaults<6>::get_thresh())
        warnings += warning(world, "6D thresh set unequal 6D thresh demanded");
    if (econv() < FunctionDefaults<3>::get_thresh())
        warnings += warning(world, "Demanded higher energy convergence than threshold for 3D");
    if (econv() < FunctionDefaults<6>::get_thresh())
        warnings += warning(world, "Demanded higher energy convergence than threshold for 6D");
    if (econv() < 0.1 * FunctionDefaults<3>::get_thresh())
        warnings += warning(world,
                            "Demanded higher energy convergence than threshold for 3D (more than factor 10 difference)");
    if (econv() < 0.1 * FunctionDefaults<6>::get_thresh())
        warnings += warning(world,
                            "Demanded higher energy convergence than threshold for 6D (more than factor 10 difference)");
    // Check if the 6D thresholds are not too high
    if (thresh_6D() < 1.e-3) warnings += warning(world, "thresh_6D is smaller than 1.e-3");
    if (thresh_6D() < tight_thresh_6D()) warnings += warning(world, "tight_thresh_6D is larger than thresh_6D");
    if (thresh_6D() < tight_thresh_3D()) warnings += warning(world, "tight_thresh_3D is larger than thresh_3D");
    if (thresh_6D() < 1.e-3) warnings += warning(world, "thresh_6D is smaller than 1.e-3");
    if (thresh_Ue() < 1.e-4) warnings += warning(world, "thresh_Ue is smaller than 1.e-4");
    if (thresh_Ue() > 1.e-4) warnings += warning(world, "thresh_Ue is larger than 1.e-4");
    if (thresh_3D() > 0.01 * thresh_6D())
        warnings += warning(world, "Demanded 6D thresh is to precise compared with the 3D thresh");
    if (thresh_3D() > 0.1 * thresh_6D())
        warnings += warning(world, "Demanded 6D thresh is to precise compared with the 3D thresh");
    if (kain() and kain_subspace() == 0)
        warnings += warning(world, "Demanded Kain solver but the size of the iterative subspace is set to zero");
    if (warnings > 0) {
        if (world.rank() == 0) std::cout << warnings << "Warnings in parameters sanity check!\n\n";
    } else {
        if (world.rank() == 0) std::cout << "Sanity check for parameters passed\n\n" << std::endl;
    }
    if (restart() == false and no_compute() == true) {
        warnings += warning(world, "no_compute flag detected but no restart flag");
    }
}

real_function_3d
CCConvolutionOperator::operator()(const CCFunction& bra, const CCFunction& ket, const bool use_im) const {
    real_function_3d result;
    if (not use_im) {
        if (world.rank() == 0)
            std::cout << "Recalculating <" << bra.name() << "|" << assign_name(operator_type) << "|" << ket.name()
                      << ">\n";
        result = ((*op)(bra.function * ket.function)).truncate();
    } else if (bra.type == HOLE and ket.type == HOLE and not imH.allpairs.empty()) result = imH(bra.i, ket.i);
    else if (bra.type == HOLE and ket.type == RESPONSE and not imR.allpairs.empty()) result = imR(bra.i, ket.i);
    else if (bra.type == HOLE and ket.type == PARTICLE and not imP.allpairs.empty()) result = imP(bra.i, ket.i);
    else if (bra.type == HOLE and ket.type == MIXED and (not imP.allpairs.empty() and not imH.allpairs.empty()))
        result = (imH(bra.i, ket.i) + imP(bra.i, ket.i));
    else {
        //if(world.rank()==0) std::cout <<"No Intermediate found for <" << bra.name()<<"|"<<assign_name(operator_type) <<"|"<<ket.name() <<"> ... recalculate \n";
        result = ((*op)(bra.function * ket.function)).truncate();
    }
    return result;
}

real_function_6d CCConvolutionOperator::operator()(const real_function_6d& u, const size_t particle) const {
    MADNESS_ASSERT(particle == 1 or particle == 2);
    MADNESS_ASSERT(operator_type == OT_G12);
    op->particle() = particle;
    return (*op)(u);
}

real_function_3d
CCConvolutionOperator::operator()(const CCFunction& bra, const real_function_6d& u, const size_t particle) const {
    MADNESS_ASSERT(particle == 1 or particle == 2);
    MADNESS_ASSERT(operator_type == OT_G12);
    const real_function_6d tmp = multiply(copy(u), copy(bra.function), particle);
    op->particle() = particle;
    const real_function_6d g_tmp = (*op)(tmp);
    const real_function_3d result = g_tmp.dirac_convolution<3>();
    return result;
}

real_function_3d CCPairFunction::project_out(const CCFunction& f, const size_t particle) const {
    MADNESS_ASSERT(particle == 1 or particle == 2);
    real_function_3d result;
    switch (type) {
        default: MADNESS_EXCEPTION("Undefined enum", 1);
        case PT_FULL :
            result = u.project_out(f.function, particle - 1); // this needs 0 or 1 for particle but we give 1 or 2
            break;
        case PT_DECOMPOSED :
            result = project_out_decomposed(f.function, particle);
            break;
        case PT_OP_DECOMPOSED:
            result = project_out_op_decomposed(f, particle);
            break;
    }
    if (not result.is_initialized()) MADNESS_EXCEPTION("Result of project out on CCPairFunction was not initialized",
                                                       1);
    return result;
}

// result is: <x|op12|f>_particle
real_function_3d
CCPairFunction::dirac_convolution(const CCFunction& x, const CCConvolutionOperator& op, const size_t particle) const {
    real_function_3d result;
    switch (type) {
        default: MADNESS_EXCEPTION("Undefined enum", 1);
        case PT_FULL:
            result = op(x, u, particle);
            break;
        case PT_DECOMPOSED :
            result = dirac_convolution_decomposed(x, op, particle);
            break;
        case PT_OP_DECOMPOSED: MADNESS_EXCEPTION("op_decomposed dirac convolution not yet implemented", 1);
    }
    return result;
}

CCPairFunction CCPairFunction::swap_particles() const {
    switch (type) {
        default: MADNESS_EXCEPTION("Undefined enum", 1);
        case PT_FULL:
            return swap_particles_pure();
            break;
        case PT_DECOMPOSED:
            return swap_particles_decomposed();
            break;
        case PT_OP_DECOMPOSED:
            return swap_particles_op_decomposed();
            break;
    }
    MADNESS_EXCEPTION("swap_particles in CCPairFunction: we should not end up here", 1);
}

real_function_3d CCPairFunction::project_out_decomposed(const real_function_3d& f, const size_t particle) const {
    real_function_3d result = real_factory_3d(world);
    const std::pair<vector_real_function_3d, vector_real_function_3d> decompf = assign_particles(particle);
    Tensor<double> c = inner(world, f, decompf.first);
    for (size_t i = 0; i < a.size(); i++) result += c(i) * decompf.second[i];
    return result;
}

real_function_3d CCPairFunction::project_out_op_decomposed(const CCFunction& f, const size_t particle) const {
    if (particle == 1) {
        return (*op)(f, x) * y.function;
    } else if (particle == 2) {
        return (*op)(f, y) * x.function;
    } else {
        MADNESS_EXCEPTION("project_out_op_decomposed: particle must be 1 or 2", 1);
        return real_factory_3d(world);
    }
}

real_function_3d CCPairFunction::dirac_convolution_decomposed(const CCFunction& bra, const CCConvolutionOperator& op,
                                                              const size_t particle) const {
    const std::pair<vector_real_function_3d, vector_real_function_3d> f = assign_particles(particle);
    const vector_real_function_3d braa = mul(world, bra.function, f.first);
    const vector_real_function_3d braga = op(braa);
    real_function_3d result = real_factory_3d(world);
    for (size_t i = 0; i < braga.size(); i++) result += braga[i] * f.second[i];
    return result;
}


const std::pair<vector_real_function_3d, vector_real_function_3d>
CCPairFunction::assign_particles(const size_t particle) const {
    if (particle == 1) {
        return std::make_pair(a, b);
    } else if (particle == 2) {
        return std::make_pair(b, a);
    } else {
        MADNESS_EXCEPTION("project_out_decomposed: Particle is neither 1 nor 2", 1);
        return std::make_pair(a, b);
    }
}

CCPairFunction CCPairFunction::swap_particles_pure() const {
    // CC_Timer timer_swap(world,"swap particles");
    // this could be done more efficiently for SVD, but it works decently
    std::vector<long> map(6);
    map[0] = 3;
    map[1] = 4;
    map[2] = 5;     // 2 -> 1
    map[3] = 0;
    map[4] = 1;
    map[5] = 2;     // 1 -> 2
    // timer_swap.info();
    real_function_6d swapped_u = mapdim(u, map);
    return CCPairFunction(world, swapped_u);
}

CCPairFunction CCPairFunction::swap_particles_decomposed() const {
    return CCPairFunction(world, b, a);
}

CCPairFunction CCPairFunction::swap_particles_op_decomposed() const {
    return CCPairFunction(world, op, y, x);
}


void CCConvolutionOperator::update_elements(const CC_vecfunction& bra, const CC_vecfunction& ket) {
    const std::string operation_name = "<" + assign_name(bra.type) + "|" + name() + "|" + assign_name(ket.type) + ">";
    if (world.rank() == 0)
        std::cout << "updating operator elements: " << operation_name << " (" << bra.size() << "x" << ket.size() << ")"
                  << std::endl;
    if (bra.type != HOLE)
        error("Can not create intermediate of type " + operation_name + " , bra-element has to be of type HOLE");
    op.reset(init_op(operator_type, parameters));
    intermediateT xim;
    for (auto tmpk : bra.functions) {
        const CCFunction& k = tmpk.second;
        for (auto tmpl : ket.functions) {
            const CCFunction& l = tmpl.second;
            real_function_3d kl = (bra(k).function * l.function);
            real_function_3d result = ((*op)(kl)).truncate();
            result.reconstruct(); // for sparse multiplication
            xim.insert(k.i, l.i, result);
        }
    }
    if (ket.type == HOLE) imH = xim;
    else if (ket.type == PARTICLE) imP = xim;
    else if (ket.type == RESPONSE) imR = xim;
    else error("Can not create intermediate of type <" + assign_name(bra.type) + "|op|" + assign_name(ket.type) + ">");
}


void CCConvolutionOperator::clear_intermediates(const FuncType& type) {
    if (world.rank() == 0)
        std::cout << "Deleting all <HOLE|" << name() << "|" << assign_name(type) << "> intermediates \n";
    switch (type) {
        case HOLE : {
            imH.allpairs.clear();
            break;
        }
        case PARTICLE: {
            imP.allpairs.clear();
            break;
        }
        case RESPONSE: {
            imR.allpairs.clear();
            break;
        }
        default:
            error("intermediates for " + assign_name(type) + " are not defined");
    }
}

size_t CCConvolutionOperator::info() const {
    const size_t size_imH = size_of(imH);
    const size_t size_imP = size_of(imP);
    const size_t size_imR = size_of(imR);
    if (world.rank() == 0) {
        std::cout << "Size of " << name() << " intermediates:\n";
        std::cout << std::setw(5) << "(" << imH.allpairs.size() << ") x <H|" + name() + "H>=" << std::scientific
                  << std::setprecision(1) << size_imH << " (Gbyte)\n";
        std::cout << std::setw(5) << "(" << imP.allpairs.size() << ") x <H|" + name() + "P>=" << std::scientific
                  << std::setprecision(1) << size_imH << " (Gbyte)\n";
        std::cout << std::setw(5) << "(" << imR.allpairs.size() << ") x <H|" + name() + "R>=" << std::scientific
                  << std::setprecision(1) << size_imH << " (Gbyte)\n";
    }
    return size_imH + size_imP + size_imR;
}

SeparatedConvolution<double, 3> *
CCConvolutionOperator::init_op(const OpType& type, const Parameters& parameters) const {
    switch (type) {
        case OT_G12 : {
            if (world.rank() == 0)
                std::cout << "Creating " << assign_name(type) << " Operator with thresh=" << parameters.thresh_op
                          << " and lo=" << parameters.lo << std::endl;
            return CoulombOperatorPtr(world, parameters.lo, parameters.thresh_op);
        }
        case OT_F12 : {
            if (world.rank() == 0)
                std::cout << "Creating " << assign_name(type) << " Operator with thresh=" << parameters.thresh_op
                          << " and lo=" << parameters.lo << " and Gamma=" << parameters.gamma << std::endl;
            return SlaterF12OperatorPtr(world, parameters.gamma, parameters.lo, parameters.thresh_op);
        }
        default : {
            error("Unknown operatorype " + assign_name(type));
            MADNESS_EXCEPTION("error", 1);
        }
    }

}

/// Assigns strings to enums for formated output
std::string
assign_name(const PairFormat& input) {
    switch (input) {
        case PT_FULL:
            return "full";
        case PT_DECOMPOSED:
            return "decomposed";
        case PT_OP_DECOMPOSED:
            return "operator-decomposed";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    MADNESS_EXCEPTION("assign_name:pairtype, should not end up here", 1);
    return "unknown pairtype";
}

/// Assigns strings to enums for formated output
std::string
assign_name(const CCState& input) {
    switch (input) {
        case GROUND_STATE:
            return "Ground State";
        case EXCITED_STATE:
            return "Excited State";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    MADNESS_EXCEPTION("assign_name:pairtype, should not end up here", 1);
    return "unknown pairtype";
}

/// Assigns strings to enums for formated output
std::string
assign_name(const OpType& input) {
    switch (input) {
        case OT_G12:
            return "g12";
        case OT_F12:
            return "f12";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    MADNESS_EXCEPTION("assign_name:optype, should not end up here", 1);
    return "unknown operatortype";
}

/// Assigns enum to string
CalcType
assign_calctype(const std::string name) {
    if (name == "mp2") return CT_MP2;
    else if (name == "cc2") return CT_CC2;
    else if (name == "lrcc2" or name == "cc2_response") return CT_LRCC2;
    else if (name == "cispd") return CT_CISPD;
    else if (name == "cis" or name == "ccs" or name == "ccs_response" or name == "lrccs") return CT_LRCCS;
    else if (name == "experimental") return CT_TEST;
    else if (name == "adc2" or name == "adc(2)") return CT_ADC2;
    else if (name == "tdhf") return CT_TDHF;
    else {
        std::string msg = "CALCULATION OF TYPE: " + name + " IS NOT KNOWN!!!!";
        MADNESS_EXCEPTION(msg.c_str(), 1);
    }
}

/// Assigns strings to enums for formated output
std::string
assign_name(const CalcType& inp) {
    switch (inp) {
        case CT_CC2:
            return "CC2";
        case CT_MP2:
            return "MP2";
        case CT_LRCC2:
            return "LRCC2";
        case CT_CISPD:
            return "CISpD";
        case CT_LRCCS:
            return "LRCCS";
        case CT_ADC2:
            return "ADC2";
        case CT_TDHF:
            return "TDHF";
        case CT_TEST:
            return "experimental";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    return "unknown";
}

/// Assigns strings to enums for formated output
std::string
assign_name(const PotentialType& inp) {
    switch (inp) {
        case POT_F3D_:
            return "F3D";
        case POT_s3a_:
            return "s3a";
        case POT_s3b_:
            return "s3b";
        case POT_s3c_:
            return "s3c";
        case POT_s5a_:
            return "s5a";
        case POT_s5b_:
            return "s5b";
        case POT_s5c_:
            return "s5c";
        case POT_s6_:
            return "s6";
        case POT_s2b_:
            return "s2b";
        case POT_s2c_:
            return "s2c";
        case POT_s4a_:
            return "s4a";
        case POT_s4b_:
            return "s4b";
        case POT_s4c_:
            return "s4c";
        case POT_ccs_:
            return "ccs";
        case POT_cis_:
            return "cis-potential";
        case POT_singles_:
            return "singles potential";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    return "undefined";
}

/// Assigns strings to enums for formated output
std::string
assign_name(const FuncType& inp) {
    switch (inp) {
        case HOLE:
            return "Hole";
        case PARTICLE:
            return "Particle";
        case MIXED:
            return "Mixed";
        case RESPONSE:
            return "Response";
        case UNDEFINED:
            return "Undefined";
        default: {
            MADNESS_EXCEPTION("Unvalid enum assignement!", 1);
            return "undefined";
        }
    }
    return "???";
}

/// Returns the size of an intermediate
double
size_of(const intermediateT& im) {
    double size = 0.0;
    for (const auto& tmp : im.allpairs) {
        size += get_size<double, 3>(tmp.second);
    }
    return size;
}

}// end namespace madness



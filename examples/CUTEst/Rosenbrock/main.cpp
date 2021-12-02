/** 
 * @example CUTEst/Rosenbrock/main.cpp
 *
 * This example shows how to load and solve CUTEst problems using alpaqa.
 */

#include <alpaqa/decl/alm.hpp>
#include <alpaqa/inner/decl/panoc.hpp>
#include <alpaqa/inner/directions/decl/lbfgs.hpp>

#include <alpaqa/interop/cutest/CUTEstLoader.hpp>

#include <iostream>

int main() {
    using alpaqa::vec;

    // Paths to the files generated by CUTEst
    const char *so_fname      = "CUTEst/ROSENBR/libcutest-problem-ROSENBR.so";
    const char *outsdif_fname = "CUTEst/ROSENBR/OUTSDIF.d";

    // Load the problem
    alpaqa::ProblemWithCounters p =
        alpaqa::CUTEstProblem{so_fname, outsdif_fname};

    // Settings for the outer augmented Lagrangian method
    alpaqa::ALMParams almparam;
    almparam.ε              = 1e-8; // tolerance
    almparam.δ              = 1e-8;
    almparam.Δ              = 10;
    almparam.max_iter       = 20;
    almparam.print_interval = 1;

    // Settings for the inner PANOC solver
    alpaqa::PANOCParams panocparam;
    panocparam.max_iter       = 500;
    panocparam.print_interval = 1;
    // Settings for the L-BFGS algorithm used by PANOC
    alpaqa::LBFGSParams lbfgsparam;
    lbfgsparam.memory = 10;

    // Create an ALM solver using PANOC as inner solver
    alpaqa::ALMSolver<alpaqa::PANOCSolver<>> solver{
        almparam,                 // params for outer solver
        {panocparam, lbfgsparam}, // inner solver
    };

    // Initial guess
    vec x = p.get_x0();
    vec y = p.get_y0();

    // Solve the problem
    auto stats = solver(p, y, x);

    // Print the results
    std::cout << "status: " << stats.status << std::endl;
    std::cout << "x = " << x.transpose() << std::endl;
    std::cout << "y = " << y.transpose() << std::endl;
    vec g(p.m);
    p.eval_g(x, g);
    std::cout << "g = " << g.transpose() << std::endl;
    std::cout << "f = " << p.eval_f(x) << std::endl;
    std::cout << "inner: " << stats.inner.iterations << std::endl;
    std::cout << "outer: " << stats.outer_iterations << std::endl;

    std::cout << p.evaluations << std::endl;
}
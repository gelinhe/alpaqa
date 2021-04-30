#pragma once

#include <panoc-alm/util/problem.hpp>

#include <memory>

/// @addtogroup grp_ExternalProblemLoaders
/// @{

/// Load an objective function generated by CasADi.
std::function<pa::Problem::f_sig>
load_CasADi_objective(const char *so_name, const char *fun_name = "f");
/// Load the gradient of an objective function generated by CasADi.
std::function<pa::Problem::grad_f_sig>
load_CasADi_gradient_objective(const char *so_name,
                               const char *fun_name = "grad_f");
/// Load a constraint function generated by CasADi.
std::function<pa::Problem::g_sig>
load_CasADi_constraints(const char *so_name, const char *fun_name = "g");
/// Load the gradient-vector product of a constraint function generated by
/// CasADi.
std::function<pa::Problem::grad_g_prod_sig>
load_CasADi_gradient_constraints_prod(const char *so_name,
                                      const char *fun_name = "grad_g");
/// Load the Hessian of a Lagrangian function generated by CasADi.
std::function<pa::Problem::hess_L_sig>
load_CasADi_hessian_lagrangian(const char *so_name,
                               const char *fun_name = "hess_L");
/// Load the Hessian-vector product of a Lagrangian function generated by
/// CasADi.
std::function<pa::Problem::hess_L_prod_sig>
load_CasADi_hessian_lagrangian_prod(const char *so_name,
                                    const char *fun_name = "hess_L_prod");

class ProblemWithParam : public pa::Problem {
  public:
    using pa::Problem::Problem;
    void set_param(pa::crvec p) { *param = p; }
    void set_param(pa::vec &&p) { *param = std::move(p); }
    pa::vec &get_param() { return *param; }
    const pa::vec &get_param() const { return *param; }
    std::shared_ptr<pa::vec> get_param_ptr() const { return param; }

  private:
    std::shared_ptr<pa::vec> param = std::make_shared<pa::vec>();
};

pa::Problem load_CasADi_problem(const char *filename, unsigned n, unsigned m);
ProblemWithParam load_CasADi_problem_with_param(const char *filename,
                                                unsigned n, unsigned m);

/// @}

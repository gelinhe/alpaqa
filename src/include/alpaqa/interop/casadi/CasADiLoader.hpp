#pragma once

#include <alpaqa/util/problem.hpp>

#include <memory>
#include <string>

namespace alpaqa {

/// @addtogroup grp_ExternalProblemLoaders
/// @{

struct CasADiProblem : ProblemWithParam {
    /// Load a problem generated by CasADi (with parameters).
    ///
    /// @param  filename
    ///         Filename of the shared library to load the functions from.
    /// @param  n
    ///         Number of decision variables (@f$ x \in \mathbb{R}^n @f$).
    /// @param  m
    ///         Number of general constraints (@f$ g(x) \in \mathbb{R}^m @f$).
    /// @param  p
    ///         The number of parameters of the problem (second argument to all
    ///         CasADi functions).
    /// @param  second_order
    ///         Load the additional functions required for second-order PANOC.
    ///
    /// The file should contain functions with the names `f`, `grad_f`, `g` and
    /// `grad_g`. These functions evaluate the objective function, its gradient,
    /// the constraints, and the constraint gradient times a vector respecitvely.
    /// If @p second_order is true, additional functions `hess_L` and
    /// `hess_L_prod` should be provided to evaluate the Hessian of the
    /// Lagrangian and Hessian-vector products.
    ///
    /// If any of the dimensions are zero, they are determined from the `g`
    /// function in the given file.
    ///
    /// @throws std::invalid_argument
    ///         The dimensions of the loaded functions do not match.
    CasADiProblem(const std::string &filename, unsigned n = 0, unsigned m = 0,
                  unsigned p = 0, bool second_order = false);
    ~CasADiProblem();

    CasADiProblem(const CasADiProblem &);
    CasADiProblem &operator=(const CasADiProblem &);
    CasADiProblem(CasADiProblem &&);
    CasADiProblem &operator=(CasADiProblem &&);

    real_t eval_f(crvec x) const override;
    void eval_g(crvec x, rvec g) const override;
    void eval_grad_ψ(crvec x, crvec y, crvec Σ, rvec grad_ψ, rvec work_n,
                     rvec work_m) const override;
    real_t eval_ψ_grad_ψ(crvec x, crvec y, crvec Σ, rvec grad_ψ, rvec work_n,
                         rvec work_m) const override;
    void eval_grad_L(crvec x, crvec y, rvec grad_L, rvec work_n) const override;
    real_t eval_ψ_ŷ(crvec x, crvec y, crvec Σ, rvec ŷ) const override;
    void eval_grad_ψ_from_ŷ(crvec x, crvec ŷ, rvec grad_ψ,
                            rvec work_n) const override;

    std::unique_ptr<Problem> clone() const & override {
        return std::unique_ptr<CasADiProblem>(new CasADiProblem(*this));
    }
    std::unique_ptr<Problem> clone() && override {
        return std::unique_ptr<CasADiProblem>(
            new CasADiProblem(std::move(*this)));
    }

  private:
    std::unique_ptr<struct CasADiFunctionsWithParam> impl;
};

/// @}

} // namespace alpaqa
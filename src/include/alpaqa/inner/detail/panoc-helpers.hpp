#pragma once

#include <alpaqa/inner/decl/panoc-stop-crit.hpp>
#include <alpaqa/util/atomic_stop_signal.hpp>
#include <alpaqa/util/problem.hpp>
#include <alpaqa/util/solverstatus.hpp>

#include <stdexcept>

namespace alpaqa::detail {

/// Calculate both ψ(x) and the vector ŷ that can later be used to compute ∇ψ.
/// @f[ \psi(x^k) = f(x^k) + \frac{1}{2}
/// \text{dist}_\Sigma^2\left(g(x^k) + \Sigma^{-1}y,\;D\right) @f]
/// @f[ \hat{y}  @f]
inline real_t calc_ψ_ŷ(const Problem &p, ///< [in]  Problem description
                       crvec x,          ///< [in]  Decision variable @f$ x @f$
                       crvec y, ///< [in]  Lagrange multipliers @f$ y @f$
                       crvec Σ, ///< [in]  Penalty weights @f$ \Sigma @f$
                       rvec ŷ   ///< [out] @f$ \hat{y} @f$
) {
    return p.eval_ψ_ŷ(x, y, Σ, ŷ);
}

/// Calculate ∇ψ(x) using ŷ.
inline void calc_grad_ψ_from_ŷ(const Problem &p, ///< [in]  Problem description
                               crvec x, ///< [in]  Decision variable @f$ x @f$
                               crvec ŷ, ///< [in]  @f$ \hat{y} @f$
                               rvec grad_ψ, ///< [out] @f$ \nabla \psi(x) @f$
                               rvec work_n  ///<       Dimension n
) {
    return p.eval_grad_ψ_from_ŷ(x, ŷ, grad_ψ, work_n);
}

/// Calculate both ψ(x) and its gradient ∇ψ(x).
/// @f[ \psi(x^k) = f(x^k) + \frac{1}{2}
/// \text{dist}_\Sigma^2\left(g(x^k) + \Sigma^{-1}y,\;D\right) @f]
/// @f[ \nabla \psi(x) = \nabla f(x) + \nabla g(x)\ \hat{y}(x) @f]
inline real_t calc_ψ_grad_ψ(const Problem &p, ///< [in]  Problem description
                            crvec x, ///< [in]  Decision variable @f$ x @f$
                            crvec y, ///< [in]  Lagrange multipliers @f$ y @f$
                            crvec Σ, ///< [in]  Penalty weights @f$ \Sigma @f$
                            rvec grad_ψ, ///< [out] @f$ \nabla \psi(x) @f$
                            rvec work_n, ///<       Dimension n
                            rvec work_m  ///<       Dimension m
) {
    return p.eval_ψ_grad_ψ(x, y, Σ, grad_ψ, work_n, work_m);
}

/// Calculate the gradient ∇ψ(x).
/// @f[ \nabla \psi(x) = \nabla f(x) + \nabla g(x)\ \hat{y}(x) @f]
inline void calc_grad_ψ(const Problem &p, ///< [in]  Problem description
                        crvec x,          ///< [in]  Decision variable @f$ x @f$
                        crvec y,     ///< [in]  Lagrange multipliers @f$ y @f$
                        crvec Σ,     ///< [in]  Penalty weights @f$ \Sigma @f$
                        rvec grad_ψ, ///< [out] @f$ \nabla \psi(x) @f$
                        rvec work_n, ///<       Dimension n
                        rvec work_m  ///<       Dimension m
) {
    return p.eval_grad_ψ(x, y, Σ, grad_ψ, work_n, work_m);
}

/// Calculate the error between ẑ and g(x).
/// @f[ \hat{z}^k = \Pi_D\left(g(x^k) + \Sigma^{-1}y\right) @f]
inline void calc_err_z(const Problem &p, ///< [in]  Problem description
                       crvec x̂,   ///< [in]  Decision variable @f$ \hat{x} @f$
                       crvec y,   ///< [in]  Lagrange multipliers @f$ y @f$
                       crvec Σ,   ///< [in]  Penalty weights @f$ \Sigma @f$
                       rvec err_z ///< [out] @f$ g(\hat{x}) - \hat{z} @f$
) {
    // g(x̂)
    p.eval_g(x̂, err_z);
    // ζ = g(x̂) + Σ⁻¹y
    // ẑ = Π(ζ, D)
    // g(x) - ẑ
    err_z = err_z - project(err_z + Σ.asDiagonal().inverse() * y, p.D);
    // TODO: catastrophic cancellation?
}

/**
 * Projected gradient step
 * @f[ \begin{aligned} 
 * \hat{x}^k &= T_{\gamma^k}\left(x^k\right) \\ 
 * &= \Pi_C\left(x^k - \gamma^k \nabla \psi(x^k)\right) \\ 
 * p^k &= \hat{x}^k - x^k \\ 
 * \end{aligned} @f]
 */
inline auto
projected_gradient_step(const Box &C, ///< [in]  Set to project onto
                        real_t γ,     ///< [in]  Step size
                        crvec x,      ///< [in]  Decision variable @f$ x @f$
                        crvec grad_ψ  ///< [in]  @f$ \nabla \psi(x^k) @f$
) {
    using binary_real_f = real_t (*)(real_t, real_t);
    return (-γ * grad_ψ)
        .binaryExpr(C.lowerbound - x, binary_real_f(std::fmax))
        .binaryExpr(C.upperbound - x, binary_real_f(std::fmin));
}

inline void calc_x̂(const Problem &prob, ///< [in]  Problem description
                   real_t γ,            ///< [in]  Step size
                   crvec x,             ///< [in]  Decision variable @f$ x @f$
                   crvec grad_ψ,        ///< [in]  @f$ \nabla \psi(x^k) @f$
                   rvec x̂, ///< [out] @f$ \hat{x}^k = T_{\gamma^k}(x^k) @f$
                   rvec p  ///< [out] @f$ \hat{x}^k - x^k @f$
) {
    p = projected_gradient_step(prob.C, γ, x, grad_ψ);
    x̂ = x + p;
}

inline bool stop_crit_requires_grad_̂ψₖ(PANOCStopCrit crit) {
    switch (crit) {
        case PANOCStopCrit::ApproxKKT: [[fallthrough]];
        case PANOCStopCrit::ApproxKKT2: return true;
        case PANOCStopCrit::ProjGradNorm: [[fallthrough]];
        case PANOCStopCrit::ProjGradNorm2: [[fallthrough]];
        case PANOCStopCrit::ProjGradUnitNorm: [[fallthrough]];
        case PANOCStopCrit::ProjGradUnitNorm2: [[fallthrough]];
        case PANOCStopCrit::FPRNorm: [[fallthrough]];
        case PANOCStopCrit::FPRNorm2: return false;
        case PANOCStopCrit::Ipopt: return true;
    }
    throw std::out_of_range("Invalid PANOCStopCrit");
}

/// Compute the ε from the stopping criterion, see @ref PANOCStopCrit.
inline real_t calc_error_stop_crit(
    const Box &C,       ///< [in]  Box constraints on x
    PANOCStopCrit crit, ///< [in]  What stoppint criterion to use
    crvec pₖ,      ///< [in]  Projected gradient step @f$ \hat x^k - x^k @f$
    real_t γ,      ///< [in]  Step size
    crvec xₖ,      ///< [in]  Current iterate
    crvec x̂ₖ,      ///< [in]  Current iterate after projected gradient step
    crvec ŷₖ,      ///< [in]  Candidate Lagrange multipliers
    crvec grad_ψₖ, ///< [in]  Gradient in @f$ x^k @f$
    crvec grad_̂ψₖ  ///< [in]  Gradient in @f$ \hat x^k @f$
) {
    switch (crit) {
        case PANOCStopCrit::ApproxKKT: {
            auto err = (1 / γ) * pₖ + (grad_ψₖ - grad_̂ψₖ);
            // These parentheses     ^^^               ^^^     are important to
            // prevent catastrophic cancellation when the step is small
            return vec_util::norm_inf(err);
        }
        case PANOCStopCrit::ApproxKKT2: {
            auto err = (1 / γ) * pₖ + (grad_ψₖ - grad_̂ψₖ);
            // These parentheses     ^^^               ^^^     are important to
            // prevent catastrophic cancellation when the step is small
            return err.norm();
        }
        case PANOCStopCrit::ProjGradNorm: {
            return vec_util::norm_inf(pₖ);
        }
        case PANOCStopCrit::ProjGradNorm2: {
            return pₖ.norm();
        }
        case PANOCStopCrit::ProjGradUnitNorm: {
            return vec_util::norm_inf(
                projected_gradient_step(C, 1, xₖ, grad_ψₖ));
        }
        case PANOCStopCrit::ProjGradUnitNorm2: {
            return projected_gradient_step(C, 1, xₖ, grad_ψₖ).norm();
        }
        case PANOCStopCrit::FPRNorm: {
            return vec_util::norm_inf(pₖ) / γ;
        }
        case PANOCStopCrit::FPRNorm2: {
            return pₖ.norm() / γ;
        }
        case PANOCStopCrit::Ipopt: {
            auto err =
                vec_util::norm_inf(projected_gradient_step(C, 1, x̂ₖ, grad_̂ψₖ));
            auto n = 2 * (ŷₖ.size() + x̂ₖ.size());
            if (n == 0)
                return err;
            auto C_lagr_mult =
                vec_util::norm_1(projecting_difference(x̂ₖ - grad_̂ψₖ, C));
            auto D_lagr_mult   = vec_util::norm_1(ŷₖ);
            const real_t s_max = 100;
            real_t s_d =
                std::max(s_max, (C_lagr_mult + D_lagr_mult) / n) / s_max;
            return err / s_d;
        }
    }
    throw std::out_of_range("Invalid PANOCStopCrit");
}

/// Increase the estimate of the Lipschitz constant of the objective gradient
/// and decrease the step size until quadratic upper bound or descent lemma is
/// satisfied:
/// @f[ \psi(x + p) \le \psi(x) + \nabla\psi(x)^\top p + \frac{L}{2} \|p\|^2 @f]
/// The projected gradient iterate @f$ \hat x^k @f$ and step @f$ p^k @f$ are
/// updated with the new step size @f$ \gamma^k @f$, and so are other quantities
/// that depend on them, such as @f$ \nabla\psi(x^k)^\top p^k @f$ and
/// @f$ \|p\|^2 @f$. The intermediate vector @f$ \hat y(x^k) @f$ can be used to
/// compute the gradient @f$ \nabla\psi(\hat x^k) @f$ or to update the Lagrange
/// multipliers.
///
/// @return The original step size, before it was reduced by this function.
inline real_t descent_lemma(
    /// [in]  Problem description
    const Problem &problem,
    /// [in]    Tolerance used to ignore rounding errors when the function
    ///         @f$ \psi(x) @f$ is relatively flat or the step size is very
    ///         small, which could cause @f$ \psi(x^k) < \psi(\hat x^k) @f$,
    ///         which is mathematically impossible but could occur in finite
    ///         precision floating point arithmetic.
    real_t rounding_tolerance,
    /// [in]    Maximum allowed Lipschitz constant estimate (prevents infinite
    ///         loop if function or derivatives are discontinuous, and keeps
    ///         step size bounded away from zero).
    real_t L_max,
    /// [in]    Current iterate @f$ x^k @f$
    crvec xₖ,
    /// [in]    Objective function @f$ \psi(x^k) @f$
    real_t ψₖ,
    /// [in]    Gradient of objective @f$ \nabla\psi(x^k) @f$
    crvec grad_ψₖ,
    /// [in]    Lagrange multipliers @f$ y @f$
    crvec y,
    /// [in]    Penalty weights @f$ \Sigma @f$
    crvec Σ,
    /// [out]   Projected gradient iterate @f$ \hat x^k @f$
    rvec x̂ₖ,
    /// [out]   Projected gradient step @f$ p^k @f$
    rvec pₖ,
    /// [out]   Intermediate vector @f$ \hat y(x^k) @f$
    rvec ŷx̂ₖ,
    /// [inout] Objective function @f$ \psi(\hat x^k) @f$
    real_t &ψx̂ₖ,
    /// [inout] Squared norm of the step @f$ \left\| p^k \right\|^2 @f$
    real_t &norm_sq_pₖ,
    /// [inout] Gradient of objective times step @f$ \nabla\psi(x^k)^\top p^k@f$
    real_t &grad_ψₖᵀpₖ,
    /// [inout] Lipschitz constant estimate @f$ L_{\nabla\psi}^k @f$
    real_t &Lₖ,
    /// [inout] Step size @f$ \gamma^k @f$
    real_t &γₖ) {

    real_t old_γₖ = γₖ;
    real_t margin = (1 + std::abs(ψₖ)) * rounding_tolerance;
    while (ψx̂ₖ - ψₖ > grad_ψₖᵀpₖ + 0.5 * Lₖ * norm_sq_pₖ + margin) {
        if (not(Lₖ * 2 <= L_max))
            break;

        Lₖ *= 2;
        γₖ /= 2;

        // Calculate x̂ₖ and pₖ (with new step size)
        calc_x̂(problem, γₖ, xₖ, grad_ψₖ, /* in ⟹ out */ x̂ₖ, pₖ);
        // Calculate ∇ψ(xₖ)ᵀpₖ and ‖pₖ‖²
        grad_ψₖᵀpₖ = grad_ψₖ.dot(pₖ);
        norm_sq_pₖ = pₖ.squaredNorm();

        // Calculate ψ(x̂ₖ) and ŷ(x̂ₖ)
        ψx̂ₖ = problem.eval_ψ_ŷ(x̂ₖ, y, Σ, /* in ⟹ out */ ŷx̂ₖ);
    }
    return old_γₖ;
}

/// Check all stop conditions (required tolerance reached, out of time,
/// maximum number of iterations exceeded, interrupted by user,
/// infinite iterate, no progress made)
template <class ParamsT, class DurationT>
inline SolverStatus check_all_stop_conditions(
    /// [in]    Parameters including `max_iter`, `max_time` and `max_no_progress`
    const ParamsT &params,
    /// [in]    Time elapsed since the start of the algorithm
    DurationT time_elapsed,
    /// [in]    The current iteration number
    unsigned iteration,
    /// [in]    A stop signal for the user to interrupt the algorithm
    const AtomicStopSignal &stop_signal,
    /// [in]    Desired primal tolerance
    real_t ε,
    /// [in]    Tolerance of the current iterate
    real_t εₖ,
    /// [in]    The number of successive iterations no progress was made
    unsigned no_progress) {

    bool out_of_time     = time_elapsed > params.max_time;
    bool out_of_iter     = iteration == params.max_iter;
    bool interrupted     = stop_signal.stop_requested();
    bool not_finite      = not std::isfinite(εₖ);
    bool conv            = εₖ <= ε;
    bool max_no_progress = no_progress > params.max_no_progress;
    return conv              ? SolverStatus::Converged
           : out_of_time     ? SolverStatus::MaxTime
           : out_of_iter     ? SolverStatus::MaxIter
           : not_finite      ? SolverStatus::NotFinite
           : max_no_progress ? SolverStatus::NoProgress
           : interrupted     ? SolverStatus::Interrupted
                             : SolverStatus::Unknown;
}

/// Compute the Hessian matrix of the augmented Lagrangian function
/// @f[ \nabla^2_{xx} L_\Sigma(x, y) =
///     \Big. \nabla_{xx}^2 L(x, y) \Big|_{\big(x,\, \hat y(x, y)\big)}
///   + \sum_{i\in\mathcal{I}} \Sigma_i\,\nabla g_i(x) \nabla g_i(x)^\top @f]
inline void calc_augmented_lagrangian_hessian(
    /// [in]  Problem description
    const Problem &problem,
    /// [in]    Current iterate @f$ x^k @f$
    crvec xₖ,
    /// [in]   Intermediate vector @f$ \hat y(x^k) @f$
    crvec ŷxₖ,
    /// [in]    Lagrange multipliers @f$ y @f$
    crvec y,
    /// [in]    Penalty weights @f$ \Sigma @f$
    crvec Σ,
    /// [out]   The constraint values @f$ g(x^k) @f$
    rvec g,
    /// [out]   Hessian matrix @f$ H(x, y) @f$
    mat &H,
    ///         Dimension n
    rvec work_n) {

    // Compute the Hessian of the Lagrangian
    problem.eval_hess_L(xₖ, ŷxₖ, H);
    // Compute the Hessian of the augmented Lagrangian
    problem.eval_g(xₖ, g);
    for (vec::Index i = 0; i < problem.m; ++i) {
        real_t ζ = g(i) + y(i) / Σ(i);
        bool inactive =
            problem.D.lowerbound(i) < ζ && ζ < problem.D.upperbound(i);
        if (not inactive) {
            problem.eval_grad_gi(xₖ, i, work_n);
            H += work_n * Σ(i) * work_n.transpose();
        }
    }
}

/// Compute the Hessian matrix of the augmented Lagrangian function multiplied
/// by the given vector, using finite differences.
/// @f[ \nabla^2_{xx} L_\Sigma(x, y)\, v \approx
///     \frac{\nabla_x L_\Sigma(x+hv, y) - \nabla_x L_\Sigma(x, y)}{h} @f]
inline void calc_augmented_lagrangian_hessian_prod_fd(
    /// [in]    Problem description
    const Problem &problem,
    /// [in]    Current iterate @f$ x^k @f$
    crvec xₖ,
    /// [in]    Lagrange multipliers @f$ y @f$
    crvec y,
    /// [in]    Penalty weights @f$ \Sigma @f$
    crvec Σ,
    /// [in]    Gradient @f$ \nabla \psi(x^k) @f$
    crvec grad_ψ,
    /// [in]    Vector to multiply with the Hessian
    crvec v,
    /// [out]   Hessian-vector product
    rvec Hv,
    ///         Dimension n
    rvec work_n1,
    ///         Dimension n
    rvec work_n2,
    ///         Dimension m
    rvec work_m) {

    real_t cbrt_ε = std::cbrt(std::numeric_limits<real_t>::epsilon());
    real_t h      = cbrt_ε * (1 + xₖ.norm());
    rvec xₖh      = work_n1;
    xₖh           = xₖ + h * v;
    calc_grad_ψ(problem, xₖh, y, Σ, Hv, work_n2, work_m);
    Hv -= grad_ψ;
    Hv /= h;
}

/// Estimate the Lipschitz constant of the gradient @f$ \nabla \psi @f$ using
/// finite differences.
inline real_t initial_lipschitz_estimate(
    /// [in]    Problem description
    const Problem &problem,
    /// [in]    Current iterate @f$ x^k @f$
    crvec xₖ,
    /// [in]    Lagrange multipliers @f$ y @f$
    crvec y,
    /// [in]    Penalty weights @f$ \Sigma @f$
    crvec Σ,
    /// [in]    Finite difference step size relative to xₖ
    real_t ε,
    /// [in]    Minimum absolute finite difference step size
    real_t δ,
    /// [in]    Minimum allowed Lipschitz estimate.
    real_t L_min,
    /// [in]    Maximum allowed Lipschitz estimate.
    real_t L_max,
    /// [out]   @f$ \psi(x^k) @f$
    real_t &ψ,
    /// [out]   Gradient @f$ \nabla \psi(x^k) @f$
    rvec grad_ψ,
    ///         Dimension n
    rvec work_n1,
    ///         Dimension n
    rvec work_n2,
    ///         Dimension n
    rvec work_n3,
    ///         Dimension m
    rvec work_m) {

    auto h        = (xₖ * ε).cwiseAbs().cwiseMax(δ);
    work_n1       = xₖ + h;
    real_t norm_h = h.norm();
    // Calculate ∇ψ(x₀ + h)
    calc_grad_ψ(problem, work_n1, y, Σ, /* in ⟹ out */ work_n2, work_n3,
                work_m);
    // Calculate ψ(xₖ), ∇ψ(x₀)
    ψ = calc_ψ_grad_ψ(problem, xₖ, y, Σ, /* in ⟹ out */ grad_ψ, work_n1,
                      work_m);

    // Estimate Lipschitz constant using finite differences
    real_t L = (work_n2 - grad_ψ).norm() / norm_h;
    return std::clamp(L, L_min, L_max);
}

/// Estimate the Lipschitz constant of the gradient @f$ \nabla \psi @f$ using
/// finite differences.
inline real_t initial_lipschitz_estimate(
    /// [in]    Problem description
    const Problem &problem,
    /// [in]    Current iterate @f$ x^k @f$
    crvec xₖ,
    /// [in]    Lagrange multipliers @f$ y @f$
    crvec y,
    /// [in]    Penalty weights @f$ \Sigma @f$
    crvec Σ,
    /// [in]    Finite difference step size relative to xₖ
    real_t ε,
    /// [in]    Minimum absolute finite difference step size
    real_t δ,
    /// [in]    Minimum allowed Lipschitz estimate.
    real_t L_min,
    /// [in]    Maximum allowed Lipschitz estimate.
    real_t L_max,
    /// [out]   Gradient @f$ \nabla \psi(x^k) @f$
    rvec grad_ψ,
    ///         Dimension n
    rvec work_n1,
    ///         Dimension n
    rvec work_n2,
    ///         Dimension n
    rvec work_n3,
    ///         Dimension m
    rvec work_m) {

    auto h        = (xₖ * ε).cwiseAbs().cwiseMax(δ);
    work_n1       = xₖ + h;
    real_t norm_h = h.norm();
    // Calculate ∇ψ(x₀ + h)
    calc_grad_ψ(problem, work_n1, y, Σ, /* in ⟹ out */ work_n2, work_n3,
                work_m);
    // Calculate ∇ψ(x₀)
    calc_grad_ψ(problem, xₖ, y, Σ, /* in ⟹ out */ grad_ψ, work_n1, work_m);

    // Estimate Lipschitz constant using finite differences
    real_t L = (work_n2 - grad_ψ).norm() / norm_h;
    return std::clamp(L, L_min, L_max);
}

} // namespace alpaqa::detail
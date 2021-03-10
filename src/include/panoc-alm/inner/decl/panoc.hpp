#pragma once

#include <panoc-alm/inner/decl/panoc-fwd.hpp>
#include <panoc-alm/util/atomic_stop_signal.hpp>
#include <panoc-alm/util/problem.hpp>
#include <panoc-alm/util/solverstatus.hpp>

#include <atomic>
#include <chrono>

namespace pa {

/// Tuning parameters for the PANOC algorithm.
struct PANOCParams {
    struct {
        /// Relative step size for initial finite difference Lipschitz estimate.
        real_t ε = 1e-6;
        /// Minimum step size for initial finite difference Lipschitz estimate.
        real_t δ = 1e-12;
        /// Factor that relates step size γ and Lipschitz constant.
        real_t Lγ_factor = 0.95;
    } Lipschitz; ///< Parameters related to the Lipschitz constant estimate
                 ///  and step size.
    /// Length of the history to keep in the L-BFGS algorithm.
    unsigned lbfgs_mem = 10; // TODO: move to LBFGS params
    /// Maximum number of inner PANOC iterations.
    unsigned max_iter = 100;
    /// Maximum duration.
    std::chrono::microseconds max_time = std::chrono::minutes(5);
    /// Minimum weight factor between Newton step and projected gradient step.
    real_t τ_min = 1e-12;

    unsigned print_interval = 0;

    bool update_lipschitz_in_linesearch = true;
    bool specialized_lbfgs              = true;
    bool alternative_linesearch_cond    = false;
};

template <class DirectionProviderT>
class PANOCSolver {
  public:
    using Params            = PANOCParams;
    using DirectionProvider = DirectionProviderT;

    struct Stats {
        unsigned iterations = 0;
        real_t ε            = inf;
        std::chrono::microseconds elapsed_time;
        SolverStatus status          = SolverStatus::Unknown;
        unsigned linesearch_failures = 0;
        unsigned lbfgs_failures      = 0; // TODO: more generic name
        unsigned lbfgs_rejected      = 0;
    };

    struct ProgressInfo {
        unsigned k;
        const vec &x;
        const vec &p;
        real_t norm_sq_p;
        const vec &x_hat;
        real_t ψ;
        const vec &grad_ψ;
        real_t ψ_hat;
        const vec &grad_ψ_hat;
        real_t γ;
        real_t ε;
        const vec &Σ;
        const vec &y;
        const Problem &problem;
        const Params &params;
    };

    PANOCSolver(Params params)
        : params(params), direction_provider({/* TODO: parameters */}) {}
    PANOCSolver(PANOCSolver &&) = default;
    PANOCSolver &operator=(PANOCSolver &&) = default;

    Stats operator()(const Problem &problem, // in
                     const vec &Σ,           // in
                     real_t ε,               // in
                     vec &x,                 // inout
                     vec &y,                 // inout
                     vec &err_z);            // out

    PANOCSolver &
    set_progress_callback(std::function<void(const ProgressInfo &)> cb) {
        this->progress_cb = cb;
        return *this;
    }

    void stop() { stop_signal.stop(); }

  private:
    Params params;
    AtomicStopSignal stop_signal;
    std::function<void(const ProgressInfo &)> progress_cb;

  public:
    DirectionProvider direction_provider;
};

} // namespace pa
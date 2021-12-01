#include <alpaqa/reference-problems/riskaverse-mpc.hpp>
#include <memory>

namespace alpaqa {
namespace problems {

struct RiskaverseProblem {
    unsigned nu = 2;
    unsigned nx = 4;
    unsigned ns = 2;
    unsigned ny = 3;
    unsigned n  = nu + ns + ny;
    unsigned m  = 3;
    real_t Ts   = 0.05;

    mat A;
    mat B;

    mat Q;
    mat R;

    vec x0;

    template <class VecX>
    auto u(VecX &&x) const {
        return x.segment(0, nu);
    }
    template <class VecX>
    auto s(VecX &&x) const {
        return x.segment(nu, ns);
    }
    template <class VecX>
    auto y(VecX &&x) const {
        return x.segment(nu + ns, ny);
    }

    Box get_C() const {
        Box C{vec(n), vec(n)};
        u(C.lowerbound).fill(-10);
        u(C.upperbound).fill(10);
        s(C.lowerbound).fill(-inf);
        s(C.upperbound).fill(inf);
        y(C.lowerbound).fill(0);
        y(C.upperbound).fill(inf);
        return C;
    }

    Box get_D() const {
        Box D{vec(m), vec(m)};
        D.lowerbound.fill(-inf);
        D.upperbound.fill(0);
        return D;
    }

    using Diag = Eigen::DiagonalMatrix<real_t, Eigen::Dynamic, Eigen::Dynamic>;

    RiskaverseProblem() {
        A       = mat::Identity(nx, nx);
        A(0, 2) = Ts;
        A(1, 3) = Ts;
        B       = mat::Zero(nx, nu);
        B(2, 0) = Ts;
        B(3, 1) = Ts;

        Q = Diag(nx);
        Q.diagonal().fill(10);
        R = Diag(nu);
        R.diagonal().fill(1);

        x0 = vec(nx);
        x0.fill(10);
    }

    auto mpc_dynamics(crvec x, crvec u) const { return A * x + B * u; };

    real_t f(crvec ux) const { return s(ux)(0); }
    void grad_f(crvec ux, rvec grad_f) const {
        (void)ux;
        grad_f.setZero();
        s(grad_f)(0) = 1;
    }
    void g(crvec ux, rvec g_u) const {
        g_u(0) = y(ux)(0) - y(ux)(1) - s(ux)(0);
        g_u(1) =
            mpc_dynamics(x0, u(ux)).dot(Q * mpc_dynamics(x0, u(ux))) - s(ux)(1);
        g_u(2) = x0.dot(Q * x0) + u(ux).dot(R * u(ux)) -
                 (y(ux)(0) - y(ux)(1) - y(ux)(2) - s(ux)(1));
    }
    void grad_g(crvec ux, crvec v, rvec grad_u_v) const {
        mat grad          = mat::Zero(n, m);
        s(grad.col(0))(0) = -1;
        y(grad.col(0))(0) = 1;
        y(grad.col(0))(1) = -1;

        u(grad.col(1))    = 2 * B.transpose() * (Q * (A * x0 + B * u(ux)));
        s(grad.col(1))(1) = -1;

        u(grad.col(2))    = 2 * (R * u(ux));
        s(grad.col(2))(1) = 1;
        y(grad.col(2))(0) = -1;
        y(grad.col(2))(1) = 1;
        y(grad.col(2))(2) = 1;

        grad_u_v = grad * v;
    }
};

LambdaProblem riskaverse_mpc_problem() {
    auto rptr = std::make_shared<RiskaverseProblem>();
    auto &r   = *rptr;
    LambdaProblem prob{
        r.n,
        r.m,
        r.get_C(),
        r.get_D(),
    };
    prob.f           = [rptr](crvec ux) { return rptr->f(ux); };
    prob.grad_f      = [rptr](crvec ux, rvec g_u) { rptr->grad_f(ux, g_u); };
    prob.g           = [rptr](crvec ux, rvec g_u) { rptr->g(ux, g_u); };
    prob.grad_g_prod = [rptr](crvec ux, crvec v, rvec grad_u_v) {
        rptr->grad_g(ux, v, grad_u_v);
    };
    return prob;
}

} // namespace problems
} // namespace alpaqa
/**
 * @file
 * This file defines all Python bindings.
 */

#include <alpaqa/decl/alm.hpp>
#include <alpaqa/inner/decl/panoc-stop-crit.hpp>
#include <alpaqa/inner/decl/panoc.hpp>
#include <alpaqa/inner/directions/lbfgs.hpp>
#include <alpaqa/inner/guarded-aa-pga.hpp>
#include <alpaqa/inner/panoc.hpp>
#include <alpaqa/inner/pga.hpp>
#include <alpaqa/inner/structured-panoc-lbfgs.hpp>
#include <alpaqa/standalone/panoc.hpp>
#include <alpaqa/util/solverstatus.hpp>

#if ALPAQA_HAVE_CASADI
#include <alpaqa/interop/casadi/CasADiLoader.hpp>
#endif

#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/chrono.h>
#include <pybind11/detail/common.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "kwargs-to-struct.hpp"
#include "polymorphic-inner-solver.hpp"
#include "polymorphic-panoc-direction.hpp"
#include "problem.hpp"

namespace py = pybind11;

template <class DirectionProviderT>
auto PolymorphicPANOCConstructor() {
    return [](const std::variant<alpaqa::PANOCParams, py::dict> &pp,
              const DirectionProviderT &dir) {
        using Base = alpaqa::PolymorphicPANOCDirectionBase;
        static_assert(std::is_base_of_v<Base, DirectionProviderT>);
        auto full_python_copy = std::make_shared<py::object>(py::cast(dir));
        auto base_copy        = full_python_copy->template cast<Base *>();
        return std::make_shared<alpaqa::PolymorphicPANOCSolver>(
            alpaqa::PANOCSolver<Base>{
                var_kwargs_to_struct(pp),
                std::shared_ptr<Base>(full_python_copy, base_copy),
            });
    };
}

template <class DirectionProviderT, class... DirectionArgumentsT>
auto PolymorphicPANOCConversion() {
    return [](const std::variant<alpaqa::PANOCParams, py::dict> &pp,
              const DirectionArgumentsT &...args) {
        using Base = alpaqa::PolymorphicPANOCDirectionBase;
        static_assert(std::is_base_of_v<Base, DirectionProviderT>);
        static_assert(std::is_constructible_v<DirectionProviderT,
                                              DirectionArgumentsT...>);
        DirectionProviderT dir{args...};
        return PolymorphicPANOCConstructor<DirectionProviderT>()(pp, dir);
    };
}

template <class DirectionProviderT>
auto PolymorphicPANOCDefaultConversion() {
    return [](const std::variant<alpaqa::PANOCParams, py::dict> &pp,
              const std::variant<alpaqa::LBFGSParams, py::dict> &args) {
        return PolymorphicPANOCConversion<DirectionProviderT,
                                          alpaqa::LBFGSParams>()(
            pp, var_kwargs_to_struct(args));
    };
}

template <class InnerSolverT>
auto PolymorphicALMConstructor() {
    return [](const std::variant<alpaqa::ALMParams, py::dict> &pp,
              const InnerSolverT &inner) {
        using Base = alpaqa::PolymorphicInnerSolverBase;
        static_assert(std::is_base_of_v<Base, InnerSolverT>);
        auto full_python_copy = std::make_shared<py::object>(py::cast(inner));
        auto base_copy        = full_python_copy->template cast<Base *>();
        return alpaqa::PolymorphicALMSolver{
            var_kwargs_to_struct<alpaqa::ALMParams>(pp),
            std::shared_ptr<Base>(full_python_copy, base_copy),
        };
    };
}

template <class InnerSolverT>
auto PolymorphicALMConstructorDefaultParams() {
    return [](const InnerSolverT &inner) {
        return PolymorphicALMConstructor<InnerSolverT>()(alpaqa::ALMParams(),
                                                         inner);
    };
}

template <class InnerSolverT, class... InnerSolverArgumentsT>
auto PolymorphicALMConversion() {
    return [](const std::variant<alpaqa::ALMParams, py::dict> &pp,
              const InnerSolverArgumentsT &...args) {
        using Base = alpaqa::PolymorphicPANOCDirectionBase;
        static_assert(std::is_base_of_v<Base, InnerSolverT>);
        static_assert(
            std::is_constructible_v<InnerSolverT, InnerSolverArgumentsT...>);
        InnerSolverT inner{args...};
        return PolymorphicALMConstructor<InnerSolverT>()(pp, inner);
    };
}

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

PYBIND11_MODULE(ALPAQA_MODULE_NAME, m) {
    using py::operator""_a;

    py::options options;
    options.enable_function_signatures();
    options.enable_user_defined_docstrings();

    m.doc() = "Alpaqa PANOC+ALM solvers"; // TODO

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif

    py::register_exception<alpaqa::not_implemented_error>(
        m, "not_implemented_error", PyExc_NotImplementedError);

    py::class_<alpaqa::Box>(m, "Box",
                            "C++ documentation: :cpp:class:`alpaqa::Box`")
        .def(py::init([](unsigned n) {
                 return alpaqa::Box{alpaqa::vec::Constant(n, alpaqa::inf),
                                    alpaqa::vec::Constant(n, -alpaqa::inf)};
             }),
             "n"_a,
             "Create an :math:`n`-dimensional box at with bounds at "
             ":math:`\\pm\\infty` (no constraints).")
        .def(py::init([](alpaqa::vec ub, alpaqa::vec lb) {
                 if (ub.size() != lb.size())
                     throw std::invalid_argument(
                         "Upper bound and lower bound dimensions do not "
                         "match");
                 return alpaqa::Box{std::move(ub), std::move(lb)};
             }),
             "ub"_a, "lb"_a, "Create a box with the given bounds.")
        .def_readwrite("upperbound", &alpaqa::Box::upperbound)
        .def_readwrite("lowerbound", &alpaqa::Box::lowerbound);

    py::class_<alpaqa::Problem, ProblemTrampoline<>>(
        m, "Problem", "C++ documentation: :cpp:class:`alpaqa::Problem`")
        // .def(py::init())
        .def(py::init<unsigned, unsigned>(), "n"_a, "m"_a,
             ":param n: Number of unknowns\n"
             ":param m: Number of constraints")
        .def("__copy__", [](const alpaqa::Problem &p) { return p.clone(); })
        .def("__deepcopy__",
             [](const alpaqa::Problem &p, py::dict) { return p.clone(); })
        .def_readwrite("n", &alpaqa::Problem::n,
                       "Number of unknowns, dimension of :math:`x`")
        .def_readwrite(
            "m", &alpaqa::Problem::m,
            "Number of general constraints, dimension of :math:`g(x)`")
        .def_readwrite("C", &alpaqa::Problem::C, "Box constraints on :math:`x`")
        .def_readwrite("D", &alpaqa::Problem::D,
                       "Box constraints on :math:`g(x)`")
        .def("eval_f", &alpaqa::Problem::eval_f)
        .def("eval_grad_f", &alpaqa::Problem::eval_grad_f)
        .def("eval_grad_f",
             [](const alpaqa::Problem &p, alpaqa::crvec x) {
                 alpaqa::vec g(p.n);
                 p.eval_grad_f(x, g);
                 return g;
             })
        .def("eval_g", &alpaqa::Problem::eval_g)
        .def("eval_g",
             [](const alpaqa::Problem &p, alpaqa::crvec x) {
                 alpaqa::vec g(p.m);
                 p.eval_g(x, g);
                 return g;
             })
        .def("eval_grad_g_prod", &alpaqa::Problem::eval_grad_g_prod)
        .def("eval_grad_g_prod",
             [](const alpaqa::Problem &p, alpaqa::crvec x, alpaqa::crvec y) {
                 alpaqa::vec g(p.n);
                 p.eval_grad_g_prod(x, y, g);
                 return g;
             })
        .def("eval_ψ_ŷ", &alpaqa::Problem::eval_ψ_ŷ)
        .def("eval_ψ_ŷ",
             [](const alpaqa::Problem &p, alpaqa::crvec x, alpaqa::crvec y,
                alpaqa::crvec Σ) {
                 alpaqa::vec ŷ(p.m);
                 auto ψ = p.eval_ψ_ŷ(x, y, Σ, ŷ);
                 return std::make_tuple(ψ, ŷ);
             })
        .def("eval_grad_ψ_from_ŷ", &alpaqa::Problem::eval_grad_ψ_from_ŷ)
        .def("eval_grad_ψ_from_ŷ",
             [](const alpaqa::Problem &p, alpaqa::crvec x, alpaqa::crvec ŷ) {
                 alpaqa::vec grad_ψ(p.n), work(p.n);
                 p.eval_grad_ψ_from_ŷ(x, ŷ, grad_ψ, work);
                 return grad_ψ;
             })
        .def("eval_grad_ψ", &alpaqa::Problem::eval_grad_ψ)
        .def("eval_grad_ψ",
             [](const alpaqa::Problem &p, alpaqa::crvec x, alpaqa::crvec y,
                alpaqa::crvec Σ) {
                 alpaqa::vec grad_ψ(p.n), work_n(p.n), work_m(p.m);
                 p.eval_grad_ψ(x, y, Σ, grad_ψ, work_n, work_m);
                 return grad_ψ;
             })
        .def("eval_ψ_grad_ψ", &alpaqa::Problem::eval_ψ_grad_ψ)
        .def("eval_ψ_grad_ψ",
             [](const alpaqa::Problem &p, alpaqa::crvec x, alpaqa::crvec y,
                alpaqa::crvec Σ) {
                 alpaqa::vec grad_ψ(p.n), work_n(p.n), work_m(p.m);
                 auto ψ = p.eval_ψ_grad_ψ(x, y, Σ, grad_ψ, work_n, work_m);
                 return std::make_tuple(ψ, grad_ψ);
             })

        ;

    py::class_<alpaqa::ProblemWithParam, alpaqa::Problem,
               ProblemTrampoline<alpaqa::ProblemWithParam>>(
        m, "ProblemWithParam",
        "C++ documentation: :cpp:class:`alpaqa::ProblemWithParam`\n\n"
        "See :py:class:`alpaqa._alpaqa.Problem` for the full documentation.")
        .def_property(
            "param", py::overload_cast<>(&alpaqa::ProblemWithParam::get_param),
            [](alpaqa::ProblemWithParam &p, alpaqa::crvec param) {
                if (param.size() != p.get_param().size())
                    throw std::invalid_argument(
                        "Invalid parameter dimension: got " +
                        std::to_string(param.size()) + ", should be " +
                        std::to_string(p.get_param().size()) + ".");
                p.set_param(param);
            },
            "Parameter vector :math:`p` of the problem");

    py::class_<alpaqa::EvalCounter::EvalTimer>(
        m, "EvalTimer",
        "C++ documentation: "
        ":cpp:class:`alpaqa::EvalCounter::EvalTimer`\n\n")
        .def(py::pickle(
            [](const alpaqa::EvalCounter::EvalTimer &p) {   // __getstate__
                return py::make_tuple(p.f,                  //
                                      p.grad_f,             //
                                      p.f_grad_f,           //
                                      p.f_g,                //
                                      p.f_grad_f_g,         //
                                      p.grad_f_grad_g_prod, //
                                      p.g,                  //
                                      p.grad_g_prod,        //
                                      p.grad_gi,            //
                                      p.grad_L,             //
                                      p.hess_L_prod,        //
                                      p.hess_L,             //
                                      p.ψ,                  //
                                      p.grad_ψ,             //
                                      p.grad_ψ_from_ŷ,      //
                                      p.ψ_grad_ψ            //
                );
            },
            [](py::tuple t) { // __setstate__
                if (t.size() != 7)
                    throw std::runtime_error("Invalid state!");
                using T = alpaqa::EvalCounter::EvalTimer;
                return T{
                    py::cast<decltype(T::f)>(t[0]),
                    py::cast<decltype(T::grad_f)>(t[1]),
                    py::cast<decltype(T::f_grad_f)>(t[2]),
                    py::cast<decltype(T::f_g)>(t[3]),
                    py::cast<decltype(T::f_grad_f_g)>(t[4]),
                    py::cast<decltype(T::grad_f_grad_g_prod)>(t[5]),
                    py::cast<decltype(T::g)>(t[6]),
                    py::cast<decltype(T::grad_g_prod)>(t[7]),
                    py::cast<decltype(T::grad_gi)>(t[8]),
                    py::cast<decltype(T::grad_L)>(t[9]),
                    py::cast<decltype(T::hess_L_prod)>(t[10]),
                    py::cast<decltype(T::hess_L)>(t[11]),
                    py::cast<decltype(T::ψ)>(t[12]),
                    py::cast<decltype(T::grad_ψ)>(t[13]),
                    py::cast<decltype(T::grad_ψ_from_ŷ)>(t[14]),
                    py::cast<decltype(T::ψ_grad_ψ)>(t[15]),
                };
            }))
        .def_readwrite("f", &alpaqa::EvalCounter::EvalTimer::f)
        .def_readwrite("grad_f", &alpaqa::EvalCounter::EvalTimer::grad_f)
        .def_readwrite("f_grad_f", &alpaqa::EvalCounter::EvalTimer::f_grad_f)
        .def_readwrite("f_g", &alpaqa::EvalCounter::EvalTimer::f_g)
        .def_readwrite("f_grad_f_g",
                       &alpaqa::EvalCounter::EvalTimer::f_grad_f_g)
        .def_readwrite("grad_f_grad_g_prod",
                       &alpaqa::EvalCounter::EvalTimer::grad_f_grad_g_prod)
        .def_readwrite("g", &alpaqa::EvalCounter::EvalTimer::g)
        .def_readwrite("grad_g_prod",
                       &alpaqa::EvalCounter::EvalTimer::grad_g_prod)
        .def_readwrite("grad_gi", &alpaqa::EvalCounter::EvalTimer::grad_gi)
        .def_readwrite("grad_L", &alpaqa::EvalCounter::EvalTimer::grad_L)
        .def_readwrite("hess_L_prod",
                       &alpaqa::EvalCounter::EvalTimer::hess_L_prod)
        .def_readwrite("hess_L", &alpaqa::EvalCounter::EvalTimer::hess_L)
        .def_readwrite("ψ", &alpaqa::EvalCounter::EvalTimer::ψ)
        .def_readwrite("grad_ψ", &alpaqa::EvalCounter::EvalTimer::grad_ψ)
        .def_readwrite("grad_ψ_from_ŷ",
                       &alpaqa::EvalCounter::EvalTimer::grad_ψ_from_ŷ)
        .def_readwrite("ψ_grad_ψ", &alpaqa::EvalCounter::EvalTimer::ψ_grad_ψ);

    py::class_<alpaqa::EvalCounter>(m, "EvalCounter",
                                    "C++ documentation: "
                                    ":cpp:class:`alpaqa::EvalCounter`\n\n")
        .def(py::pickle(
            [](const alpaqa::EvalCounter &p) {              // __getstate__
                return py::make_tuple(p.f,                  //
                                      p.grad_f,             //
                                      p.f_grad_f,           //
                                      p.f_g,                //
                                      p.f_grad_f_g,         //
                                      p.grad_f_grad_g_prod, //
                                      p.g,                  //
                                      p.grad_g_prod,        //
                                      p.grad_gi,            //
                                      p.grad_L,             //
                                      p.hess_L_prod,        //
                                      p.hess_L,             //
                                      p.ψ,                  //
                                      p.grad_ψ,             //
                                      p.grad_ψ_from_ŷ,      //
                                      p.ψ_grad_ψ,           //
                                      p.time                //
                );
            },
            [](py::tuple t) { // __setstate__
                if (t.size() != 8)
                    throw std::runtime_error("Invalid state!");
                using T = alpaqa::EvalCounter;
                return T{
                    py::cast<decltype(T::f)>(t[0]),
                    py::cast<decltype(T::grad_f)>(t[1]),
                    py::cast<decltype(T::f_grad_f)>(t[2]),
                    py::cast<decltype(T::f_g)>(t[3]),
                    py::cast<decltype(T::f_grad_f_g)>(t[4]),
                    py::cast<decltype(T::grad_f_grad_g_prod)>(t[5]),
                    py::cast<decltype(T::g)>(t[6]),
                    py::cast<decltype(T::grad_g_prod)>(t[7]),
                    py::cast<decltype(T::grad_gi)>(t[8]),
                    py::cast<decltype(T::grad_L)>(t[9]),
                    py::cast<decltype(T::hess_L_prod)>(t[10]),
                    py::cast<decltype(T::hess_L)>(t[11]),
                    py::cast<decltype(T::ψ)>(t[12]),
                    py::cast<decltype(T::grad_ψ)>(t[13]),
                    py::cast<decltype(T::grad_ψ_from_ŷ)>(t[14]),
                    py::cast<decltype(T::ψ_grad_ψ)>(t[15]),
                    py::cast<decltype(T::time)>(t[16]),
                };
            }))
        .def_readwrite("f", &alpaqa::EvalCounter::f)
        .def_readwrite("grad_f", &alpaqa::EvalCounter::grad_f)
        .def_readwrite("f_grad_f", &alpaqa::EvalCounter::f_grad_f)
        .def_readwrite("f_g", &alpaqa::EvalCounter::f_g)
        .def_readwrite("f_grad_f_g", &alpaqa::EvalCounter::f_grad_f_g)
        .def_readwrite("grad_f_grad_g_prod",
                       &alpaqa::EvalCounter::grad_f_grad_g_prod)
        .def_readwrite("g", &alpaqa::EvalCounter::g)
        .def_readwrite("grad_g_prod", &alpaqa::EvalCounter::grad_g_prod)
        .def_readwrite("grad_gi", &alpaqa::EvalCounter::grad_gi)
        .def_readwrite("grad_L", &alpaqa::EvalCounter::grad_L)
        .def_readwrite("hess_L_prod", &alpaqa::EvalCounter::hess_L_prod)
        .def_readwrite("hess_L", &alpaqa::EvalCounter::hess_L)
        .def_readwrite("ψ", &alpaqa::EvalCounter::ψ)
        .def_readwrite("grad_ψ", &alpaqa::EvalCounter::grad_ψ)
        .def_readwrite("grad_ψ_from_ŷ", &alpaqa::EvalCounter::grad_ψ_from_ŷ)
        .def_readwrite("ψ_grad_ψ", &alpaqa::EvalCounter::ψ_grad_ψ)
        .def_readwrite("time", &alpaqa::EvalCounter::time)
        .def("__str__", [](const alpaqa::EvalCounter &c) {
            std::ostringstream os;
            os << c;
            return os.str();
        });

#if ALPAQA_HAVE_CASADI
    using alpaqa::CasADiProblem;
#else
    struct CasADiProblem : alpaqa::ProblemWithParam {};
#endif
    py::class_<CasADiProblem, alpaqa::ProblemWithParam,
               ProblemTrampoline<CasADiProblem>>(
        m, "CasADiProblem",
        "C++ documentation: "
        ":cpp:class:`alpaqa::CasADiProblem`\n\n"
        "See :py:class:`alpaqa._alpaqa.Problem` for the full documentation.");

    py::class_<alpaqa::ProblemWithCounters<CasADiProblem>, CasADiProblem,
               ProblemTrampoline<alpaqa::ProblemWithCounters<CasADiProblem>>>(
        m, "CasADiProblemWithCounters",
        "C++ documentation: "
        ":cpp:class:`alpaqa::ProblemWithCounters<alpaqa::CasADiProblem>`\n\n"
        "See :py:class:`alpaqa._alpaqa.Problem` for the full documentation.")
        .def_readwrite(
            "evaluations",
            &alpaqa::ProblemWithCounters<CasADiProblem>::evaluations);

    py::class_<alpaqa::PolymorphicPANOCDirectionBase,
               std::shared_ptr<alpaqa::PolymorphicPANOCDirectionBase>,
               alpaqa::PolymorphicPANOCDirectionTrampoline>(
        m, "PANOCDirection",
        "Class that provides fast directions for the PANOC algorithm (e.g. "
        "L-BFGS)")
        .def(py::init<>())
        .def("initialize", &alpaqa::PolymorphicPANOCDirectionBase::initialize)
        .def("update", &alpaqa::PolymorphicPANOCDirectionBase::update)
        .def("apply", &alpaqa::PolymorphicPANOCDirectionBase::apply_ret)
        .def("changed_γ", &alpaqa::PolymorphicPANOCDirectionBase::changed_γ)
        .def("reset", &alpaqa::PolymorphicPANOCDirectionBase::reset)
        .def("get_name", &alpaqa::PolymorphicPANOCDirectionBase::get_name)
        .def("__str__", &alpaqa::PolymorphicPANOCDirectionBase::get_name);

    using paLBFGSParamCBFGS = decltype(alpaqa::LBFGSParams::cbfgs);
    py::class_<paLBFGSParamCBFGS>(
        m, "LBFGSParamsCBFGS",
        "C++ documentation: :cpp:member:`alpaqa::LBFGSParams::cbfgs`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<paLBFGSParamCBFGS>))
        .def("to_dict", &struct_to_dict<paLBFGSParamCBFGS>)
        .def_readwrite("α", &paLBFGSParamCBFGS::α)
        .def_readwrite("ϵ", &paLBFGSParamCBFGS::ϵ);

    py::class_<alpaqa::LBFGSParams>(
        m, "LBFGSParams", "C++ documentation: :cpp:class:`alpaqa::LBFGSParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::LBFGSParams>))
        .def("to_dict", &struct_to_dict<alpaqa::LBFGSParams>)
        .def_readwrite("memory", &alpaqa::LBFGSParams::memory)
        .def_readwrite("cbfgs", &alpaqa::LBFGSParams::cbfgs)
        .def_readwrite("rescale_when_γ_changes",
                       &alpaqa::LBFGSParams::rescale_when_γ_changes);

    auto lbfgs = py::class_<alpaqa::LBFGS>(
        m, "LBFGS", "C++ documentation: :cpp:class:`alpaqa::LBFGS`");
    auto lbfgssign = py::enum_<alpaqa::LBFGS::Sign>(
        lbfgs, "Sign", "C++ documentation :cpp:enum:`alpaqa::LBFGS::Sign`");
    lbfgssign //
        .value("Positive", alpaqa::LBFGS::Sign::Positive)
        .value("Negative", alpaqa::LBFGS::Sign::Negative)
        .export_values();
    lbfgs //
        .def(py::init<alpaqa::LBFGS::Params>())
        .def(py::init<alpaqa::LBFGS::Params, size_t>())
        .def_static("update_valid", alpaqa::LBFGS::update_valid, "params"_a,
                    "yᵀs"_a, "sᵀs"_a, "pᵀp"_a)
        .def("update", &alpaqa::LBFGS::update, "xk"_a, "xkp1"_a, "pk"_a,
             "pkp1"_a, "sign"_a, "forced"_a = false)
        .def(
            "apply",
            [](alpaqa::LBFGS &lbfgs, alpaqa::rvec q, alpaqa::real_t γ) {
                return lbfgs.apply(q, γ);
            },
            "q"_a, "γ"_a)
        .def(
            "apply",
            [](alpaqa::LBFGS &lbfgs, alpaqa::rvec q, alpaqa::real_t γ,
               const std::vector<alpaqa::vec::Index> &J) {
                return lbfgs.apply(q, γ, J);
            },
            "q"_a, "γ"_a, "J"_a)
        .def("reset", &alpaqa::LBFGS::reset)
        .def("resize", &alpaqa::LBFGS::resize, "n"_a)
        .def("scale_y", &alpaqa::LBFGS::scale_y, "factor"_a)
        .def_property_readonly("n", &alpaqa::LBFGS::n)
        .def_property_readonly("history", &alpaqa::LBFGS::history)
        .def("s",
             [](alpaqa::LBFGS &lbfgs, size_t i) -> alpaqa::rvec {
                 return lbfgs.s(i);
             })
        .def("y",
             [](alpaqa::LBFGS &lbfgs, size_t i) -> alpaqa::rvec {
                 return lbfgs.y(i);
             })
        .def("ρ",
             [](alpaqa::LBFGS &lbfgs, size_t i) -> alpaqa::real_t & {
                 return lbfgs.ρ(i);
             })
        .def("α",
             [](alpaqa::LBFGS &lbfgs, size_t i) -> alpaqa::real_t & {
                 return lbfgs.α(i);
             })
        .def_property_readonly("__str__", &alpaqa::LBFGS::get_name)
        .def_property_readonly("params", &alpaqa::LBFGS::get_params);

    py::class_<alpaqa::PolymorphicLBFGSDirection,
               std::shared_ptr<alpaqa::PolymorphicLBFGSDirection>,
               alpaqa::PolymorphicPANOCDirectionBase>(
        m, "LBFGSDirection",
        "C++ documentation: :cpp:class:`alpaqa::LBFGSDirection`")
        .def(py::init<alpaqa::LBFGSParams>(), "params"_a)
        .def("initialize", &alpaqa::PolymorphicLBFGSDirection::initialize)
        .def("update", &alpaqa::PolymorphicLBFGSDirection::update)
        .def("apply", &alpaqa::PolymorphicLBFGSDirection::apply_ret)
        .def("changed_γ", &alpaqa::PolymorphicLBFGSDirection::changed_γ)
        .def("reset", &alpaqa::PolymorphicLBFGSDirection::reset)
        .def("get_name", &alpaqa::PolymorphicLBFGSDirection::get_name)
        .def("__str__", &alpaqa::PolymorphicLBFGSDirection::get_name)
        .def_property_readonly("params",
                               &alpaqa::PolymorphicLBFGSDirection::get_params);

    py::enum_<alpaqa::LBFGSStepSize>(
        m, "LBFGSStepsize",
        "C++ documentation: :cpp:enum:`alpaqa::LBFGSStepSize`")
        .value("BasedOnGradientStepSize",
               alpaqa::LBFGSStepSize::BasedOnGradientStepSize)
        .value("BasedOnCurvature", alpaqa::LBFGSStepSize::BasedOnCurvature)
        .export_values();

    py::class_<alpaqa::LipschitzEstimateParams>(
        m, "LipschitzEstimateParams",
        "C++ documentation: :cpp:class:`alpaqa::LipschitzEstimateParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::LipschitzEstimateParams>))
        .def("to_dict", &struct_to_dict<alpaqa::LipschitzEstimateParams>)
        .def_readwrite("L_0", &alpaqa::LipschitzEstimateParams::L₀)
        .def_readwrite("ε", &alpaqa::LipschitzEstimateParams::ε)
        .def_readwrite("δ", &alpaqa::LipschitzEstimateParams::δ)
        .def_readwrite("Lγ_factor",
                       &alpaqa::LipschitzEstimateParams::Lγ_factor);

    py::class_<alpaqa::PANOCParams>(
        m, "PANOCParams", "C++ documentation: :cpp:class:`alpaqa::PANOCParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::PANOCParams>))
        .def("to_dict", &struct_to_dict<alpaqa::PANOCParams>)
        .def_readwrite("Lipschitz", &alpaqa::PANOCParams::Lipschitz)
        .def_readwrite("max_iter", &alpaqa::PANOCParams::max_iter)
        .def_readwrite("max_time", &alpaqa::PANOCParams::max_time)
        .def_readwrite("τ_min", &alpaqa::PANOCParams::τ_min)
        .def_readwrite("L_min", &alpaqa::PANOCParams::L_min)
        .def_readwrite("L_max", &alpaqa::PANOCParams::L_max)
        .def_readwrite("max_no_progress", &alpaqa::PANOCParams::max_no_progress)
        .def_readwrite("print_interval", &alpaqa::PANOCParams::print_interval)
        .def_readwrite(
            "quadratic_upperbound_tolerance_factor",
            &alpaqa::PANOCParams::quadratic_upperbound_tolerance_factor)
        .def_readwrite("update_lipschitz_in_linesearch",
                       &alpaqa::PANOCParams::update_lipschitz_in_linesearch)
        .def_readwrite("alternative_linesearch_cond",
                       &alpaqa::PANOCParams::alternative_linesearch_cond)
        .def_readwrite("lbfgs_stepsize", &alpaqa::PANOCParams::lbfgs_stepsize);

    py::enum_<alpaqa::SolverStatus>(
        m, "SolverStatus", py::arithmetic(),
        "C++ documentation: :cpp:enum:`alpaqa::SolverStatus`")
        .value("Unknown", alpaqa::SolverStatus::Unknown, "Initial value")
        .value("Converged", alpaqa::SolverStatus::Converged,
               "Converged and reached given tolerance")
        .value("MaxTime", alpaqa::SolverStatus::MaxTime,
               "Maximum allowed execution time exceeded")
        .value("MaxIter", alpaqa::SolverStatus::MaxIter,
               "Maximum number of iterations exceeded")
        .value("NotFinite", alpaqa::SolverStatus::NotFinite,
               "Intermediate results were infinite or not-a-number")
        .value("NoProgress", alpaqa::SolverStatus::NoProgress,
               "No progress was made in the last iteration")
        .value("Interrupted", alpaqa::SolverStatus::Interrupted,
               "Solver was interrupted by the user")
        .export_values();

    py::class_<alpaqa::PolymorphicInnerSolverBase::Stats>(m, "InnerSolverStats")
        .def(py::init(&alpaqa::PolymorphicInnerSolverBase::Stats::from_dict));

    py::class_<alpaqa::PolymorphicInnerSolverBase,
               std::shared_ptr<alpaqa::PolymorphicInnerSolverBase>,
               alpaqa::PolymorphicInnerSolverTrampoline>(m, "InnerSolver")
        .def(py::init<>())
        .def("__call__", &alpaqa::PolymorphicInnerSolverBase::operator())
        .def("stop", &alpaqa::PolymorphicInnerSolverBase::stop)
        .def("get_name", &alpaqa::PolymorphicInnerSolverBase::get_name)
        .def("get_params", &alpaqa::PolymorphicInnerSolverBase::get_params);

    py::enum_<alpaqa::PANOCStopCrit>(
        m, "PANOCStopCrit",
        "C++ documentation: :cpp:enum:`alpaqa::PANOCStopCrit`")
        .value("ApproxKKT", alpaqa::PANOCStopCrit::ApproxKKT)
        .value("ApproxKKT2", alpaqa::PANOCStopCrit::ApproxKKT2)
        .value("ProjGradNorm", alpaqa::PANOCStopCrit::ProjGradNorm)
        .value("ProjGradNorm2", alpaqa::PANOCStopCrit::ProjGradNorm2)
        .value("ProjGradUnitNorm", alpaqa::PANOCStopCrit::ProjGradUnitNorm)
        .value("ProjGradUnitNorm2", alpaqa::PANOCStopCrit::ProjGradUnitNorm2)
        .value("FPRNorm", alpaqa::PANOCStopCrit::FPRNorm)
        .value("FPRNorm2", alpaqa::PANOCStopCrit::FPRNorm2)
        .value("Ipopt", alpaqa::PANOCStopCrit::Ipopt)
        .export_values();

    py::class_<alpaqa::PGAParams>(
        m, "PGAParams", "C++ documentation: :cpp:class:`alpaqa::PGAParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::PGAParams>))
        .def("to_dict", &struct_to_dict<alpaqa::PGAParams>)
        .def_readwrite("Lipschitz", &alpaqa::PGAParams::Lipschitz)
        .def_readwrite("max_iter", &alpaqa::PGAParams::max_iter)
        .def_readwrite("max_time", &alpaqa::PGAParams::max_time)
        .def_readwrite("L_min", &alpaqa::PGAParams::L_min)
        .def_readwrite("L_max", &alpaqa::PGAParams::L_max)
        .def_readwrite("stop_crit", &alpaqa::PGAParams::stop_crit)
        .def_readwrite("print_interval", &alpaqa::PGAParams::print_interval)
        .def_readwrite(
            "quadratic_upperbound_tolerance_factor",
            &alpaqa::PGAParams::quadratic_upperbound_tolerance_factor);

    py::class_<alpaqa::PGAProgressInfo>(
        m, "PGAProgressInfo",
        "C++ documentation: :cpp:class:`alpaqa::PGAProgressInfo`")
        .def_readonly("k", &alpaqa::PGAProgressInfo::k)
        .def_readonly("x", &alpaqa::PGAProgressInfo::x)
        .def_readonly("p", &alpaqa::PGAProgressInfo::p)
        .def_readonly("norm_sq_p", &alpaqa::PGAProgressInfo::norm_sq_p)
        .def_readonly("x̂", &alpaqa::PGAProgressInfo::x̂)
        .def_readonly("ψ", &alpaqa::PGAProgressInfo::ψ)
        .def_readonly("grad_ψ", &alpaqa::PGAProgressInfo::grad_ψ)
        .def_readonly("ψ_hat", &alpaqa::PGAProgressInfo::ψ_hat)
        .def_readonly("grad_ψ_hat", &alpaqa::PGAProgressInfo::grad_ψ_hat)
        .def_readonly("L", &alpaqa::PGAProgressInfo::L)
        .def_readonly("γ", &alpaqa::PGAProgressInfo::γ)
        .def_readonly("ε", &alpaqa::PGAProgressInfo::ε)
        .def_readonly("Σ", &alpaqa::PGAProgressInfo::Σ)
        .def_readonly("y", &alpaqa::PGAProgressInfo::y)
        .def_property_readonly("fpr", [](const alpaqa::PGAProgressInfo &p) {
            return std::sqrt(p.norm_sq_p) / p.γ;
        });

    py::class_<alpaqa::GAAPGAParams>(
        m, "GAAPGAParams",
        "C++ documentation: :cpp:class:`alpaqa::GAAPGAParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::GAAPGAParams>))
        .def("to_dict", &struct_to_dict<alpaqa::GAAPGAParams>)
        .def_readwrite("Lipschitz", &alpaqa::GAAPGAParams::Lipschitz)
        .def_readwrite("limitedqr_mem", &alpaqa::GAAPGAParams::limitedqr_mem)
        .def_readwrite("max_iter", &alpaqa::GAAPGAParams::max_iter)
        .def_readwrite("max_time", &alpaqa::GAAPGAParams::max_time)
        .def_readwrite("L_min", &alpaqa::GAAPGAParams::L_min)
        .def_readwrite("L_max", &alpaqa::GAAPGAParams::L_max)
        .def_readwrite("stop_crit", &alpaqa::GAAPGAParams::stop_crit)
        .def_readwrite("print_interval", &alpaqa::GAAPGAParams::print_interval)
        .def_readwrite(
            "quadratic_upperbound_tolerance_factor",
            &alpaqa::GAAPGAParams::quadratic_upperbound_tolerance_factor)
        .def_readwrite("max_no_progress",
                       &alpaqa::GAAPGAParams::max_no_progress)
        .def_readwrite("full_flush_on_γ_change",
                       &alpaqa::GAAPGAParams::full_flush_on_γ_change);

    py::class_<alpaqa::GAAPGAProgressInfo>(
        m, "GAAPGAProgressInfo",
        "C++ documentation: :cpp:class:`alpaqa::GAAPGAProgressInfo`")
        .def_readonly("k", &alpaqa::GAAPGAProgressInfo::k)
        .def_readonly("x", &alpaqa::GAAPGAProgressInfo::x)
        .def_readonly("p", &alpaqa::GAAPGAProgressInfo::p)
        .def_readonly("norm_sq_p", &alpaqa::GAAPGAProgressInfo::norm_sq_p)
        .def_readonly("x̂", &alpaqa::GAAPGAProgressInfo::x̂)
        .def_readonly("ψ", &alpaqa::GAAPGAProgressInfo::ψ)
        .def_readonly("grad_ψ", &alpaqa::GAAPGAProgressInfo::grad_ψ)
        .def_readonly("ψ_hat", &alpaqa::GAAPGAProgressInfo::ψ_hat)
        .def_readonly("grad_ψ_hat", &alpaqa::GAAPGAProgressInfo::grad_ψ_hat)
        .def_readonly("L", &alpaqa::GAAPGAProgressInfo::L)
        .def_readonly("γ", &alpaqa::GAAPGAProgressInfo::γ)
        .def_readonly("ε", &alpaqa::GAAPGAProgressInfo::ε)
        .def_readonly("Σ", &alpaqa::GAAPGAProgressInfo::Σ)
        .def_readonly("y", &alpaqa::GAAPGAProgressInfo::y)
        .def_property_readonly("fpr", [](const alpaqa::GAAPGAProgressInfo &p) {
            return std::sqrt(p.norm_sq_p) / p.γ;
        });

    py::class_<alpaqa::PANOCProgressInfo>(
        m, "PANOCProgressInfo",
        "Data passed to the PANOC progress callback.\n\n"
        "C++ documentation: :cpp:class:`alpaqa::PANOCProgressInfo`")
        .def_readonly("k", &alpaqa::PANOCProgressInfo::k, //
                      "Iteration")
        .def_readonly("x", &alpaqa::PANOCProgressInfo::x, //
                      "Decision variable :math:`x`")
        .def_readonly("p", &alpaqa::PANOCProgressInfo::p, //
                      "Projected gradient step :math:`p`")
        .def_readonly("norm_sq_p", &alpaqa::PANOCProgressInfo::norm_sq_p, //
                      ":math:`\\left\\|p\\right\\|^2`")
        .def_readonly(
            "x̂", &alpaqa::PANOCProgressInfo::x̂, //
            "Decision variable after projected gradient step :math:`\\hat x`")
        .def_readonly("φγ", &alpaqa::PANOCProgressInfo::φγ, //
                      "Forward-backward envelope :math:`\\varphi_\\gamma(x)`")
        .def_readonly("ψ", &alpaqa::PANOCProgressInfo::ψ, //
                      "Objective value :math:`\\psi(x)`")
        .def_readonly("grad_ψ", &alpaqa::PANOCProgressInfo::grad_ψ, //
                      "Gradient of objective :math:`\\nabla\\psi(x)`")
        .def_readonly("ψ_hat", &alpaqa::PANOCProgressInfo::ψ_hat)
        .def_readonly("grad_ψ_hat", &alpaqa::PANOCProgressInfo::grad_ψ_hat)
        .def_readonly("L", &alpaqa::PANOCProgressInfo::L, //
                      "Estimate of Lipschitz constant of objective :math:`L`")
        .def_readonly("γ", &alpaqa::PANOCProgressInfo::γ,
                      "Step size :math:`\\gamma`")
        .def_readonly("τ", &alpaqa::PANOCProgressInfo::τ, //
                      "Line search parameter :math:`\\tau`")
        .def_readonly("ε", &alpaqa::PANOCProgressInfo::ε, //
                      "Tolerance reached :math:`\\varepsilon_k`")
        .def_readonly("Σ", &alpaqa::PANOCProgressInfo::Σ, //
                      "Penalty factor :math:`\\Sigma`")
        .def_readonly("y", &alpaqa::PANOCProgressInfo::y, //
                      "Lagrange multipliers :math:`y`")
        .def_property_readonly(
            "fpr",
            [](const alpaqa::PANOCProgressInfo &p) {
                return std::sqrt(p.norm_sq_p) / p.γ;
            },
            "Fixed-point residual :math:`\\left\\|p\\right\\| / \\gamma`");

    py::class_<alpaqa::StructuredPANOCLBFGSProgressInfo>(
        m, "StructuredPANOCLBFGSProgressInfo",
        "Data passed to the structured PANOC progress callback.\n\n"
        "C++ documentation: "
        ":cpp:class:`alpaqa::StructuredPANOCLBFGSProgressInfo`")
        .def_readonly("k", &alpaqa::StructuredPANOCLBFGSProgressInfo::k)
        .def_readonly("x", &alpaqa::StructuredPANOCLBFGSProgressInfo::x)
        .def_readonly("p", &alpaqa::StructuredPANOCLBFGSProgressInfo::p)
        .def_readonly("norm_sq_p",
                      &alpaqa::StructuredPANOCLBFGSProgressInfo::norm_sq_p)
        .def_readonly("x̂", &alpaqa::StructuredPANOCLBFGSProgressInfo::x̂)
        .def_readonly("q", &alpaqa::StructuredPANOCLBFGSProgressInfo::q)
        .def_readonly("J", &alpaqa::StructuredPANOCLBFGSProgressInfo::J)
        .def_readonly("φγ", &alpaqa::StructuredPANOCLBFGSProgressInfo::φγ)
        .def_readonly("ψ", &alpaqa::StructuredPANOCLBFGSProgressInfo::ψ)
        .def_readonly("grad_ψ",
                      &alpaqa::StructuredPANOCLBFGSProgressInfo::grad_ψ)
        .def_readonly("ψ_hat", &alpaqa::StructuredPANOCLBFGSProgressInfo::ψ_hat)
        .def_readonly("grad_ψ_hat",
                      &alpaqa::StructuredPANOCLBFGSProgressInfo::grad_ψ_hat)
        .def_readonly("L", &alpaqa::StructuredPANOCLBFGSProgressInfo::L)
        .def_readonly("γ", &alpaqa::StructuredPANOCLBFGSProgressInfo::γ)
        .def_readonly("τ", &alpaqa::StructuredPANOCLBFGSProgressInfo::τ)
        .def_readonly("ε", &alpaqa::StructuredPANOCLBFGSProgressInfo::ε)
        .def_readonly("Σ", &alpaqa::StructuredPANOCLBFGSProgressInfo::Σ)
        .def_readonly("y", &alpaqa::StructuredPANOCLBFGSProgressInfo::y)
        .def_property_readonly(
            "fpr", [](const alpaqa::StructuredPANOCLBFGSProgressInfo &p) {
                return std::sqrt(p.norm_sq_p) / p.γ;
            });

    py::class_<alpaqa::PolymorphicPANOCSolver,
               std::shared_ptr<alpaqa::PolymorphicPANOCSolver>,
               alpaqa::PolymorphicInnerSolverBase>(
        m, "PANOCSolver", "C++ documentation: :cpp:class:`alpaqa::PANOCSolver`")
        .def(py::init([] {
            return std::make_shared<alpaqa::PolymorphicPANOCSolver>(
                alpaqa::PANOCSolver<alpaqa::PolymorphicPANOCDirectionBase>{
                    alpaqa::PANOCParams{},
                    std::static_pointer_cast<
                        alpaqa::PolymorphicPANOCDirectionBase>(
                        std::make_shared<alpaqa::PolymorphicLBFGSDirection>(
                            alpaqa::LBFGSParams{}))});
        }))
        .def(py::init(PolymorphicPANOCConstructor< //
                      alpaqa::PolymorphicLBFGSDirection>()),
             "panoc_params"_a, "lbfgs_direction"_a)
        .def(py::init(PolymorphicPANOCDefaultConversion< //
                      alpaqa::PolymorphicLBFGSDirection>()),
             "panoc_params"_a, "lbfgs_params"_a)
        .def(py::init(PolymorphicPANOCConstructor< //
                      alpaqa::PolymorphicPANOCDirectionTrampoline>()),
             "panoc_params"_a, "direction"_a)
        .def(
            "set_progress_callback",
            &alpaqa::PolymorphicPANOCSolver::set_progress_callback,
            "callback"_a,
            "Attach a callback that is called on each iteration of the solver.")
        .def("__call__",
             alpaqa::InnerSolverCallWrapper<alpaqa::PolymorphicPANOCSolver>(),
             py::call_guard<py::scoped_ostream_redirect,
                            py::scoped_estream_redirect>(),
             "problem"_a, "Σ"_a, "ε"_a, "x"_a,
             "y"_a, //
             "Solve.\n\n"
             ":param problem: Problem to solve\n"
             ":param Σ: Penalty factor\n"
             ":param ε: Desired tolerance\n"
             ":param x: Initial guess\n"
             ":param y: Initial Lagrange multipliers\n\n"
             ":return: * Solution :math:`x`\n"
             "         * Updated Lagrange multipliers :math:`y`\n"
             "         * Slack variable error :math:`g(x) - z`\n"
             "         * Statistics\n\n")
        .def("__str__", &alpaqa::PolymorphicPANOCSolver::get_name)
        .def_property_readonly("params",
                               &alpaqa::PolymorphicPANOCSolver::get_params)
        .def_property_readonly(
            "direction", [](const alpaqa::PolymorphicPANOCSolver &s) {
                return s.innersolver.direction_provider.direction;
            });

    py::class_<alpaqa::PolymorphicPGASolver,
               std::shared_ptr<alpaqa::PolymorphicPGASolver>,
               alpaqa::PolymorphicInnerSolverBase>(
        m, "PGASolver", "C++ documentation: :cpp:class:`alpaqa::PGASolver`")
        .def(py::init<alpaqa::PGAParams>())
        .def(
            "set_progress_callback",
            &alpaqa::PolymorphicPGASolver::set_progress_callback, "callback"_a,
            "Attach a callback that is called on each iteration of the solver.")
        .def("__call__",
             alpaqa::InnerSolverCallWrapper<alpaqa::PolymorphicPGASolver>(),
             py::call_guard<py::scoped_ostream_redirect,
                            py::scoped_estream_redirect>(),
             "problem"_a, "Σ"_a, "ε"_a, "x"_a,
             "y"_a, //
             "Solve.\n\n"
             ":param problem: Problem to solve\n"
             ":param Σ: Penalty factor\n"
             ":param ε: Desired tolerance\n"
             ":param x: Initial guess\n"
             ":param y: Initial Lagrange multipliers\n\n"
             ":return: * Solution :math:`x`\n"
             "         * Updated Lagrange multipliers :math:`y`\n"
             "         * Slack variable error :math:`g(x) - z`\n"
             "         * Statistics\n\n")
        .def("__str__", &alpaqa::PolymorphicPGASolver::get_name)
        .def_property_readonly("params",
                               &alpaqa::PolymorphicPGASolver::get_params);

    py::class_<alpaqa::PolymorphicGAAPGASolver,
               std::shared_ptr<alpaqa::PolymorphicGAAPGASolver>,
               alpaqa::PolymorphicInnerSolverBase>(
        m, "GAAPGASolver",
        "C++ documentation: :cpp:class:`alpaqa::GAAPGASolver`")
        .def(py::init<alpaqa::GAAPGAParams>())
        .def(
            "set_progress_callback",
            &alpaqa::PolymorphicGAAPGASolver::set_progress_callback,
            "callback"_a,
            "Attach a callback that is called on each iteration of the solver.")
        .def("__call__",
             alpaqa::InnerSolverCallWrapper<alpaqa::PolymorphicGAAPGASolver>(),
             py::call_guard<py::scoped_ostream_redirect,
                            py::scoped_estream_redirect>(),
             "problem"_a, "Σ"_a, "ε"_a, "x"_a,
             "y"_a, //
             "Solve.\n\n"
             ":param problem: Problem to solve\n"
             ":param Σ: Penalty factor\n"
             ":param ε: Desired tolerance\n"
             ":param x: Initial guess\n"
             ":param y: Initial Lagrange multipliers\n\n"
             ":return: * Solution :math:`x`\n"
             "         * Updated Lagrange multipliers :math:`y`\n"
             "         * Slack variable error :math:`g(x) - z`\n"
             "         * Statistics\n\n")
        .def("__str__", &alpaqa::PolymorphicGAAPGASolver::get_name)
        .def_property_readonly("params",
                               &alpaqa::PolymorphicGAAPGASolver::get_params);

    py::class_<alpaqa::StructuredPANOCLBFGSParams>(
        m, "StructuredPANOCLBFGSParams",
        "C++ documentation: :cpp:class:`alpaqa::StructuredPANOCLBFGSParams`")
        .def(py::init(&kwargs_to_struct<alpaqa::StructuredPANOCLBFGSParams>))
        .def("to_dict", &struct_to_dict<alpaqa::StructuredPANOCLBFGSParams>)

        .def_readwrite("Lipschitz",
                       &alpaqa::StructuredPANOCLBFGSParams::Lipschitz)
        .def_readwrite("max_iter",
                       &alpaqa::StructuredPANOCLBFGSParams::max_iter)
        .def_readwrite("max_time",
                       &alpaqa::StructuredPANOCLBFGSParams::max_time)
        .def_readwrite("τ_min", &alpaqa::StructuredPANOCLBFGSParams::τ_min)
        .def_readwrite("L_min", &alpaqa::StructuredPANOCLBFGSParams::L_min)
        .def_readwrite("L_max", &alpaqa::StructuredPANOCLBFGSParams::L_max)
        .def_readwrite(
            "nonmonotone_linesearch",
            &alpaqa::StructuredPANOCLBFGSParams::nonmonotone_linesearch)
        .def_readwrite(
            "fpr_shortcut_accept_factor",
            &alpaqa::StructuredPANOCLBFGSParams::fpr_shortcut_accept_factor)
        .def_readwrite(
            "fpr_shortcut_history",
            &alpaqa::StructuredPANOCLBFGSParams::fpr_shortcut_history)
        .def_readwrite("stop_crit",
                       &alpaqa::StructuredPANOCLBFGSParams::stop_crit)
        .def_readwrite("max_no_progress",
                       &alpaqa::StructuredPANOCLBFGSParams::max_no_progress)
        .def_readwrite("print_interval",
                       &alpaqa::StructuredPANOCLBFGSParams::print_interval)
        .def_readwrite("quadratic_upperbound_tolerance_factor",
                       &alpaqa::StructuredPANOCLBFGSParams::
                           quadratic_upperbound_tolerance_factor)
        .def_readwrite(
            "update_lipschitz_in_linesearch",
            &alpaqa::StructuredPANOCLBFGSParams::update_lipschitz_in_linesearch)
        .def_readwrite(
            "alternative_linesearch_cond",
            &alpaqa::StructuredPANOCLBFGSParams::alternative_linesearch_cond)
        .def_readwrite("hessian_vec",
                       &alpaqa::StructuredPANOCLBFGSParams::hessian_vec)
        .def_readwrite(
            "hessian_vec_finite_differences",
            &alpaqa::StructuredPANOCLBFGSParams::hessian_vec_finite_differences)
        .def_readwrite(
            "full_augmented_hessian",
            &alpaqa::StructuredPANOCLBFGSParams::full_augmented_hessian)
        .def_readwrite(
            "hessian_step_size_heuristic",
            &alpaqa::StructuredPANOCLBFGSParams::hessian_step_size_heuristic)
        .def_readwrite("lbfgs_stepsize",
                       &alpaqa::StructuredPANOCLBFGSParams::lbfgs_stepsize);

    py::class_<alpaqa::PolymorphicStructuredPANOCLBFGSSolver,
               std::shared_ptr<alpaqa::PolymorphicStructuredPANOCLBFGSSolver>,
               alpaqa::PolymorphicInnerSolverBase>(
        m, "StructuredPANOCLBFGSSolver",
        "C++ documentation: :cpp:class:`alpaqa::StructuredPANOCLBFGSSolver`")
        .def(py::init([] {
            return std::make_shared<
                alpaqa::PolymorphicStructuredPANOCLBFGSSolver>(
                alpaqa::StructuredPANOCLBFGSSolver{
                    alpaqa::StructuredPANOCLBFGSParams{},
                    alpaqa::LBFGSParams{},
                });
        }))
        .def(
            py::init([](const std::variant<alpaqa::StructuredPANOCLBFGSParams,
                                           py::dict> &pp,
                        const std::variant<alpaqa::LBFGSParams, py::dict> &lp) {
                return std::make_shared<
                    alpaqa::PolymorphicStructuredPANOCLBFGSSolver>(
                    var_kwargs_to_struct(pp), var_kwargs_to_struct(lp));
            }),
            "panoc_params"_a, "lbfgs_params"_a)
        .def(
            "set_progress_callback",
            &alpaqa::PolymorphicStructuredPANOCLBFGSSolver::
                set_progress_callback,
            "callback"_a,
            "Attach a callback that is called on each iteration of the solver.")
        .def("__call__",
             alpaqa::InnerSolverCallWrapper<
                 alpaqa::PolymorphicStructuredPANOCLBFGSSolver>(),
             py::call_guard<py::scoped_ostream_redirect,
                            py::scoped_estream_redirect>(),
             "problem"_a, "Σ"_a, "ε"_a, "x"_a,
             "y"_a, //
             "Solve.\n\n"
             ":param problem: Problem to solve\n"
             ":param Σ: Penalty factor\n"
             ":param ε: Desired tolerance\n"
             ":param x: Initial guess\n"
             ":param y: Initial Lagrange multipliers\n\n"
             ":return: * Solution :math:`x`\n"
             "         * Updated Lagrange multipliers :math:`y`\n"
             "         * Slack variable error :math:`g(x) - z`\n"
             "         * Statistics\n\n")
        .def("__str__",
             &alpaqa::PolymorphicStructuredPANOCLBFGSSolver::get_name)
        .def_property_readonly(
            "params",
            &alpaqa::PolymorphicStructuredPANOCLBFGSSolver::get_params);

    py::class_<alpaqa::ALMParams>(
        m, "ALMParams", "C++ documentation: :cpp:class:`alpaqa::ALMParams`")
        .def(py::init())
        .def(py::init(&kwargs_to_struct<alpaqa::ALMParams>))
        .def("to_dict", &struct_to_dict<alpaqa::ALMParams>)
        .def_readwrite("ε", &alpaqa::ALMParams::ε)
        .def_readwrite("δ", &alpaqa::ALMParams::δ)
        .def_readwrite("Δ", &alpaqa::ALMParams::Δ)
        .def_readwrite("Δ_lower", &alpaqa::ALMParams::Δ_lower)
        .def_readwrite("Σ_0", &alpaqa::ALMParams::Σ₀)
        .def_readwrite("σ_0", &alpaqa::ALMParams::σ₀)
        .def_readwrite("Σ_0_lower", &alpaqa::ALMParams::Σ₀_lower)
        .def_readwrite("ε_0", &alpaqa::ALMParams::ε₀)
        .def_readwrite("ε_0_increase", &alpaqa::ALMParams::ε₀_increase)
        .def_readwrite("ρ", &alpaqa::ALMParams::ρ)
        .def_readwrite("ρ_increase", &alpaqa::ALMParams::ρ_increase)
        .def_readwrite("θ", &alpaqa::ALMParams::θ)
        .def_readwrite("M", &alpaqa::ALMParams::M)
        .def_readwrite("Σ_max", &alpaqa::ALMParams::Σ_max)
        .def_readwrite("Σ_min", &alpaqa::ALMParams::Σ_min)
        .def_readwrite("max_iter", &alpaqa::ALMParams::max_iter)
        .def_readwrite("max_time", &alpaqa::ALMParams::max_time)
        .def_readwrite("max_num_initial_retries",
                       &alpaqa::ALMParams::max_num_initial_retries)
        .def_readwrite("max_num_retries", &alpaqa::ALMParams::max_num_retries)
        .def_readwrite("max_total_num_retries",
                       &alpaqa::ALMParams::max_total_num_retries)
        .def_readwrite("print_interval", &alpaqa::ALMParams::print_interval)
        .def_readwrite("single_penalty_factor",
                       &alpaqa::ALMParams::single_penalty_factor);

    py::class_<alpaqa::PolymorphicALMSolver>(
        m, "ALMSolver",
        "Main augmented Lagrangian solver.\n\n"
        "C++ documentation: :cpp:class:`alpaqa::ALMSolver`")
        // Default constructor
        .def(py::init([] {
                 return alpaqa::PolymorphicALMSolver{
                     alpaqa::ALMParams{},
                     std::static_pointer_cast<
                         alpaqa::PolymorphicInnerSolverBase>(
                         std::make_shared<
                             alpaqa::PolymorphicStructuredPANOCLBFGSSolver>(
                             alpaqa::StructuredPANOCLBFGSParams{},
                             alpaqa::LBFGSParams{})),
                 };
             }),
             "Build an ALM solver using Structured PANOC as inner solver.")
        // Params and solver
        .def(py::init(
                 PolymorphicALMConstructor<alpaqa::PolymorphicPANOCSolver>()),
             "alm_params"_a, "panoc_solver"_a,
             "Build an ALM solver using PANOC as inner solver.")
        .def(
            py::init(PolymorphicALMConstructor<alpaqa::PolymorphicPGASolver>()),
            "alm_params"_a, "pga_solver"_a,
            "Build an ALM solver using the projected gradient algorithm as "
            "inner solver.")
        .def(py::init(PolymorphicALMConstructor<
                      alpaqa::PolymorphicStructuredPANOCLBFGSSolver>()),
             "alm_params"_a, "structuredpanoc_solver"_a,
             "Build an ALM solver using Structured PANOC as inner solver.")
        .def(py::init(PolymorphicALMConstructor<
                      alpaqa::PolymorphicInnerSolverTrampoline>()),
             "alm_params"_a, "inner_solver"_a,
             "Build an ALM solver using a custom inner solver.")
        // Only solver (default params)
        .def(py::init(PolymorphicALMConstructorDefaultParams<
                      alpaqa::PolymorphicPANOCSolver>()),
             "panoc_solver"_a,
             "Build an ALM solver using PANOC as inner solver.")
        .def(py::init(PolymorphicALMConstructorDefaultParams<
                      alpaqa::PolymorphicPGASolver>()),
             "pga_solver"_a,
             "Build an ALM solver using the projected gradient algorithm as "
             "inner solver.")
        .def(py::init(PolymorphicALMConstructorDefaultParams<
                      alpaqa::PolymorphicStructuredPANOCLBFGSSolver>()),
             "structuredpanoc_solver"_a,
             "Build an ALM solver using Structured PANOC as inner solver.")
        .def(py::init(PolymorphicALMConstructorDefaultParams<
                      alpaqa::PolymorphicInnerSolverTrampoline>()),
             "inner_solver"_a,
             "Build an ALM solver using a custom inner solver.")
        // Only params (default solver)
        .def(py::init([](const alpaqa::ALMParams &params) {
                 return alpaqa::PolymorphicALMSolver{
                     params,
                     std::static_pointer_cast<
                         alpaqa::PolymorphicInnerSolverBase>(
                         std::make_shared<
                             alpaqa::PolymorphicStructuredPANOCLBFGSSolver>(
                             alpaqa::StructuredPANOCLBFGSParams{},
                             alpaqa::LBFGSParams{})),
                 };
             }),
             "alm_params"_a,
             "Build an ALM solver using Structured PANOC as inner solver.")
        // Other functions and properties
        .def_property_readonly("inner_solver",
                               [](const alpaqa::PolymorphicALMSolver &s) {
                                   return s.inner_solver.solver;
                               })
        .def(
            "__call__",
            [](alpaqa::PolymorphicALMSolver &solver, const alpaqa::Problem &p,
               std::optional<alpaqa::vec> x, std::optional<alpaqa::vec> y)
                -> std::tuple<alpaqa::vec, alpaqa::vec, py::dict> {
                if (!x)
                    x = alpaqa::vec::Zero(p.n);
                else if (x->size() != p.n)
                    throw std::invalid_argument(
                        "Length of x does not match problem size problem.n");
                if (!y)
                    y = alpaqa::vec::Zero(p.m);
                else if (y->size() != p.m)
                    throw std::invalid_argument(
                        "Length of y does not match problem size problem.m");
                if (p.C.lowerbound.size() != p.n)
                    throw std::invalid_argument(
                        "Length of problem.C.lowerbound does not match problem "
                        "size problem.n");
                if (p.C.upperbound.size() != p.n)
                    throw std::invalid_argument(
                        "Length of problem.C.upperbound does not match problem "
                        "size problem.n");
                if (p.D.lowerbound.size() != p.m)
                    throw std::invalid_argument(
                        "Length of problem.D.lowerbound does not match problem "
                        "size problem.m");
                if (p.D.upperbound.size() != p.m)
                    throw std::invalid_argument(
                        "Length of problem.D.upperbound does not match problem "
                        "size problem.m");

                auto stats = solver(p, *y, *x);
                return std::make_tuple(std::move(*x), std::move(*y),
                                       stats_to_dict(stats));
            },
            "problem"_a, "x"_a = std::nullopt, "y"_a = std::nullopt,
            py::call_guard<py::scoped_ostream_redirect,
                           py::scoped_estream_redirect>(),
            "Solve.\n\n"
            ":param problem: Problem to solve.\n"
            ":param y: Initial guess for Lagrange multipliers :math:`y`\n"
            ":param x: Initial guess for decision variables :math:`x`\n\n"
            ":return: * Lagrange multipliers :math:`y` at the solution\n"
            "         * Solution :math:`x`\n"
            "         * Statistics\n\n")
        .def("__str__", &alpaqa::PolymorphicALMSolver::get_name)
        .def_property_readonly("params",
                               &alpaqa::PolymorphicALMSolver::get_params);

    constexpr auto panoc = [](std::function<alpaqa::real_t(alpaqa::crvec)> ψ,
                              std::function<alpaqa::vec(alpaqa::crvec)> grad_ψ,
                              const alpaqa::Box &C,
                              std::optional<alpaqa::vec> x0, alpaqa::real_t ε,
                              const alpaqa::PANOCParams &params,
                              const alpaqa::LBFGSParams &lbfgs_params) {
        auto n = C.lowerbound.size();
        if (C.upperbound.size() != n)
            throw std::invalid_argument("Length of C.upperbound does not "
                                        "match length of C.lowerbound");
        if (!x0)
            x0 = alpaqa::vec::Zero(n);
        else if (x0->size() != n)
            throw std::invalid_argument(
                "Length of x does not match length of bounds C");
        auto grad_ψ_ = [&](alpaqa::crvec x, alpaqa::rvec gr) {
            auto &&t = grad_ψ(x);
            if (t.size() != x.size())
                throw std::runtime_error("Invalid grad_ψ dimension");
            gr = std::move(t);
        };
        auto stats = alpaqa::panoc<alpaqa::LBFGS>(ψ, grad_ψ_, C, *x0, ε, params,
                                                  {lbfgs_params});
        return std::make_tuple(std::move(*x0), stats_to_dict(stats));
    };

    m.def("panoc", panoc, "ψ"_a, "grad_ψ"_a, "C"_a, "x0"_a = std::nullopt,
          "ε"_a = 1e-8, "params"_a = alpaqa::PANOCParams{},
          "lbfgs_params"_a = alpaqa::LBFGSParams{});

#if !ALPAQA_HAVE_CASADI
    constexpr static auto load_CasADi_problem =
        [](const char *, unsigned, unsigned, unsigned, bool,
           bool) -> alpaqa::ProblemWithParam {
        throw std::runtime_error(
            "This version of alpaqa was compiled without CasADi support");
    };
#else
    constexpr static auto load_CasADi_problem =
        [](const char *so_name, unsigned n, unsigned m, unsigned p,
           bool second_order,
           bool counted) -> std::unique_ptr<alpaqa::CasADiProblem> {
        auto prob = std::make_unique<alpaqa::CasADiProblem>(so_name, n, m, p,
                                                            second_order);
        if (counted)
            prob = std::make_unique<
                alpaqa::ProblemWithCounters<alpaqa::CasADiProblem>>(
                std::move(*prob));
        return prob;
    };
#endif

    m.def("load_casadi_problem", load_CasADi_problem, "so_name"_a, "n"_a = 0,
          "m"_a = 0, "p"_a = 0, "second_order"_a = false, "counted"_a = true,
          "Load a compiled CasADi problem.\n\n");

    m.def_submodule("detail",
                    "C++ documentation: :cpp:namespace:`alpaqa::detail`")
        .def(
            "calc_err_z",
            [](const alpaqa::Problem &p, alpaqa::crvec x̂, alpaqa::crvec y,
               alpaqa::crvec Σ) {
                alpaqa::vec e(p.m);
                alpaqa::detail::calc_err_z(p, x̂, y, Σ, e);
                return e;
            },
            "p"_a, "x̂"_a, "y"_a, "Σ"_a,
            "C++ documentation: :cpp:function:`alpaqa::detail::calc_err_z`")
        .def(
            "projected_gradient_step",
            [](const alpaqa::Box &C, alpaqa::real_t γ, alpaqa::crvec x,
               alpaqa::crvec grad_ψ) -> alpaqa::vec {
                return alpaqa::detail::projected_gradient_step(C, γ, x, grad_ψ);
            },
            "C"_a, "γ"_a, "x"_a, "grad_ψ"_a,
            "C++ documentation: "
            ":cpp:function:`alpaqa::detail::projected_gradient_step`")
        .def("calc_error_stop_crit", &alpaqa::detail::calc_error_stop_crit,
             "C"_a, "crit"_a, "p"_a, "γ"_a, "x"_a, "x̂"_a, "ŷ"_a, "grad_ψx"_a,
             "grad_ψx̂"_a,
             "C++ documentation: "
             ":cpp:function:`alpaqa::detail::calc_error_stop_crit`")
        .def("stop_crit_requires_grad_ψx̂",
             &alpaqa::detail::stop_crit_requires_grad_ψx̂, "crit"_a,
             "C++ documentation: "
             ":cpp:function:`alpaqa::detail::stop_crit_requires_grad_ψx̂`");
}

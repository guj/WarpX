/* Copyright 2024 Justin Angus
 *
 * This file is part of WarpX.
 *
 * License: BSD-3-Clause-LBNL
 */
#ifndef NEWTON_SOLVER_H_
#define NEWTON_SOLVER_H_

#include "NonlinearSolver.H"
#include "JacobianFunctionMF.H"
#include "Preconditioner.H"
#include "Utils/TextMsg.H"

#include <AMReX_GMRES.H>
#include <AMReX_ParmParse.H>

#include <vector>
#include <istream>
#include <filesystem>

/**
 * \brief Newton method to solve nonlinear equation of form:
 * F(U) = U - b - R(U) = 0. U is the solution vector, b is a constant,
 * and R(U) is some nonlinear function of U, which is computed in the
 * ComputeRHS() Ops function.
 */

template<class Vec, class Ops>
class NewtonSolver : public NonlinearSolver<Vec,Ops>
{
public:

    NewtonSolver() = default;

    ~NewtonSolver() override = default;

    // Prohibit Move and Copy operations
    NewtonSolver(const NewtonSolver&) = delete;
    NewtonSolver& operator=(const NewtonSolver&) = delete;
    NewtonSolver(NewtonSolver&&) noexcept = delete;
    NewtonSolver& operator=(NewtonSolver&&) noexcept = delete;

    void Define ( const Vec&  a_U,
                        Ops*  a_ops ) override;

    void Solve ( Vec&   a_U,
           const Vec&   a_b,
           amrex::Real  a_time,
           amrex::Real  a_dt,
           int          a_step) const override;

    void GetSolverParams ( amrex::Real&  a_rtol,
                           amrex::Real&  a_atol,
                           int&          a_maxits ) override
    {
        a_rtol = m_rtol;
        a_atol = m_atol;
        a_maxits = m_maxits;
    }

    inline void CurTime ( amrex::Real  a_time ) const
    {
        m_cur_time = a_time;
        m_linear_function->curTime( a_time );
    }

    inline void CurTimeStep ( amrex::Real  a_dt ) const
    {
        m_dt = a_dt;
        m_linear_function->curTimeStep( a_dt );
    }

    void PrintParams () const override
    {
        amrex::Print()     << "Newton verbose:             " << (this->m_verbose?"true":"false") << "\n";
        amrex::Print()     << "Newton max iterations:      " << m_maxits << "\n";
        amrex::Print()     << "Newton relative tolerance:  " << m_rtol << "\n";
        amrex::Print()     << "Newton absolute tolerance:  " << m_atol << "\n";
        amrex::Print()     << "Newton require convergence: " << (m_require_convergence?"true":"false") << "\n";
        amrex::Print()     << "GMRES verbose:            " << m_gmres_verbose_int << "\n";
        amrex::Print()     << "GMRES restart length:     " << m_gmres_restart_length << "\n";
        amrex::Print()     << "GMRES max iterations:     " << m_gmres_maxits << "\n";
        amrex::Print()     << "GMRES relative tolerance: " << m_gmres_rtol << "\n";
        amrex::Print()     << "GMRES absolute tolerance: " << m_gmres_atol << "\n";
        amrex::Print()     << "Preconditioner type:      " << amrex::getEnumNameString(m_pc_type) << "\n";

        m_linear_function->printParams();
    }

private:

    /**
     * \brief Intermediate Vec containers used by the solver.
     */
    mutable Vec m_dU, m_F, m_R;

    /**
     * \brief Pointer to Ops class.
     */
    Ops* m_ops = nullptr;

    /**
     * \brief Flag to determine whether convergence is required.
     */
    bool m_require_convergence = true;

    /**
     * \brief Relative tolerance for the Newton solver.
     */
    amrex::Real m_rtol = 1.0e-6;

    /**
     * \brief Absolute tolerance for the Newton solver.
     */
    amrex::Real m_atol = 0.;

    /**
     * \brief Maximum iterations for the Newton solver.
     */
    int m_maxits = 100;

    /**
     * \brief Relative tolerance for GMRES.
     */
    amrex::Real m_gmres_rtol = 1.0e-4;

    /**
     * \brief Absolute tolerance for GMRES.
     */
    amrex::Real m_gmres_atol = 0.;

    /**
     * \brief Maximum iterations for GMRES.
     */
    int m_gmres_maxits = 1000;

    /**
     * \brief Verbosity flag for GMRES.
     */
    int m_gmres_verbose_int = 2;

    /**
     * \brief Restart iteration for GMRES.
     */
    int m_gmres_restart_length = 30;

    /**
     * \brief Preconditioner type
     */
    PreconditionerType m_pc_type = PreconditionerType::none;

    mutable amrex::Real m_cur_time, m_dt;

    /**
     * \brief The linear function used by GMRES to compute A*v.
     * In the contect of JFNK, A = dF/dU (i.e., system Jacobian)
     */
    std::unique_ptr<JacobianFunctionMF<Vec,Ops>> m_linear_function;

    /**
     * \brief The linear solver (GMRES) object.
     */
    std::unique_ptr<amrex::GMRES<Vec,JacobianFunctionMF<Vec,Ops>>> m_linear_solver;

    void ParseParameters ();

    /**
     * \brief Compute the nonlinear residual: F(U) = U - b - R(U).
     */
    void EvalResidual ( Vec&         a_F,
                  const Vec&         a_U,
                  const Vec&         a_b,
                        amrex::Real  a_time,
                        int          a_iter ) const;

};

template <class Vec, class Ops>
void NewtonSolver<Vec,Ops>::Define ( const Vec&  a_U,
                                     Ops*        a_ops )
{
    WARPX_ALWAYS_ASSERT_WITH_MESSAGE(
        !this->m_is_defined,
        "Newton nonlinear solver object is already defined!");

    ParseParameters();

    m_dU.Define(a_U);
    m_F.Define(a_U); // residual function F(U) = U - b - R(U) = 0
    m_R.Define(a_U); // right hand side function R(U)

    m_ops = a_ops;

    m_linear_function = std::make_unique<JacobianFunctionMF<Vec,Ops>>();
    m_linear_function->define(m_F, m_ops, m_pc_type);

    m_linear_solver = std::make_unique<amrex::GMRES<Vec,JacobianFunctionMF<Vec,Ops>>>();
    m_linear_solver->define(*m_linear_function);
    m_linear_solver->setVerbose( m_gmres_verbose_int );
    m_linear_solver->setRestartLength( m_gmres_restart_length );
    m_linear_solver->setMaxIters( m_gmres_maxits );

    this->m_is_defined = true;

    // Create diagnostic file and write header
    if (amrex::ParallelDescriptor::IOProcessor()
        && !this->m_diagnostic_file.empty()
        && !amrex::FileExists(this->m_diagnostic_file)) {

        std::filesystem::path const diagnostic_path(this->m_diagnostic_file);
        std::filesystem::path const diagnostic_dir = diagnostic_path.parent_path();
        if (!diagnostic_dir.empty()) {
            std::filesystem::create_directories(diagnostic_dir);
        }

        std::ofstream diagnostic_file{this->m_diagnostic_file, std::ofstream::out | std::ofstream::trunc};
        int c = 0;
        diagnostic_file << "#";
        diagnostic_file << "[" << c++ << "]step()";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]time(s)";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]iters";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]norm_abs";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]norm_rel";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]gmres_iters";
        diagnostic_file << " ";
        diagnostic_file << "[" << c++ << "]gmres_last_res";
        diagnostic_file << "\n";
        diagnostic_file.close();
    }
}

template <class Vec, class Ops>
void NewtonSolver<Vec,Ops>::ParseParameters ()
{
    const amrex::ParmParse pp_newton("newton");
    pp_newton.query("verbose",             this->m_verbose);
    pp_newton.query("absolute_tolerance",  m_atol);
    pp_newton.query("relative_tolerance",  m_rtol);
    pp_newton.query("max_iterations",      m_maxits);
    pp_newton.query("require_convergence", m_require_convergence);
    pp_newton.query("diagnostic_file",     this->m_diagnostic_file);
    pp_newton.query("diagnostic_interval", this->m_diagnostic_interval);

    const amrex::ParmParse pp_gmres("gmres");
    pp_gmres.query("verbose_int",         m_gmres_verbose_int);
    pp_gmres.query("restart_length",      m_gmres_restart_length);
    pp_gmres.query("absolute_tolerance",  m_gmres_atol);
    pp_gmres.query("relative_tolerance",  m_gmres_rtol);
    pp_gmres.query("max_iterations",      m_gmres_maxits);

    const amrex::ParmParse pp_jac("jacobian");
    pp_jac.query("pc_type", m_pc_type);
}

template <class Vec, class Ops>
void NewtonSolver<Vec,Ops>::Solve ( Vec&         a_U,
                              const Vec&         a_b,
                                    amrex::Real  a_time,
                                    amrex::Real  a_dt,
                                    int          a_step) const
{
    BL_PROFILE("NewtonSolver::Solve()");
    WARPX_ALWAYS_ASSERT_WITH_MESSAGE(
        this->m_is_defined,
        "NewtonSolver::Solve() called on undefined object");
    using namespace amrex::literals;

    //
    // Newton routine to solve nonlinear equation of form:
    // F(U) = U - b - R(U) = 0
    //

    CurTime(a_time);
    CurTimeStep(a_dt);

    amrex::Real norm_abs = 0.;
    amrex::Real norm0 = 1._rt;
    amrex::Real norm_rel = 0.;

    int iter;
    int linear_solver_iters = 0;
    for (iter = 0; iter < m_maxits;) {

        // Compute residual: F(U) = U - b - R(U)
        EvalResidual(m_F, a_U, a_b, a_time, iter);

        // Compute norm of the residual
        norm_abs = m_F.norm2();
        if (iter == 0) {
            if (norm_abs > 0.) { norm0 = norm_abs; }
            else { norm0 = 1._rt; }
        }
        norm_rel = norm_abs/norm0;

        // Check for convergence criteria
        if (this->m_verbose || iter == m_maxits) {
            amrex::Print() << "Newton: iteration = " << std::setw(3) << iter <<  ", norm = "
                           << std::scientific << std::setprecision(5) << norm_abs << " (abs.), "
                           << std::scientific << std::setprecision(5) << norm_rel << " (rel.)" << "\n";
        }

        if (norm_abs < m_rtol) {
            amrex::Print() << "Newton: exiting at iteration = " << std::setw(3) << iter
                           << ". Satisfied absolute tolerance " << m_atol << "\n";
            break;
        }

        if (norm_rel < m_rtol) {
            amrex::Print() << "Newton: exiting at iteration = " << std::setw(3) << iter
                           << ". Satisfied relative tolerance " << m_rtol << "\n";
            break;
        }

        if (norm_abs > 100._rt*norm0) {
            amrex::Print() << "Newton: exiting at iteration = " << std::setw(3) << iter
                           << ". SOLVER DIVERGED! relative tolerance = " << m_rtol << "\n";
            std::stringstream convergenceMsg;
            convergenceMsg << "Newton: exiting at iteration " << std::setw(3) << iter <<
                              ". SOLVER DIVERGED! absolute norm = " << norm_abs <<
                              " has increased by 100X from that after first iteration.";
            WARPX_ABORT_WITH_MESSAGE(convergenceMsg.str());
        }

        // Solve linear system for Newton step [Jac]*dU = F
        m_dU.zero();
        m_linear_solver->solve( m_dU, m_F, m_gmres_rtol, m_gmres_atol );
        linear_solver_iters += m_linear_solver->getNumIters();

        // Update solution
        a_U -= m_dU;

        iter++;
        if (iter >= m_maxits) {
            amrex::Print() << "Newton: exiting at iter = " << std::setw(3) << iter
                           << ". Maximum iteration reached: iter = " << m_maxits << "\n";
            break;
        }

    }

    if (m_rtol > 0. && iter == m_maxits) {
       std::stringstream convergenceMsg;
       convergenceMsg << "Newton solver failed to converge after " << iter <<
                         " iterations. Relative norm is " << norm_rel <<
                         " and the relative tolerance is " << m_rtol <<
                         ". Absolute norm is " << norm_abs <<
                         " and the absolute tolerance is " << m_atol;
       if (this->m_verbose) { amrex::Print() << convergenceMsg.str() << "\n"; }
       if (m_require_convergence) {
           WARPX_ABORT_WITH_MESSAGE(convergenceMsg.str());
       } else {
           ablastr::warn_manager::WMRecordWarning("NewtonSolver", convergenceMsg.str());
       }
    }

    if (!this->m_diagnostic_file.empty() && amrex::ParallelDescriptor::IOProcessor() &&
        ((a_step+1)%this->m_diagnostic_interval==0 || a_step==0)) {
        std::ofstream diagnostic_file{this->m_diagnostic_file, std::ofstream::out | std::ofstream::app};
        diagnostic_file << std::setprecision(14);
        diagnostic_file << a_step+1;
        diagnostic_file << " ";;
        diagnostic_file << a_time + a_dt;
        diagnostic_file << " ";;
        diagnostic_file << iter;
        diagnostic_file << " ";;
        diagnostic_file << norm_abs;
        diagnostic_file << " ";;
        diagnostic_file << norm_rel;
        diagnostic_file << " ";;
        diagnostic_file << linear_solver_iters;
        diagnostic_file << " ";;
        diagnostic_file << m_linear_solver->getResidualNorm();
        diagnostic_file << "\n";
        diagnostic_file.close();
    }

}

template <class Vec, class Ops>
void NewtonSolver<Vec,Ops>::EvalResidual ( Vec&         a_F,
                                     const Vec&         a_U,
                                     const Vec&         a_b,
                                           amrex::Real  a_time,
                                           int          a_iter ) const
{

    m_ops->ComputeRHS( m_R, a_U, a_time, a_iter, false );

    // set base U and R(U) for matrix-free Jacobian action calculation
    m_linear_function->setBaseSolution(a_U);
    m_linear_function->setBaseRHS(m_R);

    // update preconditioner
    m_linear_function->updatePreCondMat(a_U);

    // Compute residual: F(U) = U - b - R(U)
    a_F.Copy(a_U);
    a_F -= m_R;
    a_F -= a_b;

}

#endif

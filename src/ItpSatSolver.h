#ifndef _ITPSATSOLVER_H_
#define _ITPSATSOLVER_H_

#include "boost/noncopyable.hpp"
#include "boost/logic/tribool.hpp"

#include "avy/Util/AvyDebug.h"
#include "avy/Util/Global.h"
#include "AigUtils.h"

#include "sat/bsat/satSolver.h"

#include <vector>

namespace avy
{
  using namespace abc;
  
  typedef std::vector<lit> LitVector;
  
  
  /// Thin wrapper around interpolating sat solver
  class ItpSatSolver : boost::noncopyable
  {
  private:
    sat_solver *m_pSat;
    
    /// true if last result was trivial
    bool m_Trivial;

    /// number of partitions
    unsigned m_nParts;

    /// current state
    boost::tribool m_State;

  private:
    boost::tribool tbool (int v)
    {
      switch (v)
        {
        case l_False: return false;
        case l_True: return true;
        case l_Undef: return boost::indeterminate;
        default: AVY_UNREACHABLE();
        }
    }
    
  public:
    /// create a solver with nParts partitions and nVars variables
    ItpSatSolver (unsigned nParts, unsigned nVars) : m_pSat (0) 
    { reset (nParts, nVars); }
    
    ~ItpSatSolver ()
    {
      if (m_pSat) sat_solver_delete (m_pSat);
      m_pSat = NULL;
    }

    /// reset and allocate nParts partitions and nVars variables
    void reset (unsigned nParts, unsigned nVars)
    {
      AVY_ASSERT (nParts >= 2 && "require at least 2 partitions");
      if (m_pSat) sat_solver_delete (m_pSat);
      m_nParts = nParts;
      m_Trivial = false;
      m_State = boost::indeterminate;
      m_pSat = sat_solver_new ();
      // -- allocate space for nParts-1 interpolants
      sat_solver_store_alloc (m_pSat, nParts-1);
      sat_solver_setnvars (m_pSat, nVars);
    }

    /// Mark currently unmarked clauses as belonging to partition nPart
    void markPartition (unsigned nPart)
    { 
      AVY_ASSERT (nPart < m_nParts);
      sat_solver_store_mark_clauses_a (m_pSat, nPart); 
    }

    /// add a clause
    bool addClause (lit* begin, lit* end)
    {
      m_Trivial = !sat_solver_addclause (m_pSat, begin, end); 
      LOG("sat", logs () << "NEW CLS: ";
          for (lit *it = begin; it != end; ++it)
            logs () << *it << " ";
          logs () << "\n";);
      return !m_Trivial;
    }
    
    void dumpCnf (std::string fname) 
    { sat_solver_store_write (m_pSat, const_cast<char*>(fname.c_str ())); }
    

    /// raw access to the sat solver
    sat_solver* get () { return m_pSat; }
    
    /// true if the context is decided 
    bool isSolved () { return m_Trivial || m_State || !m_State; }

    /// decide context with assumptions
    boost::tribool solve (LitVector &assumptions)
    {
      if (isTrivial ()) return false;
      if (!assumptions.empty ()) m_State = boost::indeterminate;
      
      if (isSolved ()) return m_State;
      
      AVY_ASSERT (m_pSat != NULL);
      
      sat_solver_store_mark_roots( m_pSat );
      lit *beg = NULL;
      if (!assumptions.empty ()) beg = &assumptions[0];
      int RetValue = sat_solver_solve( m_pSat, beg,  beg + assumptions.size (),
                                       (ABC_INT64_T)10000000, (ABC_INT64_T)0, 
                                       (ABC_INT64_T)0, (ABC_INT64_T)0 );
      
      m_State = tbool (RetValue);
      return m_State;
      
    }

    /// a pointer to the unsat core
    int core (int **out) { return sat_solver_final (m_pSat, out); }
    
    /// decide current context
    boost::tribool solve ()
    {
      LitVector v;
      return solve (v);
    }

    bool isTrivial () { return m_Trivial; }
    

    /// Compute an interpolant. User provides the list of shared variables
    /// Variables can only be shared between adjacent partitions.
    /// fMcM == true for McMillan, and false for McMillan'
    Aig_Man_t* getInterpolant (Vec_Int_t** vSharedVars, bool fMcM=true);
  };
  
}



#endif /* _ITPSATSOLVER_H_ */
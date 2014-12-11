#include "ItpSatSolver.h"
#include "misc/vec/vec.h"
#include "sat/bsat/satStore.h"

#include "AigPrint.h"
#include "avy/Util/Global.h" 
#include "avy/Util/Stats.h" 
namespace avy
{
  /// Compute an interpolant. User provides the list of shared variables
  /// Variables can only be shared between adjacent partitions.
  /// fMcM == true for McMillan, and false for McMillan'
  Aig_Man_t* ItpSatSolver::getInterpolant (std::vector<Vec_Int_t*> &vSharedVars, int nNumOfVars,
                                           bool fMcM)
  {
    AVY_MEASURE_FN;
    AVY_ASSERT (!isTrivial ());
    AVY_ASSERT (m_pSat != NULL);
    AVY_ASSERT (vSharedVars.size () >= m_nParts - 1);
    
    AVY_VERIFY (!solve ());
    
    Sto_Man_t* pSatCnf = (Sto_Man_t*) sat_solver_store_release( m_pSat );
    sat_solver_delete( m_pSat );
    m_pSat = NULL;
      

    // Create the interpolating manager and extract the interpolation-seq
    // with m_nParts-1 interpolants
    Ints_Man_t* pManInter = Ints_ManAlloc(m_nParts-1, gParams.itp);
    // XXX how to wire fMcM properly?
    Aig_Man_t* pMan = (Aig_Man_t *) Ints_ManInterpolate( pManInter,
                                                         pSatCnf, 
                                                         0, (void**)&vSharedVars[0],
                                                         0 );
    Ints_ManFree( pManInter );

    VERBOSE(2, Aig_ManPrintStats(pMan););
    LOG("itp_verbose", logs () << *pMan << "\n";);


    // Release memory
    Sto_ManFree( pSatCnf );
    return pMan;
  }
  
}
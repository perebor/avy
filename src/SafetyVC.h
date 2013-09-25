#ifndef _SAFETYVC_H_
#define _SAFETYVC_H_

#include "AigUtils.h"
#include "ItpSatSolver.h"
#include "sat/cnf/cnf.h"
#include "aig/saig/saig.h"

namespace avy
{
  /// smart pointer for Cnf_Dat_t. 
  typedef boost::shared_ptr<abc::Cnf_Dat_t> CnfPtr;
  namespace 
  { 
    struct cnf_deleter
    { void operator() (abc::Cnf_Dat_t* p) { if (p) abc::Cnf_DataFree (p); } };
  }
  
  inline CnfPtr cnfPtr (abc::Cnf_Dat_t *p) { return CnfPtr (p, cnf_deleter()); }

  /// Lifts Cnf for the lifetime of the instance
  class ScoppedCnfLift 
  {
    CnfPtr &m_Cnf;
    int m_nOffset;
               
  public:
    ScoppedCnfLift (CnfPtr &cnf, int nOffset) : m_Cnf(cnf), m_nOffset(nOffset)
    { Cnf_DataLift (&*m_Cnf, m_nOffset); }
    ~ScoppedCnfLift () { Cnf_DataLift (&*m_Cnf, -m_nOffset); }
  };
  

  /**
   * Safety Verification Condition: encodes Init & Tr^i & Bad
   * Tr is given by an Aig with a single Po
   * Bad is the driver of the Po of Tr
   * Init is zero for all registers
   */
  class SafetyVC
  {

    /// the original circuit
    AigManPtr m_Circuit;
    /// transition relation part of the circuit
    AigManPtr m_Tr;  
    /// the bad state
    AigManPtr m_Bad;

    /// Cnf of Tr
    CnfPtr m_cnfTr;
    
    /// Cnf of Bad sates
    CnfPtr m_cnfBad;

    /// initialize given a circuit
    void init (abc::Aig_Man_t *pCircuit);

  public:
    /// create a VC given from a circuit. Init is implicit.
    SafetyVC(abc::Aig_Man_t *pCircuit) { init (pCircuit); }
    

    /// number of Cnf variables needed for the Tr of nFrame
    unsigned trVarSize (unsigned nFrame) { return m_cnfTr->nVars; }
    
    /// number of Cnf variables for Bad
    unsigned badVarSize () { return m_cnfBad->nVars; }

    unsigned trGlueSize (unsigned nFrame) { return 0; }
    unsigned badGlueSize () { return 0; }

    /// number of Cnf variables for frames nStart up to, but not including nStop
    unsigned varSize (unsigned nStart, unsigned nStop, bool fWithBad)
    {
      unsigned nVars = 0;
      for (unsigned i = nStart; i  < nStop; ++i) 
        nVars += trGlueSize (i) + trVarSize (i);
      if (fWithBad) nVars += badGlueSize () + badVarSize ();
      return nVars;
    }
    
    /// Add Cnf for the glue between frame nFrame and nFrame+1
    /// \param nTrOffset is the offset at which Tr of nFrame was added
    /// \param nFreshVars is the offset from which new Cnf vars can be allocated
    /// \return new offset for Cnf vars. Should be nNewOffset if glue 
    /// does not consume vars
    template<typename S>
    unsigned addTrGlue (S &solver, unsigned nFrame, 
                        unsigned nTrOffset, unsigned nFreshVars);
    
    template<typename S>
    unsigned addBadGlue (S &solver, unsigned nTrOffset, unsigned nFreshVars);
    
    /** Add Cnf of one Tr to the solver
     *
     * Cnf for Frame0 is Init&Tr
     * Cnf for all other frames is Tr
     *
     * Suggested usage
     * unsigned nOffset = 0; for (i = 0 to N) nOffset += addTrCnf (i); nOffset += addBadCnf (nOffset)
     *
     * \param solver  Sat solver
     * \param nFrame frame to add. 0 means initial
     * \param nOffset offset to allocate CNF variables from
     * \return next free Cnf variable
     */
    template <typename S>
    unsigned addTrCnf (S &solver, unsigned nFrame, unsigned nOffset);
    template <typename S>
    unsigned addBadCnf (S &solver, unsigned nOffset);
  };


  template <typename S> unsigned SafetyVC::addTrGlue (S &solver, unsigned nFrame, 
                                                      unsigned nTrOffset, 
                                                      unsigned nFreshVars)
  {
    logs () << "addTrGlue: old: " << nTrOffset << " new: " << nFreshVars << "\n";
    int i;
    Aig_Obj_t *pLo, *pLi;
    lit Lits[2];
    
    Saig_ManForEachLo (&*m_Tr, pLo, i)
      {
        pLi = Saig_ManLi (&*m_Tr, i);
        int liVar = m_cnfTr->pVarNums [pLi->Id] + nTrOffset;
        int loVar = m_cnfTr->pVarNums [pLo->Id] + nFreshVars;
        
        // -- add equality constraints
        Lits [0] = toLitCond (liVar, 0);
        Lits [1] = toLitCond (loVar, 1);
        solver.addClause (Lits, Lits+2);
        
        Lits [0] = lit_neg (Lits [0]);
        Lits [1] = lit_neg (Lits [1]);
        solver.addClause (Lits, Lits+2);
        
      }

    return nFreshVars;
  }

  /** glue bad state*/
  template<typename S>
  unsigned SafetyVC::addBadGlue (S &solver, unsigned nTrOffset, unsigned nFreshVars)
  {
    logs () << "addBadGlue: old: " << nTrOffset << " new: " << nFreshVars << "\n";
    int i;
    Aig_Obj_t *pCi, *pLi;
    lit Lits[2];
    
    Saig_ManForEachLi (&*m_Tr, pLi, i)
      {

        int liVar = m_cnfTr->pVarNums [pLi->Id] + nTrOffset;
        
        pCi = Aig_ManCi (&*m_Bad, Saig_ManPiNum (&*m_Tr) + i);
        int ciVar = m_cnfBad->pVarNums [pCi->Id] + nFreshVars;
        
        // -- add equality constraints
        Lits [0] = toLitCond (liVar, 0);
        Lits [1] = toLitCond (ciVar, 1);
        solver.addClause (Lits, Lits+2);
        
        Lits [0] = lit_neg (Lits [0]);
        Lits [1] = lit_neg (Lits [1]);
        solver.addClause (Lits, Lits+2);
        
      }

    return nFreshVars;
  }
  


  //  addGlueCnf (solver, nFrame, nOffset, nOffset + trVarSize (nFrame));
  template <typename S>
  unsigned SafetyVC::addTrCnf (S &solver, unsigned nFrame, unsigned nOffset)
  {
    // add clauses for Init
    if (nFrame == 0)
      {
        logs () << "addTrCnf:Init\n";
        
        Aig_Obj_t *pObj;
        int i;
        lit Lits[1];
        
        Saig_ManForEachLo (&*m_Tr, pObj, i)
          {
            Lits[0] = toLitCond (m_cnfTr->pVarNums [pObj->Id] + nOffset, 1);
            solver.addClause (Lits, Lits + 1);
          }
      }

    {
      logs () << "addTrCnf\n";
      ScoppedCnfLift scLift (m_cnfTr, nOffset);

      // -- add clauses
      for (int i = 0; i < m_cnfTr->nClauses; ++i)
        AVY_VERIFY (solver.addClause (m_cnfTr->pClauses [i], m_cnfTr->pClauses[i+1]));
    }

    return nOffset + trVarSize (nFrame);
  }

  
  template <typename S>
  unsigned SafetyVC::addBadCnf (S &solver, unsigned nOffset)
  {
    logs () << "addBadCnf: off: " << nOffset << "\n";
    
    ScoppedCnfLift scLift (m_cnfBad, nOffset);
    // -- add clauses
    for (int i = 0; i < m_cnfBad->nClauses; ++i)
      AVY_VERIFY (solver.addClause (m_cnfBad->pClauses [i], 
                                    m_cnfBad->pClauses[i+1]));

    Aig_Obj_t *pPo = Aig_ManCo (&*m_Bad, 0);
    int poVar = m_cnfBad->pVarNums [pPo->Id];
    lit Lit = toLit (poVar);
    solver.addClause (&Lit, &Lit + 1);

    return nOffset + badVarSize ();
  }
}



#endif /* _SAFETYVC_H_ */
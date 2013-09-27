#ifndef _AVYMAIN_H_
#define _AVYMAIN_H_

#include "AigUtils.h"
#include "SafetyVC.h"
#include "ItpSatSolver.h"
#include "ItpSatSolver2.h"
#include "Pdr.h"
#include "Unroller.h"
#include <string>
#include "boost/logic/tribool.hpp"
#include "boost/foreach.hpp"
#include "boost/dynamic_bitset.hpp"

#include "boost/noncopyable.hpp"

namespace avy
{
  class AvyMain : boost::noncopyable
  {
    std::string m_fName;
    
    AigManPtr m_Aig;

    /** reference to the current VC */
    SafetyVC *m_Vc;
    
    /** refernece to the current Sat solver */
    ItpSatSolver m_Solver;
    Unroller<ItpSatSolver> m_Unroller;

    Pdr *m_pPdr;
    
    dynamic_bitset<> m_Core;
    
  public:
    AvyMain(std::string fname);
    
    virtual ~AvyMain() { if (m_pPdr) delete m_pPdr; }

    int run ();

    /// do BMC check up to depth nFrame
    boost::tribool doBmc (unsigned nFrame);
    /// convert interpolant into PDR trace
    boost::tribool doPdrTrace (AigManPtr itp);
    /// strengthen VC with current Pdr trace
    void doStrengthenVC ();
    
    bool validateItp (AigManPtr itp);
    boost::tribool solveWithCore (unsigned nFrame);
  };
}


#endif /* _AVYMAIN_H_ */







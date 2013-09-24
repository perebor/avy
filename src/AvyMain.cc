#include "AvyMain.h"
#include "boost/lexical_cast.hpp"
#include "avy/Util/Global.h"
#include "SafetyVC.h"

#include "base/main/main.h"
#include "aig/ioa/ioa.h"

using namespace boost;
using namespace std;
using namespace abc;
using namespace avy;



namespace abc
{
  extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, 
                                   int fRegisters );
}

static Aig_Man_t *loadAig (std::string fname)
{
  Abc_Frame_t *pFrame = Abc_FrameGetGlobalFrame ();
    
  VERBOSE (2, vout () << "\tReading AIG from '" << fname << "'\n";);
  string cmd = "read " + fname;
  Cmd_CommandExecute (pFrame, cmd.c_str ());
    
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk (pFrame);
    
  return Abc_NtkToDar (pNtk, 0, 1);
}


namespace avy
{
  AvyMain::AvyMain (std::string fname) : m_fName (fname), m_Vc (0), m_Solver(2, 2)
  {
    VERBOSE (2, vout () << "Starting ABC\n");
    Abc_Start ();
    
    Aig_Man_t *pAig1 = loadAig (fname);
    
    VERBOSE (2, vout () << "\tAdding reset signal\n");
    Aig_Man_t *pAig2 = Aig_AddResetPi (pAig1);
    Aig_ManStop (pAig1);
    pAig1 = NULL;
    
    string tmpName = "__tmp.aig";
    VERBOSE (2, vout () << "\tDumping to disk: " << tmpName << "\n");
    Ioa_WriteAiger (pAig2, const_cast<char*>(tmpName.c_str ()), 1, 0);

    VERBOSE(2, vout () << "Restarting ABC\n");
    Abc_Stop ();
    Abc_Start ();

    m_Aig = aigPtr (loadAig (tmpName));

    // -- keep abc running, just in case
    //Abc_Stop ()
    
    
  }  

  int AvyMain::run ()
  {
    SafetyVC vc (&*m_Aig);
    m_Vc = &vc;

    unsigned nMaxFrames = 100;
    for (unsigned nFrame = 0; nFrame < 100; ++nFrame)
      {
        tribool res = doBmc (nFrame);
        if (res)
          {
            VERBOSE (0, vout () << "SAT from BMC at frame: " << nFrame << "\n";);
            return 1;
          }
        else if (!res)
          {
            VERBOSE(0, vout () << "UNSAT from BMC at frame: " << nFrame << "\n";);
            
          }
        else 
          {
            VERBOSE (0, vout () << "UNKNOWN from BMC at frame: " 
                     << nFrame << "\n";);
            return 2;
          }
      }
    
    
    return 0;
  }

  tribool AvyMain::doBmc (unsigned nFrame)
  {
    m_Solver.reset (nFrame + 2, m_Vc->varSize (0, nFrame, true));
    
    unsigned nOffset = 0;
    unsigned nLastOffset = 0;
    
    for (unsigned i = 0; i <= nFrame; ++i)
      {
        nLastOffset = nOffset;
        nOffset = m_Vc->addTrCnf (m_Solver, i, nOffset);
        m_Solver.markPartition (i);

        if (i < nFrame) nOffset = m_Vc->addTrGlue (m_Solver, i, nLastOffset, nOffset);
        m_Solver.dumpCnf ("frame" + lexical_cast<string>(nFrame) + ".cnf");
      }

    nOffset = m_Vc->addBadGlue (m_Solver, nLastOffset, nOffset);
    nOffset = m_Vc->addBadCnf (m_Solver, nOffset);
    m_Solver.markPartition (nFrame + 1);
    m_Solver.dumpCnf ("frame" + lexical_cast<string>(nFrame+1) + ".cnf");

    LitVector assumps;
    return m_Solver.solve (assumps);
  }
  
  

}

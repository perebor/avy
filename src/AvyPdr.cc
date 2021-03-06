#include <string>

#include "boost/noncopyable.hpp"
#include "boost/logic/tribool.hpp"
#include "boost/logic/tribool_io.hpp"
#include "boost/program_options.hpp"
namespace po = boost::program_options;

#include "AigUtils.h"
#include "avy/Util/Global.h"
#include "avy/Util/AvyDebug.h"

#include "base/main/main.h"
#include "aig/ioa/ioa.h"
#include "avy/Util/Stats.h"
#include "Pdr.h"

using namespace boost;
using namespace avy;
using namespace avy::abc;

namespace ABC_NAMESPACE
{
  extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
  extern int Abc_CommandPdr( Abc_Frame_t * pAbc, int argc, char ** argv );
}

namespace avy
{
  
  class AvyPdr : boost::noncopyable
  {
    std::string m_sFName;
  public:
    AvyPdr (std::string fname): m_sFName (fname) { Abc_Start (); }
    ~AvyPdr () { Abc_Stop (); }
    
    
    tribool run ()
    {
      AVY_MEASURE_FN;
      Stats::set("Result", "UNKNOWN");
      VERBOSE (1, Stats::PrintBrunch (vout ()););
      
      std::string &fname = m_sFName;
      Abc_Frame_t *pAbc = Abc_FrameGetGlobalFrame ();
      VERBOSE (2, vout () << "\tReading AIG from '" << fname << "'\n";);
      std::string cmd = "&r " + fname;
      Cmd_CommandExecute (pAbc, cmd.c_str ());
      Cmd_CommandExecute (pAbc, "&put");

      Abc_Ntk_t *pNtk = Abc_FrameReadNtk (pAbc);
      Pdr pdr (Abc_NtkToDar (pNtk, 0, 1));
      VERBOSE (2, pdr.setVerbose (true););
      boost::tribool res = pdr.solve ();
      
      Stats::uset ("Frames", pdr.maxFrames ());
      VERBOSE (1, vout () << "Result: " << std::boolalpha << res << "\n");
      if (!res) Stats::set ("Result", "SAT");
      else if (res) Stats::set ("Result", "UNSAT");
      
      VERBOSE(1, Stats::PrintBrunch (vout ()););
      return res;
    }
  };
}

static std::string parseCmdLine (int argc, char**argv)
{
  po::options_description generic("Options");
  generic.add_options()
    ("help", "print help message")
    ("log,L", po::value< std::vector<std::string> >(), "log levels to enable")
    ("verbose,v", po::value<unsigned> (&gParams.verbosity)->default_value(0),
     "Verbosity level: 0 means silent");


  po::options_description hidden("Hidden options");
  hidden.add_options()
    ("input-file", po::value< std::string >(), "input file");        

  po::options_description cmdline;
  cmdline.add (generic).add (hidden);  

  po::positional_options_description p;
  p.add("input-file", -1);

  try
    {
      po::variables_map vm;
      po::store(po::command_line_parser(argc, argv).
                options(cmdline).positional(p).run(), vm);
      po::notify(vm);

      if (vm.count("help")) {
        outs () << generic << "\n";
        std::exit (1);
      }
  

      if (!vm.count("input-file"))
        {
          outs () << "Error: No AIG file specified\n";
          std::exit (1);
        }

      if (vm.count("log"))
        {
          using namespace std;
          vector<string> logs = vm["log"].as< vector<string> > ();
          for (vector<string>::iterator it = logs.begin (), end = logs.end (); 
               it != end; ++it)
            AvyEnableLog (it->c_str ());
        }

      gParams.fName = vm["input-file"].as< std::string > ();
      return gParams.fName;
    }
  catch (std::exception &e)
    {
      outs () << "Error: " << e.what () << "\n";
      std::exit (1);
    }
}

  
int main (int argc, char** argv)
{
  std::string fileName = parseCmdLine (argc, argv);
  AvyPdr pdr (fileName);
  tribool res = pdr.run ();
  if (res) return 1;
  else if (!res) return 0;
  else return 2;
}


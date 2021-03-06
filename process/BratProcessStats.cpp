/*
* 
*
* This file is part of BRAT
*
* BRAT is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* BRAT is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <fstream>

#include "common/tools/Exception.h"
#include "FileParams.h"

#include "BratProcess.h"
#include "BratProcessStats.h"

// When debugging changes all calls to "new" to be calls to "DEBUG_NEW" allowing for memory leaks to
// give you the file name and line number where it occurred.
// Needs to be included after all #include commands
#include "Win32MemLeaksAccurate.h"

using namespace brathl;
using namespace processes;

namespace processes
{

const KeywordHelp CBratProcessStats::StatsKeywordList[]	= {
	KeywordHelp(kwFILE,		1, 0),
	KeywordHelp(kwRECORD),
	KeywordHelp(kwOUTPUT),
	KeywordHelp(kwSELECT,		0, 0, "1"),
	KeywordHelp(kwFIELD,		1, CBratProcess::NB_MAX_Y, NULL, 24),
	KeywordHelp(kwFIELD_UNIT,	-24),
	KeywordHelp(kwFIELD_TITLE,	-24),
	KeywordHelp(kwALIAS_NAME,	0, 0, "None", 14),
	KeywordHelp(kwALIAS_VALUE,	-14),
	KeywordHelp(kwUNIT_ATTR_NAME,	0, 0, "None", 14),
	KeywordHelp(kwUNIT_ATTR_VALUE,	-14),
	KeywordHelp(kwVERBOSE,		0, 1, "0"),
 	KeywordHelp(kwLOGFILE,		0, 1, ""),
	KeywordHelp(""),
};

//-------------------------------------------------------------
//------------------- CBratProcessStats class --------------------
//-------------------------------------------------------------

CBratProcessStats::CBratProcessStats()
{
  Init();
}
//----------------------------------------
    
CBratProcessStats::~CBratProcessStats()
{

}
//----------------------------------------
void CBratProcessStats::Init()
{
  //m_nbMaxDataSlices = 1;

}
//----------------------------------------
void CBratProcessStats::ResizeArrayDependOnFields(uint32_t size)
{
  CBratProcess::ResizeArrayDependOnFields(size);

}

//----------------------------------------
bool CBratProcessStats::Initialize(std::string& msg)
{
    UNUSED(msg);

  //CTrace *p =
          CTrace::GetInstance();

  // Register Brat algorithms
  CBratAlgorithmBase::RegisterCAlgorithms();

  // Load aliases dictionnary
  std::string errorMsg;
  CAliasesDictionary::LoadAliasesDictionary(&errorMsg, false);
  if (!(errorMsg.empty())) 
  {
    std::string msg = CTools::Format("WARNING: %s",  errorMsg.c_str());
    CTrace::Tracer(1, msg);
  }

  GetParameters();
  return true;
}

//----------------------------------------
void CBratProcessStats::GetParameters(const std::string& commandFileName)
{
  m_commandFileName = commandFileName;
  GetParameters();
}
//----------------------------------------
void CBratProcessStats::GetParameters()
{
  LoadParams(m_commandFileName);
  CheckFileParams();

  CFileParams& params = *m_fileParams;
  CUnit	unit;

// Verify keyword occurences
  uint32_t nbFields		= params.CheckCount(kwFIELD,  1, CBratProcess::NB_MAX_Y);

  params.CheckCount(kwRECORD);


// Get keyword values

  //CTrace *p =
  CTrace::GetInstance();

  //---------
  
  if (params.CheckCount(kwOUTPUT, 0) == 1)
  {
    params.m_mapParam[kwOUTPUT]->GetValue(m_outputFileName);
    CDate date;
    date.SetDateNow();

    //m_outputFileName.append(date.AsString("%Y%m%d%H%M%S"));

    CTrace::Tracer(1, PCT_StrFmt, "Output file", m_outputFileName.c_str());
  }
  else
  {
    CTrace::Tracer(1, PCT_StrFmt, "Output file", "stdout");
  }

  //---------
  
  params.m_mapParam[kwRECORD]->GetValue(m_recordName);
  CTrace::Tracer(1, PCT_StrFmt, "Data set name", m_recordName.c_str());

  //---------

  if (params.CheckCount(kwEXPAND_ARRAY, 0, 1) == 1)
  {
    params.m_mapParam[kwEXPAND_ARRAY]->GetValue(m_expandArray);
  }
  CTrace::Tracer(1, PCT_IntFmt, "Expand array", m_expandArray);


  //---------
  
  GetSelectParameter(params);
   
  //m_select.Dump(*(CTrace::GetInstance()->GetDumpContext()));


  ResizeArrayDependOnFields(nbFields);

  uint32_t index;

  for (index = 0; index < nbFields; index++)
  {
    this->GetDefinition(params,
	      kwFIELD,
	      m_fields[index],
	      NULL,
	      NULL,
	      &m_units[index],
	      NULL,
	      NULL,
	      NULL,
	      NULL,
	      "Value",
	      index,
	      nbFields);

  }

  m_nbDataAllocated	= nbFields ;

  DeleteFileParams();

}
//----------------------------------------
bool CBratProcessStats::CheckCommandLineOptions(int	argc, char	**argv)
{
  return CBratProcess::CheckCommandLineOptions(argc, argv,
		              "This program computes statistics on input data files",
	      		      StatsKeywordList);

}



//----------------------------------------
int32_t CBratProcessStats::Execute(std::string& msg)
{ 
    UNUSED(msg);

  CDate startExec;
  startExec.SetDateNow();
  
  size_t nbFiles = m_inputFiles.size();
  size_t nbExpr = m_fields.size();
  size_t nbUnits = m_units.size();

  const int32_t NUMBER_OF_STATISTICS = 5;

  char **files = new char*[nbFiles];
  
  char **expressions = new char*[nbExpr];
  
  char **units = new char*[nbUnits];
  
  double **data	= new double*[nbExpr];
  
  int32_t*	sizes	= new int32_t[nbExpr];
  
  size_t	actualSize;
  

  for (size_t i = 0 ; i < nbFiles ; i++)
  {
    std::string fileName = m_inputFiles.at(i);

    files[i] = new char[fileName.length() + 1];

    strcpy(files[i], fileName.c_str());
  }

  for (size_t i = 0 ; i < nbExpr ; i++)
  {
    std::string exprStr = m_fields.at(i).AsString();

    expressions[i] = new char[exprStr.length() + 1];

    strcpy(expressions[i], exprStr.c_str());

    data[i] = new double[NUMBER_OF_STATISTICS];
    sizes[i] = NUMBER_OF_STATISTICS;
  }

  for (size_t i = 0 ; i < nbUnits ; i++)
  {
    std::string unitStr = m_units.at(i).AsString();

    units[i] = new char[unitStr.length() + 1];

    strcpy(units[i], unitStr.c_str());
  }

  int32_t result	= CProduct::ReadData(m_inputFiles.size(), files,
				  m_recordName.c_str(),
          //				  "latitude > 20 && latitude < 30",
				  m_select.AsString().c_str(),
				  nbExpr,
				  expressions,
				  units,
				  data,
				  sizes,
				  &actualSize,
				  0,
				  1,
          CTools::m_defaultValueDOUBLE,
          &m_fieldSpecificUnit);

  if (result != BRATHL_SUCCESS)
  {
    return result;
  }
  

  std::ostream* fOut = NULL;
  std::ofstream fStream;
  bool resultInFile = false;

  if (m_outputFileName.empty())
  {
    fOut = &std::cout;
  }
  else if (m_outputFileName.compare("std::cout") == 0)
  {
    fOut = &std::cout;
  }
  else if (m_outputFileName.compare("std::cerr") == 0)
  {
    fOut = &std::cerr;
  }
  else
  {

    fStream.open(m_outputFileName.c_str(), std::ios::out | std::ios::trunc);
    if (fStream.good() != true)
    {
      std::cerr << "Open file failed - file name  " << m_outputFileName << 
                " error " << fStream.rdstate() << std::endl;
      return false;
    }
    resultInFile = true;
    fOut = &fStream;
  }



  double *vectorData = NULL;
  
  for (size_t j = 0 ; j < nbExpr ; j++)
  {
    fStream.flush();

    std::string resultStr = CTools::Format("Result for '%s'",  expressions[j]);

    *fOut << "========================== " << std::endl;
    *fOut << resultStr << std::endl;
    *fOut << "========================== " << std::endl;

    CTrace::Tracer(1, "==========================");
    CTrace::Tracer(1, resultStr);
    CTrace::Tracer(1, "==========================");

    vectorData = data[j];

    for (size_t i = 0 ; i < actualSize ; i++)
    {    
      std::string label;
      switch (i)
      {
      case 0 : 
        label = "Number of valid data";
        break;
      case 1 : 
        label = "Mean";
        break;
      case 2 : 
        label = "Standard deviation";
        break;
      case 3 : 
        label = "Minimum";
        break;
      case 4 : 
        label = "Maximum";
        break;

      }

      resultStr = CTools::Format("\t%s = %f", label.c_str(), vectorData[i]);

      *fOut << resultStr << std::endl;

      CTrace::Tracer(1, resultStr);
    }
  }
 

  if (resultInFile)
  {
    CTrace::Tracer(1, "\n=====> Result is also saved into " + m_outputFileName);
    fStream.close();
  }

  for (size_t i = 0 ; i < nbFiles ; i++)
  {
    delete []files[i];
    files[i] = NULL;
  }
  
  delete []files;
  files = NULL;

  for (size_t i = 0 ; i < nbExpr ; i++)
  {
    delete []expressions[i];
    expressions[i] = NULL;

    delete []data[i];
    data[i] = NULL;
  }

  delete []expressions;
  expressions = NULL;
  
  delete []data;
  data = NULL;

  for (size_t i = 0 ; i < nbUnits ; i++)
  {
    delete []units[i];
    units[i] = NULL;
  }
  
  delete []units;
  units = NULL;

  return BRATHL_SUCCESS;

}




}

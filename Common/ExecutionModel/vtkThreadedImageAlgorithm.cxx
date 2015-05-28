/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkThreadedImageAlgorithm.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkThreadedImageAlgorithm.h"
#include "vtkCellData.h"
#include "vtkCommand.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiThreader.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkExtentTranslator.h"


#define USE_SMP_DEFAULT_2D_BLOCK

// the default 2d block is used to compare with tbb's own 2d splitting option
#ifdef USE_SMP_DEFAULT_2D_BLOCK
  #include <tbb/parallel_for.h>
  #include <tbb/blocked_range2d.h>
#else
  #include "vtkSMPTools.h"   
#endif

struct vtkImageThreadStruct
{
vtkThreadedImageAlgorithm *Filter;
vtkInformation *Request;
vtkInformationVector **InputsInfo;
vtkInformationVector *OutputsInfo;
vtkImageData   ***Inputs;
vtkImageData   **Outputs;
};


#ifdef USE_SMP_DEFAULT_2D_BLOCK
  class Apply_TransformTBB
  {
    vtkImageThreadStruct* Str;
  public:

    Apply_TransformTBB(vtkImageThreadStruct* threadStruct)
    {
      this->Str = threadStruct;
    }     
    void operator()(const tbb::blocked_range2d<vtkIdType>& r) const 
    {
      int extent[6]  = { 0, -1, 0, -1, 0, -1 };
      extent[0] = r.cols().begin();
      extent[1] = r.cols().end();
      extent[2] = r.rows().begin();
      extent[3] = r.rows().end();
      extent[4] = 0;
      extent[5] = 0;    
      
      this->Str->Filter->ThreadedRequestData(this->Str->Request,
        this->Str->InputsInfo, this->Str->OutputsInfo,
        this->Str->Inputs, this->Str->Outputs,
        extent, 1);
    }
    };
#else

  class vtkThreadedImageAlgorithmFunctor
  {
    vtkImageThreadStruct *Str;
    int  Extents[6];
    int Threads;
    vtkExtentTranslator * ExtentTranslator;
    
  public:
    vtkThreadedImageAlgorithmFunctor(vtkImageThreadStruct * threadStruct,int* ext,int threads)
    {
      this->Str = threadStruct;      
      this->Threads = threads;
      this->ExtentTranslator = vtkExtentTranslator::New();
      memcpy(this->Extents,ext,sizeof(int)*6);
      
    }

    void operator()(vtkIdType begin, vtkIdType end)
    {
      for (int i =begin;i<=end;i++)
      {           
          int splitExtent[6]= { 0, -1, 0, -1, 0, -1 };
          // 3 option is split by block mode.
          this->ExtentTranslator->PieceToExtentThreadSafe(i,this->Threads,0,this->Extents,splitExtent,3,0);
          this->Str->Filter->ThreadedRequestData(this->Str->Request,
                                       this->Str->InputsInfo, this->Str->OutputsInfo,
                                       this->Str->Inputs, this->Str->Outputs,
                                       splitExtent, i);
      }   
    }
  };

#endif

//----------------------------------------------------------------------------
vtkThreadedImageAlgorithm::vtkThreadedImageAlgorithm()
{
  this->Threader = vtkMultiThreader::New();
  this->NumberOfThreads = this->Threader->GetNumberOfThreads();
  this->NumberOfSMPBlocks = 900;// The default number of SMP blocks.
  this->UseSmp = true; // turn on smp by default
}

//----------------------------------------------------------------------------
vtkThreadedImageAlgorithm::~vtkThreadedImageAlgorithm()
{
  this->Threader->Delete();
}

//----------------------------------------------------------------------------
void vtkThreadedImageAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "NumberOfThreads: " << this->NumberOfThreads << "\n";
}

void vtkThreadedImageAlgorithm::EnableSMP(bool state)
{
  this->UseSmp = state;
}

void vtkThreadedImageAlgorithm::SetSMPBlocks(int numberOfBlocks)
{
  if (numberOfBlocks<0)
  {
    vtkDebugMacro("Number of SMP Blocks cannot be less than 0");
    return;
  }
  this->NumberOfSMPBlocks =numberOfBlocks;
}


//----------------------------------------------------------------------------
// For streaming and threads.  Splits output update extent into num pieces.
// This method needs to be called num times.  Results must not overlap for
// consistent starting extent.  Subclass can override this method.
// This method returns the number of peices resulting from a successful split.
// This can be from 1 to "total".
// If 1 is returned, the extent cannot be split.
int vtkThreadedImageAlgorithm::SplitExtent(int splitExt[6],
                                           int startExt[6],
                                           int num, int total)
{
  int splitAxis;
  int min, max;

  vtkDebugMacro("SplitExtent: ( " << startExt[0] << ", " << startExt[1] << ", "
                << startExt[2] << ", " << startExt[3] << ", "
                << startExt[4] << ", " << startExt[5] << "), "
                << num << " of " << total);

  // start with same extent
  memcpy(splitExt, startExt, 6 * sizeof(int));

  splitAxis = 2;
  min = startExt[4];
  max = startExt[5];
  while (min >= max)
    {
    // empty extent so cannot split
    if (min > max)
      {
      return 1;
      }
    --splitAxis;
    if (splitAxis < 0)
      { // cannot split
      vtkDebugMacro("  Cannot Split");
      return 1;
      }
    min = startExt[splitAxis*2];
    max = startExt[splitAxis*2+1];
    }

  // determine the actual number of pieces that will be generated
  int range = max - min + 1;
  int valuesPerThread = static_cast<int>(ceil(range/static_cast<double>(total)));
  int maxThreadIdUsed = static_cast<int>(ceil(range/static_cast<double>(valuesPerThread))) - 1;
  if (num < maxThreadIdUsed)
    {
    splitExt[splitAxis*2] = splitExt[splitAxis*2] + num*valuesPerThread;
    splitExt[splitAxis*2+1] = splitExt[splitAxis*2] + valuesPerThread - 1;
    }
  if (num == maxThreadIdUsed)
    {
    splitExt[splitAxis*2] = splitExt[splitAxis*2] + num*valuesPerThread;
    }

  vtkDebugMacro("  Split Piece: ( " <<splitExt[0]<< ", " <<splitExt[1]<< ", "
                << splitExt[2] << ", " << splitExt[3] << ", "
                << splitExt[4] << ", " << splitExt[5] << ")");

  return maxThreadIdUsed + 1;
}


// this mess is really a simple function. All it does is call
// the ThreadedExecute method after setting the correct
// extent for this thread. Its just a pain to calculate
// the correct extent.
static VTK_THREAD_RETURN_TYPE vtkThreadedImageAlgorithmThreadedExecute( void *arg )
{
  vtkImageThreadStruct *str;
  int ext[6], splitExt[6], total;
  int threadId, threadCount;

  threadId = static_cast<vtkMultiThreader::ThreadInfo *>(arg)->ThreadID;
  threadCount = static_cast<vtkMultiThreader::ThreadInfo *>(arg)->NumberOfThreads;

  str = static_cast<vtkImageThreadStruct *>
    (static_cast<vtkMultiThreader::ThreadInfo *>(arg)->UserData);

  // if we have an output
  if (str->Filter->GetNumberOfOutputPorts())
    {
    // which output port did the request come from
    int outputPort =
      str->Request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());

    // if output port is negative then that means this filter is calling the
    // update directly, for now an error
    if (outputPort == -1)
      {
      return VTK_THREAD_RETURN_VALUE;
      }

    // get the update extent from the output port
    vtkInformation *outInfo =
      str->OutputsInfo->GetInformationObject(outputPort);
    int updateExtent[6];
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
                 updateExtent);
    memcpy(ext,updateExtent, sizeof(int)*6);
    }
  else
    {
    // if there is no output, then use UE from input, use the first input
    int inPort;
    bool found = false;
    for (inPort = 0; inPort < str->Filter->GetNumberOfInputPorts(); ++inPort)
      {
      if (str->Filter->GetNumberOfInputConnections(inPort))
        {
        int updateExtent[6];
        str->InputsInfo[inPort]
          ->GetInformationObject(0)
          ->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
                updateExtent);
        memcpy(ext,updateExtent, sizeof(int)*6);
        found = true;
        break;
        }
      }
    if (!found)
      {
      return VTK_THREAD_RETURN_VALUE;
      }
    }

  // execute the actual method with appropriate extent
  // first find out how many pieces extent can be split into.
  total = str->Filter->SplitExtent(splitExt, ext, threadId, threadCount);

  if (threadId < total)
    {
    // return if nothing to do
    if (splitExt[1] < splitExt[0] ||
        splitExt[3] < splitExt[2] ||
        splitExt[5] < splitExt[4])
      {
      return VTK_THREAD_RETURN_VALUE;
      }
    str->Filter->ThreadedRequestData(str->Request,
                                     str->InputsInfo, str->OutputsInfo,
                                     str->Inputs, str->Outputs,
                                     splitExt, threadId);
    }
  // else
  //   {
  //   otherwise don't use this thread. Sometimes the threads dont
  //   break up very well and it is just as efficient to leave a
  //   few threads idle.
  //   }

  return VTK_THREAD_RETURN_VALUE;
}


//----------------------------------------------------------------------------
// This is the superclasses style of Execute method.  Convert it into
// an imaging style Execute method.
int vtkThreadedImageAlgorithm::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  int i;

  // setup the threasd structure
  vtkImageThreadStruct str;
  str.Filter = this;
  str.Request = request;
  str.InputsInfo = inputVector;
  str.OutputsInfo = outputVector;
  

  int updateExtent[6] = { 0, -1, 0, -1, 0, -1 };
  bool updateExtentsFound = false;
  // now we must create the output array
  str.Outputs = 0;
  if (this->GetNumberOfOutputPorts())
    {
    str.Outputs = new vtkImageData * [this->GetNumberOfOutputPorts()];
    for (i = 0; i < this->GetNumberOfOutputPorts(); ++i)
      {
      vtkInformation* info = outputVector->GetInformationObject(i);
      vtkImageData *outData = vtkImageData::SafeDownCast(
        info->Get(vtkDataObject::DATA_OBJECT()));
      str.Outputs[i] = outData;
      if (outData)
        {
        
        info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
                  updateExtent);

        // unlike geometry filters, for image filters data is pre-allocated
        // in the superclass (which means, in this class)
        this->AllocateOutputData(outData, info, updateExtent);
        updateExtentsFound = true;
        }
      }
    }

  // now create the inputs array
  str.Inputs = 0;
  if (this->GetNumberOfInputPorts())
    {
    str.Inputs = new vtkImageData ** [this->GetNumberOfInputPorts()];
    for (i = 0; i < this->GetNumberOfInputPorts(); ++i)
      {
      str.Inputs[i] = 0;
      vtkInformationVector* portInfo = inputVector[i];

      if (portInfo->GetNumberOfInformationObjects())
        {
        int j;
        str.Inputs[i] =
          new vtkImageData *[portInfo->GetNumberOfInformationObjects()];
        for (j = 0; j < portInfo->GetNumberOfInformationObjects(); ++j)
          {
          vtkInformation* info = portInfo->GetInformationObject(j);
          str.Inputs[i][j] = vtkImageData::SafeDownCast(
            info->Get(vtkDataObject::DATA_OBJECT()));
          
            if(!updateExtentsFound && this->UseSmp)
            {
              info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
                  updateExtent);
            }          
          }
        }
      }
    }

  // copy other arrays
  if (str.Inputs && str.Inputs[0] && str.Outputs)
    {
    this->CopyAttributeData(str.Inputs[0][0],str.Outputs[0],inputVector);
    }

  if (this->UseSmp)
  {
    #ifdef USE_SMP_DEFAULT_2D_BLOCK
      tbb::parallel_for(tbb::blocked_range2d<vtkIdType>(updateExtent[0],updateExtent[1],updateExtent[2],updateExtent[3]), Apply_TransformTBB(&str));
    #else
      vtkThreadedImageAlgorithmFunctor functor(&str,updateExtent,NumberOfSMPBlocks);
      vtkSMPTools::For(0, NumberOfSMPBlocks-1, functor);
    #endif
  }
  else
  {
    this->Threader->SetNumberOfThreads(this->NumberOfThreads);
    this->Threader->SetSingleMethod(vtkThreadedImageAlgorithmThreadedExecute, &str);
    // always shut off debugging to avoid threading problems with GetMacros
    bool debug = this->Debug;
    this->Debug = false;
    this->Threader->SingleMethodExecute();
    this->Debug = debug;
  }
 

 

  // free up the arrays
  for (i = 0; i < this->GetNumberOfInputPorts(); ++i)
    {
    delete [] str.Inputs[i];
    }
  delete [] str.Inputs;
  delete [] str.Outputs;

  return 1;
}

//----------------------------------------------------------------------------
// The execute method created by the subclass.
void vtkThreadedImageAlgorithm::ThreadedRequestData(
  vtkInformation* vtkNotUsed( request ),
  vtkInformationVector** vtkNotUsed( inputVector ),
  vtkInformationVector* vtkNotUsed( outputVector ),
  vtkImageData ***inData,
  vtkImageData **outData,
  int extent[6],
  int threadId)
{
  this->ThreadedExecute(inData[0][0], outData[0], extent, threadId);
}

//----------------------------------------------------------------------------
// The execute method created by the subclass.
void vtkThreadedImageAlgorithm::ThreadedExecute(
  vtkImageData * inData,
  vtkImageData * outData,
  int extent[6],
  int threadId)
{
  (void)inData;
  (void)outData;
  (void)extent;
  (void)threadId;
  vtkErrorMacro("Subclass should override this method!!!");
}

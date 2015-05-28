// #include <vtkSmartPointer.h>
// #include <vtkImageConvolve.h>
// #include <vtkImageMandelbrotSource.h>
// #include <tbb/task_scheduler_init.h>

// int main(int argc, char *argv[])
// {

//   if(argc !=6)
//   {
//     printf ("Not all parms have being passed in\n"
//         "parm#1: Number Of threads to run ~4. \n"
//         "parm#2: Number of split SMP blocks ~500.\n"
//         "parm#3: Number of test iterations.\n"
//         "parm#4: The size of the work load ~5000\n"
//         "parm#5: Enable/Disabl SMP, use true/false\n");
//     return -1;
//   }

//   int numberOfThreadsToRun = atoi(argv[1]);
//   int numberOfSMPBlocks = atoi(argv[2]);
//   int testIterations = atoi(argv[3]);
//   int workSize = atoi(argv[4]);
//   bool enableSMP;

//   if(strcmp(argv[5],"true")==0)
//   {
//     enableSMP = true;
//   }
//   else
//   {
//     enableSMP = false;
//   }

  
//   int workExtent[6] = {0,workSize,0,workSize,0,0};

//   vtkSmartPointer<vtkImageMandelbrotSource> source =
//     vtkSmartPointer<vtkImageMandelbrotSource>::New();
//   source->SetWholeExtent(workExtent);
//   source->Update(); 
  
//   // Initilize with passed in number of threads
  
//   tbb::task_scheduler_init init(numberOfThreadsToRun);
  
//   for(int i =0;i <testIterations;i++)
//   {
//     clock_t start = clock(), diff;

//     vtkSmartPointer<vtkImageConvolve> convolveFilter =
//     vtkSmartPointer<vtkImageConvolve>::New();
//     convolveFilter->SetInputConnection(source->GetOutputPort());
//     double kernel[9] = {1,1,1,1,1,1,1,1,1};
//     convolveFilter->EnableSMP(enableSMP);
//     convolveFilter->SetSMPBlocks(numberOfSMPBlocks);
//     convolveFilter->SetKernel3x3(kernel);
//     convolveFilter->Update();
    
//     diff = clock() - start;
//     int msec = diff * 1000 / CLOCKS_PER_SEC;
    
//     printf("%d:%d,%d\n", i,msec/1000, msec%1000);
//   }
  
 
//   return EXIT_SUCCESS;
// }


#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkImageMapper3D.h>
#include <vtkImageConvolve.h>
#include <vtkImageMandelbrotSource.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkRenderer.h>
#include <vtkImageActor.h>
#include <vtkImageCast.h>
 
int main(int, char *[])
{
  // Create an image
  vtkSmartPointer<vtkImageMandelbrotSource> source =
    vtkSmartPointer<vtkImageMandelbrotSource>::New();
  source->Update();
 
  vtkSmartPointer<vtkImageCast> originalCastFilter =
    vtkSmartPointer<vtkImageCast>::New();
  originalCastFilter->SetInputConnection(source->GetOutputPort());
  originalCastFilter->SetOutputScalarTypeToUnsignedChar();
  originalCastFilter->Update();
 
  vtkSmartPointer<vtkImageConvolve> convolveFilter =
    vtkSmartPointer<vtkImageConvolve>::New();
  convolveFilter->SetInputConnection(source->GetOutputPort());
  double kernel[9] = {1,1,1,1,1,1,1,1,1};
  convolveFilter->SetKernel3x3(kernel);
  convolveFilter->Update();
 
  vtkSmartPointer<vtkImageCast> convolvedCastFilter =
    vtkSmartPointer<vtkImageCast>::New();
  convolvedCastFilter->SetInputConnection(convolveFilter->GetOutputPort());
  convolvedCastFilter->SetOutputScalarTypeToUnsignedChar();
  convolvedCastFilter->Update();
 
  // Create an actor
  vtkSmartPointer<vtkImageActor> originalActor =
    vtkSmartPointer<vtkImageActor>::New();
  originalActor->GetMapper()->SetInputConnection(
    originalCastFilter->GetOutputPort());
 
  vtkSmartPointer<vtkImageActor> convolvedActor =
    vtkSmartPointer<vtkImageActor>::New();
  convolvedActor->GetMapper()->SetInputConnection(
    convolvedCastFilter->GetOutputPort());
 
  // Define viewport ranges
  // (xmin, ymin, xmax, ymax)
  double leftViewport[4] = {0.0, 0.0, 0.5, 1.0};
  double rightViewport[4] = {0.5, 0.0, 1.0, 1.0};
 
  // Setup renderer
  vtkSmartPointer<vtkRenderer> originalRenderer =
    vtkSmartPointer<vtkRenderer>::New();
  originalRenderer->SetViewport(leftViewport);
  originalRenderer->AddActor(originalActor);
  originalRenderer->ResetCamera();
 
  vtkSmartPointer<vtkRenderer> convolvedRenderer =
    vtkSmartPointer<vtkRenderer>::New();
  convolvedRenderer->SetViewport(rightViewport);
  convolvedRenderer->AddActor(convolvedActor);
  convolvedRenderer->ResetCamera();
 
  // Setup render window
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->SetSize(600,300);
  renderWindow->AddRenderer(originalRenderer);
  renderWindow->AddRenderer(convolvedRenderer);
 
  // Setup render window interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  vtkSmartPointer<vtkInteractorStyleImage> style =
    vtkSmartPointer<vtkInteractorStyleImage>::New();
 
  renderWindowInteractor->SetInteractorStyle(style);
 
  // Render and start interaction
  renderWindowInteractor->SetRenderWindow(renderWindow);
  renderWindowInteractor->Initialize();
 
  renderWindowInteractor->Start();
 
  return EXIT_SUCCESS;
}

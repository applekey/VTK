/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageMapper3D.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkImageMapper3D - abstract class for mapping images to the screen
// .SECTION Description
// vtkImageMapper3D is a mapper that will draw a 2D image, or a slice
// of a 3D image.  The slice plane can be set automatically follow the
// camera, so that it slices through the focal point and faces the camera.
// .SECTION Thanks
// Thanks to David Gobbi at the Seaman Family MR Centre and Dept. of Clinical
// Neurosciences, Foothills Medical Centre, Calgary, for providing this class.
// .SECTION See also
// vtkImage vtkImageProperty vtkImageResliceMapper vtkImageSliceMapper


#ifndef __vtkImageMapper3D_h
#define __vtkImageMapper3D_h

#include "vtkAbstractMapper3D.h"

class vtkRenderer;
class vtkProp3D;
class vtkMatrix4x4;
class vtkLookupTable;
class vtkScalarsToColors;
class vtkImageSlice;
class vtkImageProperty;
class vtkImageData;
class vtkImageToImageMapper3DFriendship;

#define VTK_IMAGE_DEFAULT  0
#define VTK_IMAGE_UNDERLAY 1
#define VTK_IMAGE_OVERLAY  2

class VTK_RENDERING_EXPORT vtkImageMapper3D : public vtkAbstractMapper3D
{
public:
  vtkTypeMacro(vtkImageMapper3D,vtkAbstractMapper3D);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // This should only be called by the renderer.
  virtual void Render(vtkRenderer *renderer, vtkImageSlice *prop) = 0;

  // Description:
  // Release any graphics resources that are being consumed by
  // this mapper.  The parameter window is used to determine
  // which graphic resources to release.
  virtual void ReleaseGraphicsResources(vtkWindow *) = 0;

  // Description:
  // The input data for this mapper.
  void SetInput(vtkImageData *input);
  vtkImageData *GetInput();
  vtkDataSet *GetDataSetInput();
  vtkDataObject *GetDataObjectInput();

  // Description:
  // Instead of displaying the image only out to the image
  // bounds, include a half-voxel border around the image.
  // Within this border, the image values will be extrapolated
  // rather than interpolated.
  vtkSetMacro(Border, int);
  vtkBooleanMacro(Border, int);
  vtkGetMacro(Border, int);

  // Description:
  // Automatically set the slice position to the camera focal point.
  // This provides a convenient way to interact with the image, since
  // most Interactors directly control the camera.
  vtkSetMacro(SliceAtFocalPoint, int);
  vtkBooleanMacro(SliceAtFocalPoint, int);
  vtkGetMacro(SliceAtFocalPoint, int);

  // Description:
  // Automatically set the slice orientation so that it faces the camera.
  // This provides a convenient way to interact with the image, since
  // most Interactors directly control the camera.
  vtkSetMacro(SliceFacesCamera, int);
  vtkBooleanMacro(SliceFacesCamera, int);
  vtkGetMacro(SliceFacesCamera, int);

  // Description:
  // A plane that describes what slice of the input is being
  // rendered by the mapper.  This plane is in world coordinates,
  // not data coordinates.  Before using this plane, call Update
  // or UpdateInformation to make sure the plane is up-to-date.
  // These methods are automatically called by Render.
  vtkGetObjectMacro(SlicePlane, vtkPlane);

  // Description:
  // Get the plane as a homogeneous 4-vector that gives the plane
  // equation coefficients.  The prop3D matrix must be provided so
  // that the plane can be converted to data coords.
  virtual void GetSlicePlaneInDataCoords(vtkMatrix4x4 *propMatrix,
                                         double plane[4]);

protected:
  vtkImageMapper3D();
  ~vtkImageMapper3D();

  // Description:
  // See algorithm for more info
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  // Description:
  // Handle requests from the pipeline executive.
  virtual int ProcessRequest(vtkInformation* request,
                             vtkInformationVector** inInfo,
                             vtkInformationVector* outInfo);


  // Description:
  // Apply a lookup table to the slice specified by the extent, and
  // put only that slice into the output as RGBA.
  static void ApplyLookupTableToImageScalars(
    void *inPtr, unsigned char *outPtr, const int extent[6],
    int numComponents, int inIncY, int inIncZ, int outIncY, int outIncZ,
    int scalarType, vtkScalarsToColors *lookupTable);

  // Description:
  // Copy the specified extent from the input to the output, and convert
  // to RGBA at the same time by adjusting the scalar range to [0,255]
  // and adding an alpha component.
  static void ConvertImageScalarsToRGBA(
    void *inPtr, unsigned char *outPtr, const int extent[6],
    int numComp, int inIncY, int inIncZ, int outIncY, int outIncZ,
    int scalarType, double scalarRange[2]);

  // Description:
  // Checkerboard the alpha component of an RGBA image.  The origin and
  // spacing are in pixel units.
  static void CheckerboardRGBA(
    unsigned char *data, int xsize, int ysize,
    double originx, double originy, double spacingx, double spacingy);

  // Description:
  // Perform window/level and color mapping operations to produce
  // unsigned char data that can be used as a texture.  See the
  // source file for more information.
  unsigned char *MakeTextureData(
    vtkImageProperty *property, vtkImageData *input, int extent[6],
    int &xsize, int &ysize, int &bytesPerPixel, bool &reuseTexture,
    bool &reuseData);

  // Description:
  // Compute the coordinates and texture coordinates for the image, given
  // an extent that describes a single slice.
  void MakeTextureGeometry(
    vtkImageData *input, const int extent[6], int border,
    double coords[12], double tcoords[8]);

  // Description:
  // Given an extent that describes a slice (it must have unit thickness
  // in one of the three directions), return the dimension indices that
  // correspond to the texture "x" and "y", provide the x, y image size,
  // and provide the texture size (padded to a power of two if the hardware
  // requires).
  virtual void ComputeTextureSize(
    const int extent[6], int &xdim, int &ydim,
    int imageSize[2], int textureSize[2]);

  // Description:
  // Check the texture size.
  virtual bool TextureSizeOK(const int size[2]);

  // Description:
  // Called by RecursiveRenderTexturedPolygon, overriden by subclasses.
  virtual void RenderTexturedPolygon(
    vtkRenderer *ren, vtkProp3D *prop, vtkImageProperty *property,
    vtkImageData *image, int extent[6], bool recursive);

  // Description:
  // Recursive internal method, will call the non-recursive method
  // as many times as necessary if the texture must be broken up into
  // pieces that are small enough for the GPU to render
  virtual void RecursiveRenderTexturedPolygon(
    vtkRenderer *ren, vtkProp3D *prop, vtkImageProperty *property,
    vtkImageData *image, int extent[6], bool recursive);

  // Description:
  // Get the renderer associated with this mapper, or zero if none.
  // This will raise an error if multiple renderers are found.
  vtkRenderer *GetCurrentRenderer();

  // Description:
  // Get the vtkImage prop associated with this mapper, or zero if none.
  vtkImageSlice *GetCurrentProp() { return this->CurrentProp; }

  // Description:
  // Get the data-to-world matrix for this mapper, according to the
  // assembly path for its prop.
  vtkMatrix4x4 *GetDataToWorldMatrix();

  int Border;
  vtkLookupTable *DefaultLookupTable;

  // The slice.
  vtkPlane *SlicePlane;
  int SliceAtFocalPoint;
  int SliceFacesCamera;

  // Information about the image, updated by UpdateInformation
  double DataSpacing[3];
  double DataOrigin[3];
  int DataWholeExtent[6];

  // Set by vtkImageStack when doing multi-pass rendering
  bool MatteEnable;
  bool ColorEnable;
  bool DepthEnable;

private:
  // The prop this mapper is attached to, or zero if none.
  vtkImageSlice *CurrentProp;

  // The cached data-to-world matrix
  vtkMatrix4x4 *DataToWorldMatrix;

  // Set by vtkImageSlice if a render is in progress
  bool InRender;

  vtkImageMapper3D(const vtkImageMapper3D&);  // Not implemented.
  void operator=(const vtkImageMapper3D&);  // Not implemented.

  friend class vtkImageToImageMapper3DFriendship;
};

#endif
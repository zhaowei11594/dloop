/**
 * File: demoDetector.h
 * Date: November 2011
 * Author: Dorian Galvez-Lopez
 * Description: demo application of DLoopDetector
 */

#ifndef __DEMO_DETECTOR__
#define __DEMO_DETECTOR__

#include <iostream>
#include <vector>
#include <string>

// OpenCV
#include <opencv/cv.h>
#include <opencv/highgui.h>

// DLoopDetector and DBoW2
#include "DBoW2.h"
#include "DLoopDetector.h"
#include "DUtils.h"
#include "DUtilsCV.h"
#include "DVision.h"

using namespace DLoopDetector;
using namespace DBoW2;
using namespace std;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

/// Generic class to create functors to extract features
template<class TDescriptor>
class FeatureExtractor
{
public:
  /**
   * Extracts features
   * @param im image
   * @param keys keypoints extracted
   * @param descriptors descriptors extracted
   */
  virtual void operator()(const cv::Mat &im, 
    vector<cv::KeyPoint> &keys, vector<TDescriptor> &descriptors) const = 0;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

/// @param TVocabulary vocabulary class (e.g: Surf64Vocabulary)
/// @param TDetector detector class (e.g: Surf64LoopDetector)
/// @param TDescriptor descriptor class (e.g: vector<float> for SURF)
template<class TVocabulary, class TDetector, class TDescriptor>
/// Class to run the demo 
class demoDetector
{
public:

  /**
   * @param vocfile vocabulary file to load
   * @param imagedir directory to read images from
   * @param posefile pose file
   * @param width image width
   * @param height image height
   */
  demoDetector(const std::string &vocfile, const std::string &imagedir,
    const std::string &posefile, const std::string &loopFile,int width, int height);
    
  ~demoDetector(){}

  /**
   * Runs the demo
   * @param name demo name
   * @param extractor functor to extract features
   */
  void run(const std::string &name, 
    const FeatureExtractor<TDescriptor> &extractor);

protected:

  /**
   * Reads the robot poses from a file
   * @param filename file
   * @param xs
   * @param ys
   */
  void readPoseFile(const char *filename, std::vector<double> &xs, 
    std::vector<double> &ys) const;


protected:

  std::string m_vocfile;
  std::string m_imagedir;
  std::string m_posefile;
  int m_width;
  int m_height;
   
  DUtils::LineFile m_loopFile;
};

// ---------------------------------------------------------------------------

template<class TVocabulary, class TDetector, class TDescriptor>
demoDetector<TVocabulary, TDetector, TDescriptor>::demoDetector
  (const std::string &vocfile, const std::string &imagedir,
  const std::string &posefile, const std::string &loopFile,int width, int height)
  : m_vocfile(vocfile), m_imagedir(imagedir), m_posefile(posefile),
    m_width(width), m_height(height)
{
	m_loopFile.OpenForAppending(loopFile.c_str());
}

// ---------------------------------------------------------------------------

template<class TVocabulary, class TDetector, class TDescriptor>
void demoDetector<TVocabulary, TDetector, TDescriptor>::run
  (const std::string &name, const FeatureExtractor<TDescriptor> &extractor)
{
  cout << "DLoopDetector Demo" << endl 
    << "Dorian Galvez-Lopez" << endl
    << "http://webdiis.unizar.es/~dorian" << endl << endl;
  
  // Set loop detector parameters
  typename TDetector::Parameters params(m_height, m_width);
  
  // Parameters given by default are:
  // use nss = true
  // alpha = 0.3
  // k = 3
  // geom checking = GEOM_DI
  // di levels = 0
  
  // We are going to change these values individually:
  params.use_nss = true; // use normalized similarity score instead of raw score
  params.alpha = 0.2; // nss threshold
  params.k = 1; // a loop must be consistent with 1 previous matches
  params.geom_check = GEOM_DI; // use direct index for geometrical checking
  params.di_levels = 2; // use two direct index levels
  
  // To verify loops you can select one of the next geometrical checkings:
  // GEOM_EXHAUSTIVE: correspondence points are computed by comparing all
  //    the features between the two images.
  // GEOM_FLANN: as above, but the comparisons are done with a Flann structure,
  //    which makes them faster. However, creating the flann structure may
  //    be slow.
  // GEOM_DI: the direct index is used to select correspondence points between
  //    those features whose vocabulary node at a certain level is the same.
  //    The level at which the comparison is done is set by the parameter
  //    di_levels:
  //      di_levels = 0 -> features must belong to the same leaf (word).
  //         This is the fastest configuration and the most restrictive one.
  //      di_levels = l (l < L) -> node at level l starting from the leaves.
  //         The higher l, the slower the geometrical checking, but higher
  //         recall as well.
  //         Here, L stands for the depth levels of the vocabulary tree.
  //      di_levels = L -> the same as the exhaustive technique.
  // GEOM_NONE: no geometrical checking is done.
  //
  // In general, with a 10^6 vocabulary, GEOM_DI with 2 <= di_levels <= 4 
  // yields the best results in recall/time.
  // Check the T-RO paper for more information.
  //
  
  // Load the vocabulary to use
  cout << "Loading " << name << " vocabulary..." << endl;
  TVocabulary voc(m_vocfile);
  
  // Initiate loop detector with the vocabulary 
  cout << "Processing sequence..." << endl;
  TDetector detector(voc, params);
  
  // Process images
  vector<cv::KeyPoint> keys;
  vector<TDescriptor> descriptors;

  // load image filenames  
  vector<string> filenames = 
    DUtils::FileFunctions::Dir(m_imagedir.c_str(), ".png", true);
  
  // load robot poses
  vector<double> xs, ys;
  readPoseFile(m_posefile.c_str(), xs, ys);
  
  // we can allocate memory for the expected number of images
  detector.allocate(filenames.size());
  
  // prepare visualization windows
  DUtilsCV::GUI::tWinHandler win 	   = "Current image";
  DUtilsCV::GUI::tWinHandler winloop   = "Looped image";
  DUtilsCV::GUI::tWinHandler winplot   = "Trajectory";
  
  DUtilsCV::Drawing::Plot::Style normal_style(2); // thickness
  DUtilsCV::Drawing::Plot::Style loop_style('r', 2); // color, thickness
  
  DUtilsCV::Drawing::Plot implot(240, 320,
    - *std::max_element(xs.begin(), xs.end()),
    - *std::min_element(xs.begin(), xs.end()),
    *std::min_element(ys.begin(), ys.end()),
    *std::max_element(ys.begin(), ys.end()), 20);
  
  // prepare profiler to measure times
  DUtils::Profiler profiler;
  
  int count = 0;
 
   // write headerline into file 
    std::string head = "# currentImgIdx loopedImgIdx currentImgName loopedImgName ";
    m_loopFile << head;
  // go
  for(unsigned int i = 0; i < filenames.size(); ++i)
  {
    cout << "Adding image " << i << ": " << filenames[i] << "... " << endl;
   
    std::string _str;
    std::stringstream ss;
    ss << i; 
    // get image
    cv::Mat im = cv::imread(filenames[i].c_str(), 0); // grey scale
    
    // show image
    DUtilsCV::GUI::showImage(im, true, &win, 10);
    
    // get features
    profiler.profile("features");
    extractor(im, keys, descriptors);
    profiler.stop();
        
    // add image to the collection and check if there is some loop
    DetectionResult result;
    
    profiler.profile("detection");
    detector.detectLoop(keys, descriptors, result);
    profiler.stop();
    std::string currentFileName = DUtils::FileFunctions::FileName(filenames[i]);

    if(result.detection())
    {
     cout << "- Loop found with image " << result.match << "!" << endl;
     std::string loopedFileName = DUtils::FileFunctions::FileName(filenames[result.match]);
     std::stringstream ss1;
     ss1 << result.match;
     _str = ss.str() + " " + ss1.str() + " " + currentFileName + " " + loopedFileName;

     /* ========== Add by Samuel for debugging =============== */
     // get image
     cv::Mat im = cv::imread(filenames[result.match].c_str(), 0); // grey scale
     // show image
     DUtilsCV::GUI::showImage(im, true, &winloop, 10);

     /*
     cout << " Press ENTER to continue... " << endl;
     std::cin.ignore(); // Pause for debugging.
     */
     /* ====================================================== */
      ++count;
    }
    else
    {
      cout << "- No loop: ";
      switch(result.status)
      {
        case CLOSE_MATCHES_ONLY:
          cout << "All the images in the database are very recent" << endl;
          break;
          
        case NO_DB_RESULTS:
          cout << "There are no matches against the database (few features in"
            " the image?)" << endl;
          break;
          
        case LOW_NSS_FACTOR:
          cout << "Little overlap between this image and the previous one"
            << endl;
          break;
            
        case LOW_SCORES:
          cout << "No match reaches the score threshold (alpha: " <<
            params.alpha << ")" << endl;
          break;
          
        case NO_GROUPS:
          cout << "Not enough close matches to create groups. "
            << "Best candidate: " << result.match << endl;
          break;
          
        case NO_TEMPORAL_CONSISTENCY:
          cout << "No temporal consistency (k: " << params.k << "). "
            << "Best candidate: " << result.match << endl;
          break;
          
        case NO_GEOMETRICAL_CONSISTENCY:
          cout << "No geometrical consistency. Best candidate: " 
            << result.match << endl;
          break;
          
        default:
          break;
      }
       //_str << ss.str() << " " << "N" << " " << currentFileName << " " << "N";
       _str = ss.str() + " " + "N" + " " + currentFileName + " " + "N";
    }
    
    cout << endl;
    m_loopFile << _str;
    // show trajectory
    if(i > 0)
    {
      if(result.detection())
        implot.line(-xs[i-1], ys[i-1], -xs[i], ys[i], loop_style);
      else
        implot.line(-xs[i-1], ys[i-1], -xs[i], ys[i], normal_style);
      
      DUtilsCV::GUI::showImage(implot.getImage(), true, &winplot, 10); 
    }
  }
  
  if(count == 0)
  {
    cout << "No loops found in this image sequence" << endl;
  }
  else
  {
    cout << count << " loops found in this image sequence!" << endl;
  } 

  cout << endl << "Execution time:" << endl
    << " - Feature computation: " << profiler.getMeanTime("features") * 1e3
    << " ms/image" << endl
    << " - Loop detection: " << profiler.getMeanTime("detection") * 1e3
    << " ms/image" << endl;

  cout << endl << "Press a key to finish..." << endl;
  DUtilsCV::GUI::showImage(implot.getImage(), true, &winplot, 0);
  m_loopFile.Close();
}

// ---------------------------------------------------------------------------

template<class TVocabulary, class TDetector, class TDescriptor>
void demoDetector<TVocabulary, TDetector, TDescriptor>::readPoseFile
  (const char *filename, std::vector<double> &xs, std::vector<double> &ys)
  const
{
  xs.clear();
  ys.clear();
  
  fstream f(filename, ios::in);
  
  string s;
  double ts, x, y, t;
  while(!f.eof())
  {
    getline(f, s);
    if(!f.eof() && !s.empty())
    {
      //sscanf(s.c_str(), "%lf, %lf, %lf, %lf", &ts, &x, &y, &t);
      sscanf(s.c_str(), "%lf, %lf", &x, &y);
      std::cout<<"x, y = "<<x<<" "<<y<<std::endl;
      xs.push_back(x);
      ys.push_back(y);
    }
  }
  
  f.close();
}

// ---------------------------------------------------------------------------

#endif


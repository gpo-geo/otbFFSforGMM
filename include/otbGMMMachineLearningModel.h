#ifndef __otbGMMMachineLearningModel_h
#define __otbGMMMachineLearningModel_h

#include "itkMacro.h"
#include "itkLightObject.h"
#include "itkCovarianceSampleFilter.h"
#include "itkSampleClassifierFilter.h" // for Subsample type
#include "otbMachineLearningModel.h"

namespace otb
{
template <class TInputValue, class TTargetValue>
class ITK_EXPORT GMMMachineLearningModel
  : public MachineLearningModel <TInputValue, TTargetValue>
{

public:
  /** Standard class typedefs. */
  typedef GMMMachineLearningModel                         Self;
  typedef MachineLearningModel<TInputValue, TTargetValue> Superclass;
  typedef itk::SmartPointer<Self>                         Pointer;
  typedef itk::SmartPointer<const Self>                   ConstPointer;

  typedef typename Superclass::InputValueType             InputValueType;
  typedef typename Superclass::InputSampleType            InputSampleType;
  typedef typename Superclass::InputListSampleType        InputListSampleType;
  typedef typename Superclass::TargetValueType            TargetValueType;
  typedef typename Superclass::TargetSampleType           TargetSampleType;
  typedef typename Superclass::TargetListSampleType       TargetListSampleType;
  typedef typename Superclass::ConfidenceValueType        ConfidenceValueType;

  typedef double RealType;
  typedef vnl_matrix<RealType> MatrixType;
  typedef vnl_vector<RealType> VectorType;

  itkNewMacro(Self);
  itkTypeMacro(GMMMachineLearningModel, MachineLearningModel);

  /** Set m_tau and update m_lambdaQ and m_cstDecision */
  void SetTau(RealType tau);

  /** Make a gridsearch with cross-validation to select the appropriate tau */
  void TrainTau(std::vector<RealType> tauGrid, int nfold, const std::string & criterion="accuracy", int seed=0);

  /** Compute decomposition in eigenvalues and eigenvectors of a symmetric matrix */
  void Decomposition(MatrixType &inputMatrix, MatrixType &outputMatrix, VectorType &eigenValues);

  /** Train the machine learning model */
  virtual void Train();

  /** Save the model to file */
  virtual void Save(const std::string & filename, const std::string & name="");

  /** Load the model from file */
  virtual void Load(const std::string & filename, const std::string & name="");

  /**\name Classification model file compatibility tests */
  //@{
  /** Is the input model file readable and compatible with the corresponding classifier ? */
  virtual bool CanReadFile(const std::string &);

  /** Is the input model file writable and compatible with the corresponding classifier ? */
  virtual bool CanWriteFile(const std::string &);
  //@}

  /** Define accessors */
  itkSetMacro(ClassNb, unsigned int);
  itkGetMacro(ClassNb, unsigned int);
  itkSetMacro(FeatNb, unsigned int);
  itkGetMacro(FeatNb, unsigned int);
  itkGetMacro(Tau, RealType);
  itkGetMacro(RateGridsearch, std::vector<RealType>);
  itkGetMacro(NbSpl, std::vector<unsigned>);
  void SetMapOfClasses(const std::map<TargetValueType, int>& mapOfClasses);
  void SetMapOfIndices(const std::map<int, TargetValueType>& mapOfIndices);

  /** Functions to update model */
  void AddMean(VectorType vector);
  void AddCovMatrix(MatrixType covMatrix);
  void AddNbSpl(unsigned long n);
  void UpdateProportion();
  void UpdateDecomposition();

protected:
  /** Constructor */
  GMMMachineLearningModel();

  /** Destructor */
  virtual ~GMMMachineLearningModel();

  /** Predict values using the model */
  TargetSampleType DoPredict(const InputSampleType& input, ConfidenceValueType *quality=ITK_NULLPTR) const ITK_OVERRIDE;

  /** PrintSelf method */
  void PrintSelf(std::ostream& os, itk::Indent indent) const;

  /** Number of class */
  unsigned int m_ClassNb;

  /** Number of features */
  unsigned int m_FeatNb;

  /** Regularization constant */
  RealType m_Tau;

  /** Map of classes */
  std::map<TargetValueType, int> m_MapOfClasses;
  std::map<int, TargetValueType> m_MapOfIndices;

  /** Vector containing the number of samples in each class */
  std::vector<unsigned> m_NbSpl;

  /** Vector containing the proportion of samples in each class */
  std::vector<double> m_Proportion;

  /** Vector of size C containing the mean vector (size d) of each class */
  std::vector<VectorType> m_Means;

  /** Vector of covariance matrix (dxd) of each class */
  std::vector<MatrixType> m_Covariances;

  /** Vector of size C containing eigenvalues of the covariance matrices */
  std::vector<VectorType> m_EigenValues;

  /** Vector of size C of eigenvectors matrix (dxd) of each class (each line is an eigenvector) */
  std::vector<MatrixType> m_Q;

  /** Vector of size C of matrix (dxd) eigenvalues^(-1/2) * Q.T for each class */
  std::vector<MatrixType> m_LambdaQ;

  /** Vector of size C of scalar (logdet cov - 2*log proportion) for each class */
  std::vector<RealType> m_CstDecision;

  /** Create one subset of samples for each class */
  typedef itk::Statistics::Subsample< InputListSampleType > ClassSampleType;
  std::vector< typename ClassSampleType::Pointer > m_ClassSamples;

  /** Vector of classification rate for each tau tested */
  std::vector<RealType> m_RateGridsearch;

  /** Flag that tells if the model support confidence index output */
  bool m_ConfidenceIndex;

private:
  GMMMachineLearningModel(const Self &); //purposely not implemented
  void operator =(const Self&); //purposely not implemented


};
} // end namespace otb

#ifndef OTB_MANUAL_INSTANTIATION
#include "otbGMMMachineLearningModel.txx"
#endif

#endif

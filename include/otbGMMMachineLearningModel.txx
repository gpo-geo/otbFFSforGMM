#ifndef __otbGMMMachineLearningModel_txx
#define __otbGMMMachineLearningModel_txx

#include <fstream>
#include <math.h>
#include <limits>
#include <vector>
#include "itkMacro.h"
#include "itkSubsample.h"
#include "itkSymmetricEigenAnalysis.h"
#include "otbGMMMachineLearningModel.h"

namespace otb
{

template <class TInputValue, class TOutputValue>
GMMMachineLearningModel<TInputValue,TOutputValue>
::GMMMachineLearningModel():
  m_classNb(0),
  m_featNb(0),
  m_tau(0)
{
  m_CovarianceEstimator = CovarianceEstimatorType::New();
}


template <class TInputValue, class TOutputValue>
GMMMachineLearningModel<TInputValue,TOutputValue>
::~GMMMachineLearningModel()
{
}

/** Compute de decomposition in eigenvalues and eigenvectors of a matrix */
template <class TInputValue, class TOutputValue>
void GMMMachineLearningModel<TInputValue,TOutputValue>
::Decomposition(MatrixType &inputMatrix, MatrixType &outputMatrix, itk::VariableLengthVector<MatrixValueType> &eigenValues)
{
  typedef itk::SymmetricEigenAnalysis< MatrixType, itk::VariableLengthVector<MatrixValueType>, MatrixType > SymmetricEigenAnalysisType;
  SymmetricEigenAnalysisType symmetricEigenAnalysis(inputMatrix.Cols());

  symmetricEigenAnalysis.SetOrderEigenValues(false);

  symmetricEigenAnalysis.ComputeEigenValuesAndVectors(inputMatrix, eigenValues, outputMatrix);

  if (m_tau != 0)
  {
    for (int i = 0; i < eigenValues.GetSize(); ++i)
    {
      if (eigenValues[i] < std::numeric_limits<MatrixValueType>::epsilon())
        eigenValues[i] = std::numeric_limits<MatrixValueType>::epsilon();
    }
  }
}

/** Train the machine learning model */
template <class TInputValue, class TOutputValue>
void
GMMMachineLearningModel<TInputValue,TOutputValue>
::Train()
{
  // Get pointer to samples and labels
  typename InputListSampleType::Pointer samples = this->GetInputListSample();
  typename TargetListSampleType::Pointer labels = this->GetTargetListSample();

  // Declare iterator for samples and labels
  typename TargetListSampleType::ConstIterator refIterator = labels->Begin();
  typename InputListSampleType::ConstIterator inputIterator = samples->Begin();

  // Get number of samples
  unsigned long sampleNb = samples->Size();

  // Get number of features
  m_featNb = samples->GetMeasurementVectorSize();

  // Get number of classes and map indice with label
  while (refIterator != labels->End())
  {
    TargetValueType currentLabel = refIterator.GetMeasurementVector()[0];
    if (m_MapOfClasses.find(currentLabel) == m_MapOfClasses.end())
    {
      m_MapOfClasses[currentLabel] = m_classNb;
      m_MapOfIndices[m_classNb] = currentLabel;
      ++m_classNb;
    }
    ++refIterator;
  }

  // Create one subsample set for each class
  typedef itk::Statistics::Subsample< InputListSampleType > ClassSampleType;
  std::vector< typename ClassSampleType::Pointer > classSamples;
  for ( unsigned int i = 0; i < m_classNb; ++i )
  {
    classSamples.push_back( ClassSampleType::New() );
    classSamples[i]->SetSample( samples );
  }
  refIterator = labels->Begin();
  inputIterator = samples->Begin();
  while (inputIterator != samples->End())
  {
    TargetValueType currentLabel = refIterator.GetMeasurementVector()[0];
    classSamples[m_MapOfClasses[currentLabel]]->AddInstance( inputIterator.GetInstanceIdentifier() );
    ++inputIterator;
    ++refIterator;
  }

  // Estimate covariance matrices, mean vectors and proportions
  for ( unsigned int i = 0; i < m_classNb; ++i )
  {
    m_NbSpl.push_back(classSamples[i]->Size());
    m_Proportion.push_back((float) m_NbSpl[i] / (float) sampleNb);

    m_CovarianceEstimator->SetInput( classSamples[i] );
    m_CovarianceEstimator->Update();

    m_Covariances.push_back(m_CovarianceEstimator->GetCovarianceMatrix());
    m_Means.push_back(m_CovarianceEstimator->GetMean());
  }

  // Decompose covariance matrix in eigenvalues/eigenvectors
  if (m_Q.empty())
  {
    // MatrixType matrix(m_featNb,m_featNb);
    m_Q.resize(m_classNb,MatrixType(m_featNb,m_featNb));
    itk::VariableLengthVector<MatrixValueType> newVector;
    newVector.SetSize(m_featNb);
    m_eigenValues.resize(m_classNb,newVector);

    for (int i = 0; i < m_classNb; ++i)
    {
      // Make decomposition
      Decomposition(m_Covariances[i], m_Q[i], m_eigenValues[i]);
    }
  }

  // Precompute lambda^(-1/2) * Q and log(det lambda)
  if (m_lambdaQ.empty())
  {
    MatrixType lambda(m_featNb,m_featNb);
    MatrixValueType logdet;
    for (int i = 0; i < m_classNb; ++i)
    {
      lambda.Fill(0);
      logdet = 0;

      for (int j = 0; j < m_featNb; ++j)
      {
        for (int k = 0; k < m_featNb; ++k)
        {
          lambda(k,k) = 1 / sqrt(m_eigenValues[i][k] + m_tau);
          logdet += log(m_eigenValues[i][k] + m_tau);
        }
      }

      m_cstDecision.push_back(logdet - 2*log(m_Proportion[i]));
      m_lambdaQ.push_back(lambda * m_Q[i]);
    }
  }

}

template <class TInputValue, class TOutputValue>
typename GMMMachineLearningModel<TInputValue,TOutputValue>
::TargetSampleType
GMMMachineLearningModel<TInputValue,TOutputValue>
::Predict(const InputSampleType & input, ConfidenceValueType *quality) const
{
  if (quality != NULL)
  {
    if (!this->HasConfidenceIndex())
    {
      itkExceptionMacro("Confidence index not available for this classifier !");
    }
  }

  itk::Array<MatrixValueType> input_c;
  input_c.SetSize(input.GetSize());
  // Compute decision function
  std::vector<MatrixValueType> decisionFct;
  for (int i = 0; i < m_classNb; ++i)
  {
    for (int j = 0; j < m_featNb; ++j)
      input_c[j]= input[j] - m_Means[i][j];

    itk::Array<MatrixValueType> lambdaQInputC = m_lambdaQ[i] * input_c;
    MatrixValueType decisionValue = 0;
    for (int j = 0; j < m_featNb; ++j)
      decisionValue += pow(lambdaQInputC[j],2);

    decisionFct.push_back( decisionValue );
  }

  int argmin = std::distance(decisionFct.begin(), std::min_element(decisionFct.begin(), decisionFct.end()));

  TargetSampleType res;
  res[0] = m_MapOfIndices.at(argmin);

  return res;
}

template <class TInputValue, class TOutputValue>
void
GMMMachineLearningModel<TInputValue,TOutputValue>
::Save(const std::string & filename, const std::string & name)
{
  // if (name == "")
  //   m_NormalBayesModel->save(filename.c_str(), 0);
  // else
  //   m_NormalBayesModel->save(filename.c_str(), name.c_str());
}

template <class TInputValue, class TOutputValue>
void
GMMMachineLearningModel<TInputValue,TOutputValue>
::Load(const std::string & filename, const std::string & name)
{
  // if (name == "")
  // else
}

template <class TInputValue, class TOutputValue>
bool
GMMMachineLearningModel<TInputValue,TOutputValue>
::CanReadFile(const std::string & file)
{
  // std::ifstream ifs;
  // ifs.open(file.c_str());

  // if(!ifs)
  // {
  //   std::cerr<<"Could not read file "<<file<<std::endl;
  //   return false;
  // }

  // while (!ifs.eof())
  // {
  //   std::string line;
  //   std::getline(ifs, line);

  //   if (line.find(CV_TYPE_NAME_ML_NBAYES) != std::string::npos)
  //   {
  //      //std::cout<<"Reading a "<<CV_TYPE_NAME_ML_NBAYES<<" model"<<std::endl;
  //      return true;
  //   }
  // }
  // ifs.close();
  return false;
}

template <class TInputValue, class TOutputValue>
bool
GMMMachineLearningModel<TInputValue,TOutputValue>
::CanWriteFile(const std::string & itkNotUsed(file))
{
  return false;
}

template <class TInputValue, class TOutputValue>
void
GMMMachineLearningModel<TInputValue,TOutputValue>
::PrintSelf(std::ostream& os, itk::Indent indent) const
{
  // Call superclass implementation
  Superclass::PrintSelf(os,indent);
}

} //end namespace otb

#endif
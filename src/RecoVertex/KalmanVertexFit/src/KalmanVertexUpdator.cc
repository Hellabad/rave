#include "RecoVertex/KalmanVertexFit/interface/KalmanVertexUpdator.h"
#include "DataFormats/GeometryVector/interface/GlobalVector.h"
#include "DataFormats/CLHEP/interface/AlgebraicObjects.h"
#include "RecoVertex/VertexPrimitives/interface/VertexException.h"
#include "DataFormats/GeometrySurface/interface/ReferenceCounted.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <algorithm>
#include <iostream>
// Based on the R.Fruhwirth et al Computer Physics Communications 96 (1996) 189-208
template <unsigned int N>
CachingVertex<N> KalmanVertexUpdator<N>::update(const  CachingVertex<N> & oldVertex,
	const RefCountedVertexTrack track, float weight, int sign) const
{
  if(abs(sign) != 1) throw VertexException
                          ("KalmanVertexUpdator::abs(sign) not equal to 1");

  VertexState newVertexState = positionUpdate(oldVertex.vertexState(), 
			  	track->linearizedTrack(), weight, sign);
  if (!newVertexState.isValid()) return CachingVertex<N>();

  float chi1 = oldVertex.totalChiSquared();
  pair <bool, double> chi2P = chi2Increment(oldVertex.vertexState(), newVertexState, 
                             track->linearizedTrack() , weight );
  if (!chi2P.first) return CachingVertex<N>(); // return invalid vertex

  chi1 +=sign * chi2P.second;

//adding or removing track from the CachingVertex::VertexTracks
  vector<RefCountedVertexTrack> newVertexTracks = oldVertex.tracks();

  if (sign > 0) {
    newVertexTracks.push_back(track);
  }else{

    typename vector<RefCountedVertexTrack>::iterator pos 
      = find(newVertexTracks.begin(), newVertexTracks.end(), track);
    if (pos != newVertexTracks.end()) {
      newVertexTracks.erase(pos);
    } else {
      cout<<"KalmanVertexUpdator::Unable to find requested track in the current vertex"<<endl;
      throw VertexException("KalmanVertexUpdator::Unable to find requested track in the current vertex");
    }
  }

  if  (oldVertex.hasPrior()) {
    return CachingVertex<N>( oldVertex.priorVertexState(),
                          newVertexState, newVertexTracks, chi1);
  } else {
    return CachingVertex<N>(newVertexState, newVertexTracks, chi1);
  }
}


template <unsigned int N>
CachingVertex<N> KalmanVertexUpdator<N>::add(const CachingVertex<N> & oldVertex, 
    const RefCountedVertexTrack track) const
{
  float weight = track->weight();
  return update(oldVertex,track,weight,+1);
}

template <unsigned int N>
CachingVertex<N> KalmanVertexUpdator<N>::remove(const CachingVertex<N> & oldVertex, 
    const RefCountedVertexTrack track) const
{
  float weight = track->weight();
  return update(oldVertex,track,weight,-1);
}


template <unsigned int N>
VertexState 
KalmanVertexUpdator<N>::positionUpdate (const VertexState & oldVertex,
	 const RefCountedLinearizedTrackState linearizedTrack, 
	 const float weight, int sign) const
{
  int error;

  const AlgebraicMatrixN3 & a = linearizedTrack->positionJacobian();
  const AlgebraicMatrixNM & b = linearizedTrack->momentumJacobian();

//   AlgebraicVectorN trackParameters = 
//   	linearizedTrack->predictedStateParameters();

  AlgebraicSymMatrixNN trackParametersWeight = 
  	linearizedTrack->predictedStateWeight(error);
  /*
  if(error != 0) {
    edm::LogError ( "KalmanVertexUpdator" ) << linearizedTrack->predictedStateError();
    edm::LogWarning("KalmanVertexUpdator") << "predictedState error matrix inversion failed. An invalid vertex will be returned.";
    return VertexState();
  }
    */


  // Jacobians
  //  edm::LogInfo("RecoVertex/KalmanVertexUpdator") 
  //    << "Now updating position" << "\n";

  //vertex information
//   AlgebraicSymMatrix33 oldVertexWeight = oldVertex.weight().matrix_new();
  AlgebraicSymMatrixMM s = ROOT::Math::SimilarityT(b,trackParametersWeight);
  error = ! s.Invert(); 
  /*
  if(error != 0) {
    edm::LogWarning("KalmanVertexUpdator") << "S matrix inversion failed. An invalid vertex will be returned.";
    return VertexState();
  }
  */

  AlgebraicSymMatrixNN gB = trackParametersWeight -
       ROOT::Math::Similarity(trackParametersWeight, ROOT::Math::Similarity(b,s));

// Getting the new covariance matrix of the vertex.

  AlgebraicSymMatrix33 newVertexWeight =  oldVertex.weight().matrix_new()
	+ weight * sign * ROOT::Math::SimilarityT(a,gB);
  //  edm::LogInfo("RecoVertex/KalmanVertexUpdator") 
  //    << "weight matrix" << newVertexWeight << "\n";


  AlgebraicVector3 newSwr =
                oldVertex.weightTimesPosition() + weight * sign * ROOT::Math::Transpose(a) * gB *
                ( linearizedTrack->predictedStateParameters() - linearizedTrack->constantTerm());
  //  edm::LogInfo("RecoVertex/KalmanVertexUpdator") 
  //    << "weighttimespos" << newSwr << "\n";

  VertexState newpos (newSwr, GlobalWeight(newVertexWeight), 1.0);

  //  edm::LogInfo("RecoVertex/KalmanVertexUpdator") 
  //    << "pos" << newpos.position() << "\n";

  return newpos;
}


template <unsigned int N>
pair <bool, double>  KalmanVertexUpdator<N>::chi2Increment(const VertexState & oldVertex, 
	const VertexState & newVertexState,
	const RefCountedLinearizedTrackState linearizedTrack, 
	float weight) const 
{
  int error;
  GlobalPoint newVertexPosition = newVertexState.position();

  AlgebraicVector3 newVertexPositionV;
  newVertexPositionV(0) = newVertexPosition.x();
  newVertexPositionV(1) = newVertexPosition.y();
  newVertexPositionV(2) = newVertexPosition.z();

  const AlgebraicMatrixN3 & a = linearizedTrack->positionJacobian();
  const AlgebraicMatrixNM & b = linearizedTrack->momentumJacobian();

  AlgebraicVectorN trackParameters = 
  	linearizedTrack->predictedStateParameters();

  AlgebraicSymMatrixNN trackParametersWeight = 
  	linearizedTrack->predictedStateWeight(error);
  /*
  if(error!=0) {
    cout << "aaartgg" << endl;
    edm::LogError ("KalmanVertexUpdator" ) << " error: " << linearizedTrack->predictedStateError();
    edm::LogWarning("KalmanVertexUpdator") << "predictedState error matrix inversion failed. An invalid vertex will be returned.";
    return pair <bool, double> (false, -1.);
  }*/

  AlgebraicSymMatrixMM s = ROOT::Math::SimilarityT(b,trackParametersWeight);
  error = ! s.Invert();
  if(error!=0) {
    edm::LogWarning("KalmanVertexUpdator") << "S matrix inversion failed. An invalid vertex will be returned.";
    return pair <bool, double> (false, -1.);
  }

  const AlgebraicVectorN & theResidual = linearizedTrack->constantTerm();
  AlgebraicVectorM newTrackMomentumP =  s * ROOT::Math::Transpose(b) * trackParametersWeight *
    (trackParameters - theResidual - a*newVertexPositionV);


//   AlgebraicVectorN rtp = ( theResidual +  a * newVertexPositionV + b * newTrackMomentumP);

  AlgebraicVectorN parameterResiduals = trackParameters -
	( theResidual +  a * newVertexPositionV + b * newTrackMomentumP);
  linearizedTrack->checkParameters(parameterResiduals);

  double chi2 = weight * ROOT::Math::Similarity(parameterResiduals, trackParametersWeight);

//   chi2 += vertexPositionChi2(oldVertex, newVertexPosition);
  chi2 += helper.vertexChi2(oldVertex, newVertexState);
  cout<<"Does this happen chi2 incr. "<<chi2<<endl; 
  return pair <bool, double> (true, chi2);
}

template class KalmanVertexUpdator<5>;
template class KalmanVertexUpdator<6>;

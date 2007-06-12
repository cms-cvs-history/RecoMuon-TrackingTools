/**
 *  Class: MuonTrackConverter
 *
 *  Description:
 *     Convert a reco::Track into a Trajectory by
 *     performing a refit
 *
 *
 *  $Date: 2007/04/30 15:24:13 $
 *  $Revision: 1.8 $ 
 *
 *  Authors :
 *  N. Neumeister            Purdue University
 *  A. Everett               Purdue University
 *
 **/

#include "RecoMuon/TrackingTools/interface/MuonTrackConverter.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "RecoMuon/TrackingTools/interface/MuonTrackReFitter.h"
#include "RecoMuon/TrackingTools/interface/MuonServiceProxy.h"

#include "TrackingTools/TransientTrackingRecHit/interface/TransientTrackingRecHitBuilder.h"
#include "RecoMuon/TransientTrackingRecHit/interface/MuonTransientTrackingRecHitBuilder.h"
#include "TrackingTools/Records/interface/TransientRecHitRecord.h"

#include "TrackingTools/TrackFitters/interface/TrajectoryStateWithArbitraryError.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

using namespace edm;
using namespace std;

//
// constructor
//
MuonTrackConverter::MuonTrackConverter(const edm::ParameterSet& par,const MuonServiceProxy *service): theService(service) {

  ParameterSet refitterPSet = par.getParameter<ParameterSet>("RefitterParameters");
  theRefitter = new MuonTrackReFitter(refitterPSet,theService);
  
  theTTRHBuilderName = par.getParameter<string>("TrackRecHitBuilder");

}


//
// destructor
//
MuonTrackConverter::~MuonTrackConverter() {

  if ( theRefitter ) delete theRefitter;
}

//
// convert a reco::TrackRef into a Trajectory
//
vector<Trajectory> MuonTrackConverter::convert(const reco::TrackRef& t) const {

  vector<Trajectory> result  = convert(*t);

  return result;

}


//
// convert a reco::Track into a Trajectory
//
vector<Trajectory> MuonTrackConverter::convert(const reco::Track& t) const {
  
  vector<Trajectory> result;
  
  // use TransientTrackingRecHitBuilder to get TransientTrackingRecHits
  ConstRecHitContainer hits = getTransientRecHits(t);
  if ( hits.empty() ) return result;
  
  // sort RecHits AlongMomentum
  if ( !hits.front()->isValid() ) return result;

  if ( hits.front()->geographicalId().det() == DetId::Tracker ) {
    reverse(hits.begin(),hits.end());
  }
  
  //printHits(hits);

  // use TransientTrackBuilder to get a starting TSOS
  TrajectoryStateTransform tsTransform;

  TrajectoryStateOnSurface firstState = tsTransform.innerStateOnSurface(t,*theService->trackingGeometry(), &*theService->magneticField());

  if ( hits.front()->geographicalId().det() == DetId::Tracker ) {
    firstState = theService->propagator(theRefitter->propagatorAlongMomentum())->propagate(firstState, 
											   hits.front()->det()->surface());
  }

  if ( !firstState.isValid() ) return result;

  AlgebraicSymMatrix55 C = AlgebraicMatrixID();
  C *= 10.;
  LocalTrajectoryParameters lp = firstState.localParameters();
  TrajectoryStateOnSurface theTSOS(lp,LocalTrajectoryError(C),
                                   firstState.surface(), 
                                   &*theService->magneticField());
  
  theTSOS = firstState;

  if ( hits.front()->geographicalId().det() == DetId::Tracker ) {
    theTSOS = TrajectoryStateWithArbitraryError()(firstState);
  }
  if ( !theTSOS.isValid() ) return result;

  const TrajectorySeed* seed = new TrajectorySeed();
  vector<Trajectory> trajs = theRefitter->trajectories(*seed,hits,theTSOS);
  if ( !trajs.empty()) result.push_back(trajs.front());
 
  return result;
  
}


//
// get container of transient RecHits from a Track
//
MuonTrackConverter::ConstRecHitContainer 
MuonTrackConverter::getTransientRecHits(const reco::Track& track) const {
  
  ESHandle<TransientTrackingRecHitBuilder> tkHitBuilder;
  theService->eventSetup().get<TransientRecHitRecord>().get(theTTRHBuilderName,tkHitBuilder);
  
  MuonTransientTrackingRecHitBuilder muHitBuilder;
  
  ConstRecHitContainer result;
  
  for (trackingRecHit_iterator iter = track.recHitsBegin(); iter != track.recHitsEnd(); ++iter) {
    if ( (*iter)->geographicalId().det() == DetId::Tracker ) {
      result.push_back(tkHitBuilder->build(&**iter));
    }
    else if ( (*iter)->geographicalId().det() == DetId::Muon ){
      result.push_back(muHitBuilder.build(&**iter,theService->trackingGeometry()));
    }
  }
  
  return result;
}


//
// get container of transient muon RecHits from a Track
//
MuonTrackConverter::ConstMuonRecHitContainer
MuonTrackConverter::getTransientMuonRecHits(const reco::Track& track) const {
  
   ConstMuonRecHitContainer result;
   for (trackingRecHit_iterator iter = track.recHitsBegin(); iter != track.recHitsEnd(); ++iter) {
     
     const TrackingRecHit* p = (*iter).get();
     const GeomDet* gd = theService->trackingGeometry()->idToDet(p->geographicalId());
     
      MuonTransientTrackingRecHit::MuonRecHitPointer mp = MuonTransientTrackingRecHit::specificBuild(gd,p);
      
      result.push_back(mp);
      
   }
   
   return result;
   
}


//
// print RecHits
//
void MuonTrackConverter::printHits(const ConstRecHitContainer& hits) const {

  LogInfo("MuonTrackConverter") << "Used RecHits: ";
  for (ConstRecHitContainer::const_iterator ir = hits.begin(); ir != hits.end(); ir++ ) {
    if ( !(*ir)->isValid() ) {
      LogInfo("MuonTrackConverter") << "invalid RecHit";
      continue;
    }

    const GlobalPoint& pos = (*ir)->globalPosition();
    LogInfo("MuonTrackConverter")
    << "r = " << sqrt(pos.x() * pos.x() + pos.y() * pos.y())     << "  z = " << pos.z()
    << "  dimension = " << (*ir)->dimension()
    << "  " << (*ir)->det()->geographicalId().det()
    << "  " << (*ir)->det()->subDetector();
  }

}

/**
 *  A selector for muon tracks
 *
 *  $Date: 2009/10/31 02:05:56 $
 *  $Revision: 1.27 $
 *  \author R.Bellan - INFN Torino
 */
#include "RecoMuon/TrackingTools/interface/MuonTrajectoryCleaner.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

using namespace std; 

void MuonTrajectoryCleaner::clean(TrajectoryContainer& trajC){ 
  const std::string metname = "Muon|RecoMuon|MuonTrajectoryCleaner";

  LogTrace(metname) << "Muon Trajectory Cleaner called" << endl;

  TrajectoryContainer::iterator iter, jter;
  Trajectory::DataContainer::const_iterator m1, m2;

  if ( trajC.size() < 2 ) return;
  
  LogTrace(metname) << "Number of trajectories in the container: " <<trajC.size()<< endl;

  int i(0), j(0);
  int match(0);

  // CAVEAT: vector<bool> is not a vector, its elements are not addressable!
  // This is fine as long as only operator [] is used as in this case.
  // cf. par 16.3.11
  vector<bool> mask(trajC.size(),true);
  
  TrajectoryContainer result;
  //  result.reserve(trajC.size());
  
  for ( iter = trajC.begin(); iter != trajC.end(); iter++ ) {
    if ( !mask[i] ) { i++; continue; }
    if ( !(*iter)->isValid() || (*iter)->empty() ) {mask[i] = false; i++; continue; }
    const Trajectory::DataContainer& meas1 = (*iter)->measurements();
    j = i+1;
    bool skipnext=false;
    for ( jter = iter+1; jter != trajC.end(); jter++ ) {
      if ( !mask[j] ) { j++; continue; }
      const Trajectory::DataContainer& meas2 = (*jter)->measurements();
      match = 0;
      for ( m1 = meas1.begin(); m1 != meas1.end(); m1++ ) {
	if( !(*m1).recHit()->isValid() )
	  continue;
        for ( m2 = meas2.begin(); m2 != meas2.end(); m2++ ) {
	  if( !(*m2).recHit()->isValid() )
	    continue;
	  if ( ( (*m1).recHit()->globalPosition() - (*m2).recHit()->globalPosition()).mag()< 10e-5 ) match++;
        }
      }
      

      // FIXME Set Boff/on via cfg!
      double chi2_dof_i = (*iter)->ndof() > 0 ? (*iter)->chiSquared()/(*iter)->ndof() : (*iter)->chiSquared()/1e-10;
      double chi2_dof_j = (*jter)->ndof() > 0 ? (*jter)->chiSquared()/(*jter)->ndof() : (*jter)->chiSquared()/1e-10;

      LogTrace(metname) 
	<< " MuonTrajectoryCleaner: trajC " 
	<< i << "(pT="<<(*iter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV)"
	<< " chi2/nDOF = " << (*iter)->chiSquared() << "/" << (*iter)->ndof() 
	<< " (RH=" << (*iter)->foundHits() << ") = " << chi2_dof_i
	<< " vs trajC " 
	<< j << "(pT="<<(*jter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV)"
	<< " chi2/nRH = " << (*jter)->chiSquared() << "/" <<  (*jter)->ndof() 
	<< " (RH=" << (*jter)->foundHits() << ") = " << chi2_dof_j
	<< " Shared RecHits: " << match; 

      int hit_diff =  (*iter)->foundHits() - (*jter)->foundHits() ;       
      // If there are matches, reject the worst track
      if ( match > 0 ) {
        // If the difference of # of rechits is less than 4, compare the chi2/ndf
        if ( abs(hit_diff) <= 4  ) {

	  double minPt = 3.5;
	  double dPt = 7.;  // i.e. considering 10% (conservative!) resolution at minPt it is ~ 10 sigma away from the central value

	  double maxFraction = 0.95;

       	  double fraction = (2.*match)/((*iter)->foundHits()+(*jter)->foundHits());
	  int belowLimit = 0;
	  int above = 0;

	  if((*jter)->lastMeasurement().updatedState().globalMomentum().perp() <= minPt) ++belowLimit; 
	  if((*iter)->lastMeasurement().updatedState().globalMomentum().perp() <= minPt) ++belowLimit; 
	 
	  if((*jter)->lastMeasurement().updatedState().globalMomentum().perp() >= dPt) ++above; 
	  if((*iter)->lastMeasurement().updatedState().globalMomentum().perp() >= dPt) ++above; 
	  

	  if(fraction >= maxFraction && belowLimit == 1 && above == 1){
	    if((*iter)->lastMeasurement().updatedState().globalMomentum().perp() < minPt){
	      mask[i] = false;
	      skipnext=true;
	      LogTrace(metname) << "Trajectory # " << i << " (pT="<<(*iter)->lastMeasurement().updatedState().globalMomentum().perp() 
				<< " GeV) rejected because it has too low pt";
	    } 
	    else {
	      mask[j] = false;
	      LogTrace(metname) << "Trajectory # " << j << " (pT="<<(*jter)->lastMeasurement().updatedState().globalMomentum().perp() 
				<< " GeV) rejected because it has too low pt";
	    }
	  }
	  else{   
	    if (chi2_dof_i  > chi2_dof_j) {
	      mask[i] = false;
	      skipnext=true;
	      LogTrace(metname) << "Trajectory # " << i << " (pT="<<(*iter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV) rejected";
	    }
	    else{
	      mask[j] = false;
	      LogTrace(metname) << "Trajectory # " << j << " (pT="<<(*jter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV) rejected";
	    }
	  }
	}
        else { // different number of hits
          if ( hit_diff < 0 ) {
            mask[i] = false;
            skipnext=true;
	    LogTrace(metname) << "Trajectory # " << i << " (pT="<<(*iter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV) rejected";
          }
          else { 
	    mask[j] = false;
	    LogTrace(metname) << "Trajectory # " << j << " (pT="<<(*jter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV) rejected";
	  }
        } 
      }
      if(skipnext) break;
      j++;
    }
    i++;
    if(skipnext) continue;
  }
  
  i = 0;
  for ( iter = trajC.begin(); iter != trajC.end(); iter++ ) {
    if ( mask[i] ){
      result.push_back(*iter);
      LogTrace(metname) << "Keep trajectory with pT = " << (*iter)->lastMeasurement().updatedState().globalMomentum().perp() << " GeV";
    }
    else delete *iter;
    i++;
  }
  
  trajC.clear();
  trajC = result;
}

//
// clean CandidateContainer
//
void MuonTrajectoryCleaner::clean(CandidateContainer& candC){ 
  const std::string metname = "Muon|RecoMuon|MuonTrajectoryCleaner";

  LogTrace(metname) << "Muon Trajectory Cleaner called" << endl;

  if ( candC.size() < 2 ) return;

  CandidateContainer::iterator iter, jter;
  Trajectory::DataContainer::const_iterator m1, m2;

  const float deltaEta = 0.01;
  const float deltaPhi = 0.01;
  const float deltaPt  = 1.0;
  
  LogTrace(metname) << "Number of muon candidates in the container: " <<candC.size()<< endl;

  int i(0), j(0);
  int match(0);
  bool directionMatch = false;

  // CAVEAT: vector<bool> is not a vector, its elements are not addressable!
  // This is fine as long as only operator [] is used as in this case.
  // cf. par 16.3.11
  vector<bool> mask(candC.size(),true);
  
  CandidateContainer result;
  
  for ( iter = candC.begin(); iter != candC.end(); iter++ ) {
    if ( !mask[i] ) { i++; continue; }
    const Trajectory::DataContainer& meas1 = (*iter)->trajectory()->measurements();
    j = i+1;
    bool skipnext=false;

    TrajectoryStateOnSurface innerTSOS;

    if ((*iter)->trajectory()->direction() == alongMomentum) {
      innerTSOS = (*iter)->trajectory()->firstMeasurement().updatedState();
    } 
    else if ((*iter)->trajectory()->direction() == oppositeToMomentum) { 
      innerTSOS = (*iter)->trajectory()->lastMeasurement().updatedState();
	 }
    if ( !(innerTSOS.isValid()) ) continue;
    float pt1 = innerTSOS.globalMomentum().perp();
    float eta1 = innerTSOS.globalMomentum().eta();
    float phi1 = innerTSOS.globalMomentum().phi();

    for ( jter = iter+1; jter != candC.end(); jter++ ) {
      if ( !mask[j] ) { j++; continue; }
      directionMatch = false;
      const Trajectory::DataContainer& meas2 = (*jter)->trajectory()->measurements();
      match = 0;
      for ( m1 = meas1.begin(); m1 != meas1.end(); m1++ ) {
        for ( m2 = meas2.begin(); m2 != meas2.end(); m2++ ) {
          if ( (*m1).recHit()->isValid() && (*m2).recHit()->isValid() ) 
	    if ( ( (*m1).recHit()->globalPosition() - (*m2).recHit()->globalPosition()).mag()< 10e-5 ) match++;
        }
      }
      
      LogTrace(metname) 
	<< " MuonTrajectoryCleaner: candC " << i << " chi2/nRH = " 
	<< (*iter)->trajectory()->chiSquared() << "/" << (*iter)->trajectory()->foundHits() <<
	" vs trajC " << j << " chi2/nRH = " << (*jter)->trajectory()->chiSquared() <<
	"/" << (*jter)->trajectory()->foundHits() << " Shared RecHits: " << match;

      TrajectoryStateOnSurface innerTSOS2;       
      if ((*jter)->trajectory()->direction() == alongMomentum) {
        innerTSOS2 = (*jter)->trajectory()->firstMeasurement().updatedState();
      }
      else if ((*jter)->trajectory()->direction() == oppositeToMomentum) {
        innerTSOS2 = (*jter)->trajectory()->lastMeasurement().updatedState();
      }
      if ( !(innerTSOS2.isValid()) ) continue;

      float pt2 = innerTSOS2.globalMomentum().perp();
      float eta2 = innerTSOS2.globalMomentum().eta();
      float phi2 = innerTSOS2.globalMomentum().phi();

      float deta(fabs(eta1-eta2));
      float dphi(fabs(Geom::Phi<float>(phi1)-Geom::Phi<float>(phi2)));
      float dpt(fabs(pt1-pt2));
      if ( dpt < deltaPt && deta < deltaEta && dphi < deltaPhi ) {
        directionMatch = true;
        LogTrace(metname)
        << " MuonTrajectoryCleaner: candC " << i<<" and "<<j<< " direction matched: "
        <<innerTSOS.globalMomentum()<<" and " <<innerTSOS2.globalMomentum();
      }

      // If there are matches, reject the worst track
      bool hitsMatch = ((match > 0) && ((match/((*iter)->trajectory()->foundHits()) > 0.25) || (match/((*jter)->trajectory()->foundHits()) > 0.25))) ? true : false;
      bool tracksMatch = 
      ( ( (*iter)->trackerTrack() == (*jter)->trackerTrack() ) && 
        ( deltaR<double>((*iter)->muonTrack()->eta(),(*iter)->muonTrack()->phi(), (*jter)->muonTrack()->eta(),(*jter)->muonTrack()->phi()) < 0.2) );

      //if ( ( tracksMatch ) || (hitsMatch > 0) || directionMatch ) {
			if ( ( tracksMatch ) || (hitsMatch > 0) ) { 
	if (  (*iter)->trajectory()->foundHits() == (*jter)->trajectory()->foundHits() ) {
          if ( (*iter)->trajectory()->chiSquared() > (*jter)->trajectory()->chiSquared() ) {
            mask[i] = false;
            skipnext=true;
          }
          else mask[j] = false;
        }
        else { // different number of hits
          if ( (*iter)->trajectory()->foundHits() < (*jter)->trajectory()->foundHits() ) {
	    mask[i] = false;
            skipnext=true;
          }
          else mask[j] = false;
	}
      }
      if(skipnext) break;
      j++;
    }
    i++;
    if(skipnext) continue;
  }
  
  i = 0;
  for ( iter = candC.begin(); iter != candC.end(); iter++ ) {
    if ( mask[i] ) {
       result.push_back(*iter);
    } else {
       delete (*iter)->trajectory();
       delete (*iter)->trackerTrajectory();
       delete *iter;
    } 
    i++;
  }
  
  candC.clear();
  candC = result;
}


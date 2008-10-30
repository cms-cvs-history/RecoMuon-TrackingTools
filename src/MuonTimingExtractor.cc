/**\class MuonTimingExtractor
 *
 * Description: <one line class summary>
 *
*/
//
// Original Author:  Traczyk Piotr
//         Created:  Thu Oct 11 15:01:28 CEST 2007
// $Id: MuonTimingExtractor.cc,v 1.1 2008/10/13 13:01:10 ptraczyk Exp $
//
//

#include "RecoMuon/TrackingTools/interface/MuonTimingExtractor.h"


// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "RecoMuon/TrackingTools/interface/MuonServiceProxy.h"

#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "DataFormats/MuonDetId/interface/MuonSubdetId.h"
#include "Geometry/DTGeometry/interface/DTGeometry.h"
#include "Geometry/DTGeometry/interface/DTLayer.h"
#include "Geometry/DTGeometry/interface/DTChamber.h"
#include "Geometry/DTGeometry/interface/DTSuperLayer.h"
#include "DataFormats/DTRecHit/interface/DTSLRecSegment2D.h"
#include "DataFormats/DTRecHit/interface/DTRecSegment2D.h"

#include "Geometry/Records/interface/GlobalTrackingGeometryRecord.h"
#include "Geometry/CommonDetUnit/interface/GlobalTrackingGeometry.h"

#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/MuonReco/interface/MuonFwd.h"
#include "DataFormats/DTRecHit/interface/DTRecSegment2D.h"
#include "DataFormats/DTRecHit/interface/DTRecSegment4DCollection.h"
#include "DataFormats/DTRecHit/interface/DTRecSegment2DCollection.h"
#include "DataFormats/TrackingRecHit/interface/TrackingRecHitFwd.h"
#include "RecoMuon/TransientTrackingRecHit/interface/MuonTransientTrackingRecHit.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/TrackExtra.h"
#include "DataFormats/TrackReco/interface/TrackExtraFwd.h"

                   

// system include files
#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace edm {
  class ParameterSet;
  class EventSetup;
  class InputTag;
}

class MuonServiceProxy;

using namespace std;
using namespace edm;
using namespace reco;

//
// constructors and destructor
//
MuonTimingExtractor::MuonTimingExtractor(const edm::ParameterSet& iConfig)
  :
  DTSegmentTags_(iConfig.getUntrackedParameter<edm::InputTag>("DTsegments")),
  theHitsMin(iConfig.getParameter<int>("HitsMin")),
  thePruneCut(iConfig.getParameter<double>("PruneCut")),
  debug(iConfig.getParameter<bool>("debug"))
{
  // service parameters
  ParameterSet serviceParameters = iConfig.getParameter<ParameterSet>("ServiceParameters");
  // the services
  theService = new MuonServiceProxy(serviceParameters);
  
  ParameterSet matchParameters = iConfig.getParameter<edm::ParameterSet>("MatchParameters");

  theMatcher = new MuonSegmentMatcher(serviceParameters, matchParameters, theService);

}


MuonTimingExtractor::~MuonTimingExtractor()
{
 
  if (theService) delete theService;

}


//
// member functions
//

// ------------ method called to produce the data  ------------
void
MuonTimingExtractor::fillTiming(edm::Event& iEvent, const edm::EventSetup& iSetup, reco::Muon& muon)
{

  //using namespace edm;
  using reco::TrackCollection;

  if (debug) 
    cout << " *** Muon Timimng Extractor ***" << endl;

  theService->update(iSetup);

  const GlobalTrackingGeometry *theTrackingGeometry = &*theService->trackingGeometry();
  
  Propagator *propag = &*theService->propagator("SteppingHelixPropagatorAny")->clone();

  if (debug) 
    cout << " Muon p = " << muon.p() << endl;

  //the associated muon track
  TrackRef muonTrack;
  if ((muon).combinedMuon().isNonnull()) muonTrack = (muon).combinedMuon();
    else {
      cout << " null Combined... " << endl;
      if ((muon).standAloneMuon().isNonnull()) muonTrack = (muon).standAloneMuon();
      else {
        cout << " null StandAlone... " << endl;
        return;
        }
      }

  double invbeta=0;
  double invbetaerr=0;
    int totalWeight=0;
    int nStations=0;
    vector<TimeMeasurement> tms;
    
    math::XYZPoint  pos=muonTrack->innerPosition();
    math::XYZVector mom=muonTrack->innerMomentum();
    GlobalPoint  posp(pos.x(), pos.y(), pos.z());
    GlobalVector momv(mom.x(), mom.y(), mom.z());
    FreeTrajectoryState muonFTS(posp, momv, (TrackCharge)muonTrack->charge(), theService->magneticField().product());

    if (debug) 
      cout << " STA Track:   RecHits: " << (*muonTrack).recHitsSize() 
           << " momentum: " << (*muonTrack).p() << endl;

    // get the DT segments that were used to construct the muon
    vector<const DTRecSegment4D*> range = theMatcher->matchDT(*(muon.standAloneMuon()),iEvent);

    // create a collection on TimeMeasurements for the track        
    for (vector<const DTRecSegment4D*>::iterator rechit = range.begin(); rechit!=range.end();++rechit) {

      // Create the ChamberId
      DetId id = (*rechit)->geographicalId();
      DTChamberId chamberId(id.rawId());
      DTLayerId layerId(id.rawId());
      DTSuperLayerId slayerId(id.rawId());
      int station = chamberId.station();

      // use only segments with both phi and theta projections present
      if ( (!(*rechit)->hasPhi()) || (!(*rechit)->hasZed()) ) continue;

      // loop over (theta, phi) segments
      for (int phi=0; phi<2; phi++) {

	const DTRecSegment2D* segm;
	if (phi) segm = dynamic_cast<const DTRecSegment2D*>((*rechit)->phiSegment()); 
	else segm = dynamic_cast<const DTRecSegment2D*>((*rechit)->zSegment());
	if(segm == 0){cout << "skip this" << endl;  continue; }   
	if (!segm->specificRecHits().size()) continue;

	const GeomDet* geomDet = theTrackingGeometry->idToDet(segm->geographicalId());
	const vector<DTRecHit1D> hits1d = segm->specificRecHits();

	// store all the hits from the segment
	for (vector<DTRecHit1D>::const_iterator hiti=hits1d.begin(); hiti!=hits1d.end(); hiti++) {

	  const GeomDet* dtcell = theTrackingGeometry->idToDet(hiti->geographicalId());
	  TimeMeasurement thisHit;

	  thisHit.driftCell = hiti->geographicalId();
	  if (hiti->lrSide()==DTEnums::Left) thisHit.isLeft=true; else thisHit.isLeft=false;
	  thisHit.isPhi = phi;
	  thisHit.posInLayer = geomDet->toLocal(dtcell->toGlobal(hiti->localPosition())).x();
	  thisHit.distIP = dtcell->toGlobal(hiti->localPosition()).mag();
	  thisHit.station = station;
	  tms.push_back(thisHit);
	}

      } // phi = (0,1) 	        
    } // rechit

      
    bool modified = false;
    totalWeight=0;
    nStations=0;
    vector <double> dstnc, dsegm, dtraj, hitWeight, left;
    
    // Now loop over the measurements, calculate 1/beta and cut away outliers
    do {    

      modified = false;
      dstnc.clear();
      dsegm.clear();
      dtraj.clear();
      hitWeight.clear();
      left.clear();
      
      vector <int> hit_idx;
      totalWeight=0;
      nStations=0;
      
      // Rebuild segments
      for (int sta=1;sta<5;sta++)
        for (int phi=0;phi<2;phi++) {
          vector <TimeMeasurement> seg;
          vector <int> seg_idx;
          int tmpos=0;
          for (vector<TimeMeasurement>::iterator tm=tms.begin(); tm!=tms.end(); ++tm) {
            if ((tm->station==sta) && (tm->isPhi==phi)) {
              seg.push_back(*tm);
              seg_idx.push_back(tmpos);
            }
            tmpos++;  
          }
          
          unsigned int segsize = seg.size();
          
          if (segsize<theHitsMin) continue;

          double a=0, b=0;
          vector <double> hitxl,hitxr,hityl,hityr;

          for (vector<TimeMeasurement>::iterator tm=seg.begin(); tm!=seg.end(); ++tm) {
 
            DetId id = tm->driftCell;
            const GeomDet* dtcell = theTrackingGeometry->idToDet(id);
            DTChamberId chamberId(id.rawId());
            const GeomDet* dtcham = theTrackingGeometry->idToDet(chamberId);

            double celly=dtcham->toLocal(dtcell->position()).z();
            
            if (tm->isLeft) {
	      hitxl.push_back(celly);
	      hityl.push_back(tm->posInLayer);
            } else {
	      hitxr.push_back(celly);
	      hityr.push_back(tm->posInLayer);
            }    
          }

          if (!fitT0(a,b,hitxl,hityl,hitxr,hityr)) {
            if (debug)
              cout << "     t0 = zero, Left hits: " << hitxl.size() << " Right hits: " << hitxr.size() << endl;
            continue;
          }
          
          // a segment must have at least one left and one right hit
          if ((!hitxl.size()) || (!hityl.size())) continue;

          nStations++;

          int segidx=0;
          for (vector<TimeMeasurement>::const_iterator tm=seg.begin(); tm!=seg.end(); ++tm) {

            DetId id = tm->driftCell;
            const GeomDet* dtcell = theTrackingGeometry->idToDet(id);
            DTChamberId chamberId(id.rawId());
            const GeomDet* dtcham = theTrackingGeometry->idToDet(chamberId);

            double dist = tm->distIP;
            double layerZ  = dtcham->toLocal(dtcell->position()).z();
            double segmLocalPos = b+layerZ*a;
            double hitLocalPos = tm->posInLayer;
            int hitSide = -tm->isLeft*2+1;
            double t0_segm = (-(hitSide*segmLocalPos)+(hitSide*hitLocalPos))/0.00543;
            
            std::pair< TrajectoryStateOnSurface, double> tsos;
            tsos=propag->propagateWithPath(muonFTS,dtcell->surface());
            
            if (tsos.first.isValid()) dist = tsos.second+posp.mag();
            
            if ((debug) || (fabs(dist)>1500.)) {
              cout << " Dist: " << dist << "   segm: " << segmLocalPos << "   hit: " << hitLocalPos;
              if (tsos.first.isValid()) cout << " traj: " << dtcham->toLocal(tsos.first.globalPosition()) << " path: " << tsos.second+posp.mag();
              cout << " Start: " << posp;
              cout << endl;
            }

            dstnc.push_back(dist);
            dsegm.push_back(t0_segm);
            left.push_back(hitSide);
            hitWeight.push_back(((double)seg.size()-2.)/(double)seg.size());
            hit_idx.push_back(seg_idx.at(segidx));
            segidx++;
          }
          
          totalWeight+=seg.size()-2;

        }

      nStations/=2;
      invbeta=0;

      // calculate the value and error of 1/beta from the complete set of 1D hits
      if (debug)
	cout << " Points for global fit: " << dstnc.size() << endl;

      // inverse beta - weighted average of the contributions from individual hits
      for (unsigned int i=0;i<dstnc.size();i++) {
	invbeta+=(1.+dsegm.at(i)/dstnc.at(i)*30.)*hitWeight.at(i)/totalWeight;
	//          cout << "    Dstnc: " << dstnc.at(i) << "   delta t0(hit-segment): " << dsegm.at(i) << "   weight: " << hitWeight.at(i); 
	//          cout << " Local 1/beta: " << 1.+dsegm.at(i)/dstnc.at(i)*30. << endl;
      }

      double chimax=0.;
      vector<TimeMeasurement>::iterator tmmax;
    
      // the dispersion of inverse beta
      double diff;
      for (unsigned int i=0;i<dstnc.size();i++) {
	diff=(1.+dsegm.at(i)/dstnc.at(i)*30.)-invbeta;
	diff=diff*diff*hitWeight.at(i);
	invbetaerr+=diff;
	if (diff/totalWeight/totalWeight>chimax/100.) { 
	  tmmax=tms.begin()+hit_idx.at(i);
	  chimax=diff;
	}
      }
    
      invbetaerr=sqrt(invbetaerr/totalWeight);
 
      // cut away the outliers
      if (chimax>thePruneCut) {
	tms.erase(tmmax);
	modified=true;
      }    

      if (debug)
	cout << " Measured 1/beta: " << invbeta << " +/- " << invbetaerr << endl;

//      tof.invBeta=invbeta;
//      tof.invBetaErr=invbetaerr;      

    } while (modified);

    // unconstrained fit to the full set of points
    vector <double> x,y;
    double freeBeta, freeBetaErr, freeTime, freeTimeErr, vertexTime=0, vertexTimeErr=0;    

    for (unsigned int i=0;i<dstnc.size();i++) {
      x.push_back(dstnc.at(i)/30.);
      y.push_back(dsegm.at(i)+dstnc.at(i)/30.);
      vertexTime+=dsegm.at(i)*hitWeight.at(i)/totalWeight;
    }

    double diff;
    for (unsigned int i=0;i<dstnc.size();i++) {
      diff=dsegm.at(i)-vertexTime;
      vertexTimeErr+=diff*diff*hitWeight.at(i);
    }
    vertexTimeErr=sqrt(vertexTimeErr/totalWeight);

    reco::MuonTime muonTime;

   muonTime.inverseBeta=invbeta;                        
   muonTime.inverseBetaErr=invbetaerr;  
   muonTime.timeAtIpInOut = vertexTime;
   muonTime.timeAtIpInOutErr = vertexTimeErr;
//   muonTime.timeAtIpOutIn = outInVertexTime;
//   muonTime.timeAtIpOutInErr = sqrt(outInVertexTime2 - outInVertexTime*outInVertexTime);
      
    rawFit(freeBeta, freeBetaErr, freeTime, freeTimeErr, x, y);

  muonTime.freeInverseBeta = freeBeta;	  
  muonTime.freeInverseBetaErr = freeBetaErr;	  
    
//    tof.nHits = dstnc.size();
  muonTime.nStations=nStations;

  muon.setTime(muonTime);

  if (debug) {
      cout << " Free 1/beta: " << freeBeta << " +/- " << freeBetaErr << endl;   
      cout << "   Free time: " << freeTime << " +/- " << freeTimeErr << endl;   
      cout << " Vertex time: " << vertexTime << " +/- " << freeTimeErr << endl;   
    cout << " *** Muon Timing Extractor End ***" << endl;
  }
}

double
MuonTimingExtractor::fitT0(double &a, double &b, vector<double> xl, vector<double> yl, vector<double> xr, vector<double> yr ) {

  double sx=0,sy=0,sxy=0,sxx=0,ssx=0,ssy=0,s=0,ss=0;

  for (unsigned int i=0; i<xl.size(); i++) {
    sx+=xl[i];
    sy+=yl[i];
    sxy+=xl[i]*yl[i];
    sxx+=xl[i]*xl[i];
    s++;
    ssx+=xl[i];
    ssy+=yl[i];
    ss++;
  } 

  for (unsigned int i=0; i<xr.size(); i++) {
    sx+=xr[i];
    sy+=yr[i];
    sxy+=xr[i]*yr[i];
    sxx+=xr[i]*xr[i];
    s++;
    ssx-=xr[i];
    ssy-=yr[i];
    ss--;
  } 

  double delta = ss*ss*sxx+s*sx*sx+s*ssx*ssx-s*s*sxx-2*ss*sx*ssx;
  
  double t0_corr=0.;

  if (delta) {
    a=(ssy*s*ssx+sxy*ss*ss+sy*sx*s-sy*ss*ssx-ssy*sx*ss-sxy*s*s)/delta;
    b=(ssx*sy*ssx+sxx*ssy*ss+sx*sxy*s-sxx*sy*s-ssx*sxy*ss-sx*ssy*ssx)/delta;
    t0_corr=(ssx*s*sxy+sxx*ss*sy+sx*sx*ssy-sxx*s*ssy-sx*ss*sxy-ssx*sx*sy)/delta;
  }

  // convert drift distance to time
  t0_corr/=-0.00543;

  return t0_corr;
}


void 
MuonTimingExtractor::rawFit(double &a, double &da, double &b, double &db, const vector<double> hitsx, const vector<double> hitsy) {

  double s=0,sx=0,sy=0,x,y;
  double sxx=0,sxy=0;

  a=b=0;
  if (hitsx.size()==0) return;
  if (hitsx.size()==1) {
    b=hitsy[0];
  } else {
    for (unsigned int i = 0; i != hitsx.size(); i++) {
      x=hitsx[i];
      y=hitsy[i];
      sy += y;
      sxy+= x*y;
      s += 1.;
      sx += x;
      sxx += x*x;
    }

    double d = s*sxx - sx*sx;
    b = (sxx*sy- sx*sxy)/ d;
    a = (s*sxy - sx*sy) / d;
    da = sqrt(sxx/d);
    db = sqrt(s/d);
  }
}


//define this as a plug-in
//DEFINE_FWK_MODULE(MuonTimingExtractor);
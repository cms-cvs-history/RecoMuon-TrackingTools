#ifndef RecoMuon_TrackingTools_MuonBestMeasurementFinder_H
#define RecoMuon_TrackingTools_MuonBestMeasurementFinder_H

/** \class MuonBestMeasurementFinder
 *  Algorithmic class to get best measurement from a list of TM
 *
 *  $Date: 2006/05/23 17:46:07 $
 *  $Revision: 1.1 $
 *  \author R. Bellan - INFN Torino <riccardo.bellan@cern.ch>
 *  \author S. Lacaprara - INFN Legnaro <stefano.lacaprara@pd.infn.it>
 */

#include <vector>

class Propagator;
class MeasurementEstimator;
class TrajectoryMeasurement;

class MuonBestMeasurementFinder {
  typedef std::vector<TrajectoryMeasurement>    TMContainer;
  typedef TMContainer::iterator                 TMIterator;

public:
  
  /// Constructor
  MuonBestMeasurementFinder(Propagator* prop);

  /// Destructor
  virtual ~MuonBestMeasurementFinder();

  // Operations

   /// return the Tm with the best chi2: no cut applied.
  TrajectoryMeasurement* findBestMeasurement(TMContainer& measC);

  /// OLD ORCA algo. Reported for timing comparison pourpose
  /// Will be removed after the comparison!
  TrajectoryMeasurement* findBestMeasurement_OLD(TMContainer& measC);
  
  const Propagator* propagator() const { return thePropagator;}
  const MeasurementEstimator* estimator() const { return theEstimator;}

protected:

private:

  const Propagator* thePropagator;
  const MeasurementEstimator* theEstimator;

};
#endif


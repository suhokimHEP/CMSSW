// Last commit: $Id: SiStripConfigDb.cc,v 1.5 2006/06/09 13:15:43 bainbrid Exp $
// Latest tag:  $Name:  $
// Location:    $Source: /cvs_server/repositories/CMSSW/CMSSW/OnlineDB/SiStripConfigDb/src/SiStripConfigDb.cc,v $

#include "OnlineDB/SiStripConfigDb/interface/SiStripConfigDb.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DataFormats/SiStripDetId/interface/SiStripControlKey.h"
#include "CondFormats/SiStripObjects/interface/SiStripFedCabling.h"
#include "CondFormats/SiStripObjects/interface/FedChannelConnection.h"
#include "CalibFormats/SiStripObjects/interface/SiStripFecCabling.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

// -----------------------------------------------------------------------------
// Constructors, destructors ---------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
SiStripConfigDb::SiStripConfigDb( string user, 
				  string passwd, 
				  string path ) : 
  factory_(0), 
  user_( user ), passwd_( passwd ), path_( path ), partition_(),
  usingDb_(true), 
  inputModuleXml_(""), inputDcuInfoXml_(""), inputFecXml_(), inputFedXml_(),
  devices_(), piaResets_(), feds_(), connections_(), dcuDetIdMap_(),
  resetDevices_(true), resetPiaResets_(true), resetFeds_(true), resetConnections_(true), resetDcuDetIdMap_(true),
  usingStrips_(true)
{
  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::SiStripConfigDb]"
			   << " Using configuration database...";
}

// -----------------------------------------------------------------------------
// 
SiStripConfigDb::SiStripConfigDb( string input_module_xml,
				  string input_dcuinfo_xml,
				  vector<string> input_fec_xmls,
				  vector<string> input_fed_xmls,
				  string output_module_xml,
				  string output_dcuinfo_xml,
				  vector<string> output_fec_xmls,
				  vector<string> output_fed_xmls ) : 
  factory_(0), 
  user_(""), passwd_(""), path_(""), partition_(),
  usingDb_(false), 
  inputModuleXml_( input_module_xml ), inputDcuInfoXml_( input_dcuinfo_xml ), inputFecXml_( input_fec_xmls ), inputFedXml_( input_fed_xmls ),
  outputModuleXml_( output_module_xml ), outputDcuInfoXml_( output_dcuinfo_xml ), outputFecXml_( output_fec_xmls ), outputFedXml_( output_fed_xmls ),
  devices_(), piaResets_(), feds_(), connections_(), dcuDetIdMap_(),
  resetDevices_(true), resetPiaResets_(true), resetFeds_(true), resetConnections_(true), resetDcuDetIdMap_(true),
  usingStrips_(true)
{
  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::SiStripConfigDb]"
			   << " Using XML files...";
}

// -----------------------------------------------------------------------------
//
SiStripConfigDb::~SiStripConfigDb() {
  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::~SiStripConfigDb]"
			   << " Destructing object...";
}

// -----------------------------------------------------------------------------
// Database connection and local cache  ----------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
bool SiStripConfigDb::openDbConnection() {
  edm::LogInfo("FedCabling") << "[SiStripConfigDb::openDbConnection]";
  cout << "[SiStripConfigDb::openDbConnection]" << endl;

  try {

    // Create device factory object
    factory_ = new DeviceFactory(); 
    if ( !deviceFactory("SiStripConfigDb::openDbConnection") ) { return false; }
    deviceFactory()->setUsingDb( usingDb_ ); //@@ necessary?
    
    edm::LogInfo("ConfigDb") << "[SiStripConfigDb::openDbConnection]"
			     << " DeviceFactory created at address " << deviceFactory();

    // Initialise IO 
    if ( usingDb_ ) { if ( usingDatabase() ) { return false; } }
    else { if ( usingXmlFiles() ) { return false; } }

    // Refresh local cache
    //refreshLocalCache();
    
  } 
  catch (...) { handleSqlException( "SiStripConfigDb::openDbConnection" ); }
  
  return true;
}

// -----------------------------------------------------------------------------
//
bool SiStripConfigDb::closeDbConnection() {
  edm::LogInfo("FedCabling") << "[SiStripConfigDb::closeDbConnection]";
  try { 
    if ( factory_ ) { 
      delete factory_; 
      factory_ = 0; 
    } 
  }
  catch (...) { handleSqlException( "SiStripConfigDb::closeDbConnection" ); }
  return true;
}

// -----------------------------------------------------------------------------
//
void SiStripConfigDb::updateLocalCaches() {
  LogDebug("FedCabling") << "[SiStripConfigDb::updateLocalCaches]";
  // Reset all local caches
  resetDeviceDescriptions();
  resetFedDescriptions();
  resetFedConnections();
  resetPiaResetDescriptions();
  // Retrieve descriptions from DB or xml files
  getDeviceDescriptions();
  getFedDescriptions();
  getFedConnections();
  getPiaResetDescriptions();
}

// -----------------------------------------------------------------------------
// Front-end devices -----------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::DeviceDescriptions& SiStripConfigDb::getDeviceDescriptions( const enumDeviceType& device_type,
										   bool all_devices_except ) {
  // Use static object to hold device description of a particular type
  static SiStripConfigDb::DeviceDescriptions descriptions;
  descriptions.clear();

  DeviceDescriptions::iterator idevice = devices_.begin();
  for ( ; idevice != devices_.end(); idevice++ ) {
    deviceDescription* desc = *idevice;
    // Extract devices of given type from descriptions found in local cache  
    if ( !all_devices_except && desc->getDeviceType() == device_type ) { descriptions.push_back( desc ); }
    // Extract all devices EXCEPT those of given type from descriptions found in local cache  
    if ( all_devices_except && desc->getDeviceType() != device_type ) { descriptions.push_back( desc ); }
  }
  
  if ( descriptions.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::getDeviceDescriptions]"
			      << " No descriptions found!";
  }
  
  return descriptions;
}

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::DeviceDescriptions& SiStripConfigDb::getDeviceDescriptions() {

  if ( !deviceFactory("SiStripConfigDb::getDeviceDescriptions") ) { return devices_; }

  if ( !resetDevices_ ) { return devices_; }

  try { 
    
    if ( !usingDb_ ) {
      resetPiaResetDescriptions();
      getPiaResetDescriptions();
    }
    
    deviceFactory()->getFecDeviceDescriptions( partition_.name_, 
					       devices_,
					       partition_.major_,
					       partition_.minor_ );
    deviceFactory()->getDcuDescriptions( partition_.name_, 
					 devices_,
					 0, 999999 ); // timestamp start/stop
    resetDevices_ = false;
    
  }
  catch (...) { handleFecException( "SiStripConfigDb::getDeviceDescriptions" ); }
  
  if ( devices_.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::getDeviceDescriptions]"
			      << " No device descriptions found!";
  }

  return devices_;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::resetDeviceDescriptions() {
  //FecFactory::deleteVector( devices_ );
  devices_.clear();
  resetDevices_ = true;
}

// -----------------------------------------------------------------------------
// 
//@@ if new major, upload all desc. if not, upload just modified ones... ???
void SiStripConfigDb::uploadDeviceDescriptions( bool new_major_version ) {

  if ( !deviceFactory("SiStripConfigDb::uploadDeviceDescriptions") ) { return; }

  try { 

    if ( !usingDb_ ) {
      deviceFactory()->setPiaResetDescriptions( piaResets_, 
						partition_.name_ );
    }
    
    deviceFactory()->setFecDeviceDescriptions( getDeviceDescriptions( DCU, true ), // all devices except DCUs
					       partition_.name_, 
					       &partition_.major_,
					       &partition_.minor_,
					       new_major_version );

    deviceFactory()->setDcuDescriptions( partition_.name_, 
					 getDeviceDescriptions( DCU ) );

  }
  catch (...) { 
    handleFecException( "SiStripConfigDb::getDeviceDescriptions" ); 
  }
  
}

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::DeviceAddress& SiStripConfigDb::deviceAddress( const deviceDescription& description ) {

  // Set default values
  static SiStripConfigDb::DeviceAddress addr;
  static uint16_t all_ = 0xFFFF;
  addr.fecCrate_ = all_;
  addr.fecSlot_  = all_;
  addr.fecRing_  = all_;
  addr.ccuAddr_  = all_;
  addr.ccuChan_  = all_;
  addr.i2cAddr_  = all_;
  
  // Retrieve FEC key
  keyType key;
  try { key = const_cast<deviceDescription&>(description).getKey(); }
  catch (...) { handleFecException( "SiStripConfigDb::deviceAddress" ); }
  
  // Extract hardware addresses
  addr.fecCrate_ = static_cast<uint16_t>( 0 ); //@@ getCrateKey(key) );
  addr.fecSlot_  = static_cast<uint16_t>( getFecKey(key) );
  addr.fecRing_  = static_cast<uint16_t>( getRingKey(key) );
  addr.ccuAddr_  = static_cast<uint16_t>( getCcuKey(key) );
  addr.ccuChan_  = static_cast<uint16_t>( getChannelKey(key) );
  addr.i2cAddr_  = static_cast<uint16_t>( getAddressKey(key) );
  
  return addr;
}

// -----------------------------------------------------------------------------
// Front-end driver ------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::FedDescriptions& SiStripConfigDb::getFedDescriptions() {

  if ( !deviceFactory("SiStripConfigDb::uploadDeviceDescriptions") ) { return feds_; }

  if ( !resetFeds_ ) { return feds_; }

  try {
    deviceFactory()->setUsingStrips( usingStrips_ );
    feds_ = *( deviceFactory()->getFed9UDescriptions( partition_.name_, 
						      partition_.major_, 
						      partition_.minor_ ) );
    resetFeds_ = false;
  }
  catch (... ) {
    handleFedException( "SiStripConfigDb::getFedDescriptions" );
  }
  
  if ( feds_.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::getFedDescriptions]"
			      << " No FED descriptions found!";
  }

  return feds_;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::resetFedDescriptions() {
  //if ( !deviceFactory("SiStripConfigDb::resetFedDescriptions") ) { return; }
  //deviceFactory()->clearDescriptions();
  feds_.clear();
  resetFeds_ = true;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::uploadFedDescriptions( bool new_major_version ) { //@@ this ok???
  
  if ( !deviceFactory("SiStripConfigDb::uploadFedDescriptions") ) { return; }

  try { 
    if ( !usingDb_ ) {
      //deviceFactory()->addFedOutputFileName( "/tmp/fec.xml" ); //@@ ???
    }
    SiStripConfigDb::FedDescriptions::iterator ifed = feds_.begin();
    for ( ; ifed != feds_.end(); ifed++ ) {
      deviceFactory()->setFed9UDescription( **ifed, 
					    (uint16_t*)(&partition_.major_), 
					    (uint16_t*)(&partition_.minor_),
					    (new_major_version?1:0) );
    }
  }
  catch (...) { 
    handleFecException( "SiStripConfigDb::getDeviceDescriptions" ); 
  }
  
}

// -----------------------------------------------------------------------------
/** */ 
const vector<uint16_t>& SiStripConfigDb::getFedIds() {
  
  static vector<uint16_t> fed_ids;
  fed_ids.clear();

  getFedDescriptions();
  FedDescriptions::iterator ifed = feds_.begin();
  for ( ; ifed != feds_.end(); ifed++ ) { 
    fed_ids.push_back( (*ifed)->getFedId() );
  }
  if ( fed_ids.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::getFedIds]"
			      << " No FED ids found!"; 
  }
  return fed_ids;
}

// -----------------------------------------------------------------------------
// Fed connections -------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::FedConnections& SiStripConfigDb::getFedConnections() {

  if ( !deviceFactory("SiStripConfigDb::getFedConnections") ) { return connections_; }

  if ( !resetConnections_ ) { return connections_; }

  try {
    if ( usingDb_ ) { 
      deviceFactory()->setInputDBVersion( partition_.name_,
					  partition_.major_,
					  partition_.minor_ );
    }
    for ( int iconn = 0; iconn < deviceFactory()->getNumberOfFedChannel(); iconn++ ) {
      connections_.push_back( deviceFactory()->getFedChannelConnection( iconn ) ); 
    }
    resetConnections_ = false;
  }
  catch (...) { handleFecException( "SiStripConfigDb::getFedConnections" ); }

  if ( connections_.empty() ) {
    stringstream ss; ss << "No FED connections found";
    if ( !usingDb_ ) { ss << " in input xml file " << inputModuleXml_; }
    else { ss << " in database partition " << partition_.name_; }
    edm::LogError("ConfigDb") << "[SiStripConfigDb::fedConnections]" << ss.str();
  }

  return connections_;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::resetFedConnections() {
  //if ( !deviceFactory("SiStripConfigDb::resetFedConnections") ) { return; }
  //deviceFactory()->getTrackerParser()->purge(); 
  connections_.clear(); 
  resetConnections_ = true;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::uploadFedConnections( bool new_major_version ) {

  if ( !deviceFactory("SiStripConfigDb::uploadFedConnections") ) { return; }

  try { 

    if ( usingDb_ ) {
      deviceFactory()->setInputDBVersion( partition_.name_, //@@ ???
					  partition_.major_,
					  partition_.minor_ );
    }

    SiStripConfigDb::FedConnections::iterator ifed = connections_.begin();
    for ( ; ifed != connections_.end(); ifed++ ) {
      deviceFactory()->addFedChannelConnection( *ifed );
    }
    deviceFactory()->upload();

  }
  catch (...) {
    handleFecException( "SiStripConfigDb::getFedConnections" );
  }

}

// -----------------------------------------------------------------------------
// DCU info --------------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::DcuDetIdMap& SiStripConfigDb::getDcuDetIdMap() {

  if ( !deviceFactory("SiStripConfigDb::getDcuDetIdMap") ) { return dcuDetIdMap_; }
  if ( !resetDcuDetIdMap_ ) { return dcuDetIdMap_; }
  
  try {
    //     if ( !usingDb_ ) {
    //       if ( inputDcuInfoXml_ != "" ) { 
    dcuDetIdMap_ = deviceFactory()->getInfos(); 
    //       }
    //     }
    resetDcuDetIdMap_ = false;
  }
  catch (... ) {
    handleSqlException( "SiStripConfigDb::getDcuDetIdMap" );
  }
  return dcuDetIdMap_;
}

// -----------------------------------------------------------------------------
//
void SiStripConfigDb::resetDcuDetIdMap() {
  //if ( !deviceFactory("SiStripConfigDb::resetDcuDetIdMap") ) { return; }
  //deviceFactory()->deleteHashMapTkDcuInfo();
  dcuDetIdMap_.clear(); 
  resetDcuDetIdMap_ = true;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::uploadDcuDetIdMap() {

  if ( !deviceFactory("SiStripConfigDb::uploadDcuDetIdMap") ) { return; }

  try {
  }
  catch (... ) {
    handleSqlException( "SiStripConfigDb::uploadDcuDetIdMap" );
  }

}

// -----------------------------------------------------------------------------
// 
const SiStripConfigDb::PiaResetDescriptions& SiStripConfigDb::getPiaResetDescriptions() {
  
  if ( !deviceFactory("SiStripConfigDb::getPiaResetDescriptions") ) { return piaResets_; }

  if ( !resetPiaResets_ ) { return piaResets_; }
  
  try { 
    deviceFactory()->getPiaResetDescriptions( partition_.name_, piaResets_ );
    resetPiaResets_ = false;
  }
  catch (...) { handleFecException( "SiStripConfigDb::getPiaResetDescriptions" ); }
  if ( piaResets_.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::getPiaResetDescriptions]"
			      << " No PIA reset descriptions found!";
  }
  
  return piaResets_;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::resetPiaResetDescriptions() {
  //FecFactory::deleteVector( piaResets_ );
  piaResets_.clear();
  resetPiaResets_ = true;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::uploadPiaResetDescriptions() {
  
  if ( !deviceFactory("SiStripConfigDb::uploadPiaResetDescriptions") ) { return; }
  
  try { 
    deviceFactory()->setPiaResetDescriptions( piaResets_, 
					      partition_.name_ );
  }
  catch (... ) { handleSqlException( "SiStripConfigDb::uploadPiaResetDescriptions" ); }
  
}

// -----------------------------------------------------------------------------
// Miscellaneous methods -------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::createDescriptions( const SiStripFecCabling& fec_cabling,
					  DeviceDescriptions& devices,
					  PiaResetDescriptions pia_resets,
					  DcuConversions dcu_convs ) {
  // Clear contents
  devices.clear(); //@@ delete??? cached?
  pia_resets.clear();
  dcu_convs.clear();

  // Default settings for APV, DOH LLD, AOH LLD, APV MUX and PLL
  apvDescription apv_default( (uint8_t)0x2B,
			      (uint8_t)0x64,
			      (uint8_t)0x4,
			      (uint8_t)0x73,
			      (uint8_t)0x3C,
			      (uint8_t)0x32,
			      (uint8_t)0x32,
			      (uint8_t)0x32,
			      (uint8_t)0x50,
			      (uint8_t)0x32,
			      (uint8_t)0x50,
			      (uint8_t)0,
			      (uint8_t)0x43,
			      (uint8_t)0x43,
			      (uint8_t)0x14,
			      (uint8_t)0xFB,
			      (uint8_t)0xFE,
			      (uint8_t)0 );
  tscType8 doh_bias[3] = {24,24,24}; 
  tscType8 aoh_bias[3] = {23,23,23}; 
  laserdriverDescription doh_default(2,doh_bias);
  laserdriverDescription aoh_default(2,aoh_bias);
  muxDescription mux_default( (uint16_t)0xFF );
  pllDescription pll_default(6,1);
  
  // Default settings for DCU conversion factors 
  TkDcuConversionFactors dcu_conv_default ( 0, partition_.name_, DCUCCU ) ;
  dcu_conv_default.setAdcGain0(2.144) ;
  dcu_conv_default.setAdcOffset0(0) ;
  dcu_conv_default.setAdcCal0(false) ;
  dcu_conv_default.setAdcInl0(0) ;
  dcu_conv_default.setAdcInl0OW(true) ;
  dcu_conv_default.setI20(0.02122);
  dcu_conv_default.setI10(.01061);
  dcu_conv_default.setICal(false) ;
  dcu_conv_default.setKDiv(0.56) ;
  dcu_conv_default.setKDivCal(false) ;
  dcu_conv_default.setTsGain(8.9) ;
  dcu_conv_default.setTsOffset(2432) ;
  dcu_conv_default.setTsCal(false) ;
  dcu_conv_default.setR68(0) ;
  dcu_conv_default.setR68Cal(false) ;
  dcu_conv_default.setAdcGain2(0) ;
  dcu_conv_default.setAdcOffset2(0) ;
  dcu_conv_default.setAdcCal2(false) ;
  dcu_conv_default.setAdcGain3(0) ;
  dcu_conv_default.setAdcCal3(false) ;
  
  // Create containers for device and PIA reset descriptions and DCU conversions
  keyType index; // unique key (within a FEC crate)
  uint32_t control_key; // unique key (within the tracker)
  
  // Iterate through control system, create descriptions and populate containers 
  for ( vector<SiStripFecCrate>::const_iterator icrate = fec_cabling.crates().begin(); icrate != fec_cabling.crates().end(); icrate++ ) {
    for ( vector<SiStripFec>::const_iterator ifec = icrate->fecs().begin(); ifec != icrate->fecs().end(); ifec++ ) {
      
      // FEC hardware id
      stringstream fec_hardware_id; 
      fec_hardware_id << partition_.name_ << ifec->fecSlot();
      
      for ( vector<SiStripRing>::const_iterator iring = ifec->rings().begin(); iring != ifec->rings().end(); iring++ ) {
      
	// Add dummy CCU with DCU at FEC ring level
	control_key = SiStripControlKey::key( icrate->fecCrate(), 
					      ifec->fecSlot(), 
					      iring->fecRing(), 
					      0x7F,  // CCU address
					      0x10,  // CCU channel
					      0x0 ); // I2C address
	index = buildCompleteKey( ifec->fecSlot(), 
				  iring->fecRing(), 
				  0x7F,  // CCU address
				  0x10,  // CCU channel
				  0x0 ); // I2C address
	dcuDescription* dcu = new dcuDescription( index,       // access key
						  0,           // timestamp
						  control_key, // DCU id
						  0,0,0,0,0,0,0,0 ); // DCU channels
	dcu->setFecHardwareId( fec_hardware_id.str() );
	devices.push_back( dcu );
	TkDcuConversionFactors* dcu_conv = new TkDcuConversionFactors( dcu_conv_default );
	dcu_conv->setDetId( 0 );
	dcu_conv->setDcuHardId( control_key ); // has to be unique within tracker
	dcu_convs.push_back( dcu_conv );
	
	for ( vector<SiStripCcu>::const_iterator iccu = iring->ccus().begin(); iccu != iring->ccus().end(); iccu++ ) {
	  
	  // Add two DOH description at CCU level
	  if ( iccu->ccuAddr() == 1 || iccu->ccuAddr() == 2 ) {
	    laserdriverDescription* doh = new laserdriverDescription( doh_default ) ;
	    index = buildCompleteKey( ifec->fecSlot(), 
				      iring->fecRing(), 
				      iccu->ccuAddr(), 
				      0x10,   // CCU channel
				      0x70 ); // I2C address
	    doh->setAccessKey( index ) ;
	    doh->setFecHardwareId( fec_hardware_id.str() );
	    devices.push_back( doh ) ;
	  }

	  // Add DCU description at CCU level
	  control_key = SiStripControlKey::key( icrate->fecCrate(), 
						ifec->fecSlot(), 
						iring->fecRing(), 
						iccu->ccuAddr(), 
						0x10,  // CCU channel
						0x0 ); // I2C address
	  index = buildCompleteKey( ifec->fecSlot(), 
				    iring->fecRing(), 
				    iccu->ccuAddr(), 
				    0x10,  // CCU channel
				    0x0 ); // I2C address
	  dcuDescription* dcu = new dcuDescription( index,       // access key
						    0,           // timestamp
						    control_key, // DCU id
						    0,0,0,0,0,0,0,0 ); // DCU channels
	  dcu->setFecHardwareId( fec_hardware_id.str() );
	  devices.push_back( dcu );
	  TkDcuConversionFactors* dcu_conv = new TkDcuConversionFactors( dcu_conv_default );
	  dcu_conv->setDetId( 0 );
	  dcu_conv->setDcuHardId( control_key ); // has to be unique within tracker
	  dcu_convs.push_back( dcu_conv );
	
	  // Add PIA reset description at CCU level
	  index = buildCompleteKey( ifec->fecSlot(), 
				    iring->fecRing(), 
				    iccu->ccuAddr(), 
				    0x30,  // CCU channel
				    0x0 ); // I2C address
	  piaResetDescription* pia = new piaResetDescription( index, 10, 10000, 0xFF );
	  pia->setFecHardwareId( fec_hardware_id.str() );
	  pia_resets.push_back( pia );

	  for ( vector<SiStripModule>::const_iterator imod = iccu->modules().begin(); imod != iccu->modules().end(); imod++ ) {

	    index = buildCompleteKey( ifec->fecSlot(), 
				      iring->fecRing(), 
				      iccu->ccuAddr(), 
				      imod->ccuChan(), 
				      0x0 ); // I2C address
	  
	    // Add APV descriptions at module level
	    vector<uint16_t> apvs = imod->activeApvs();
	    vector<uint16_t>::const_iterator iapv = apvs.begin();
	    for ( ; iapv != apvs.end(); iapv++ ) {
	      apvDescription* apv = new apvDescription( apv_default );
	      apv->setAccessKey( index | setAddressKey(*iapv) ) ;
	      apv->setFecHardwareId( fec_hardware_id.str() );
	      devices.push_back( apv );
	    }

	    // Add DCU description at module level
	    dcuDescription* dcu = new dcuDescription( index, // access key
						      0,     // timestamp
						      imod->dcuId(),
						      0,0,0,0,0,0,0,0 ); // DCU channels
	    dcu->setFecHardwareId( fec_hardware_id.str() );
	    devices.push_back( dcu );

	    // Add MUX description at module level
	    muxDescription* mux = new muxDescription( mux_default );
	    mux->setAccessKey( index | 0x43 );
	    mux->setFecHardwareId( fec_hardware_id.str() );
	    devices.push_back( mux );

	    // Add PLL description at module level
	    pllDescription* pll = new pllDescription( pll_default );
	    pll->setAccessKey( index | 0x44 );
	    pll->setFecHardwareId( fec_hardware_id.str() );
	    devices.push_back( pll );

	  }
	}
      }
    }
  }
  
}

// -----------------------------------------------------------------------------
// Misc private methods --------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//
bool SiStripConfigDb::usingDatabase() {
  LogDebug("ConfigDb") << "[SiStripConfigDb::usingDatabase]";
  
  // Check on whether not using database
  if ( !usingDb_ ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingDatabase]"
			      << " Using xml files, not database!";
    return true;
  }
  
  // Retrieve db connection parameters
  if ( user_ == "" || passwd_ == "" || path_ == "" ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingDatabase]"
			      << " Problem with DB connection parameters!"
			      << " Attempting to use env. var. CONFDB...";
    DbAccess::getDbConfiguration( user_, passwd_, path_ );
    if ( user_ == "" || passwd_ == "" || path_ == "" ) {
      edm::LogError("ConfigDb") << "[SiStripConfigDb::usingDatabase]"
				<< " Env. var. CONFDB set incorrectly!";
      return true;
    }
  }

  // Set connection parameters
  try { 
    deviceFactory()->setDatabaseAccess( user_, passwd_, path_ ); 
  } catch (...) { 
    handleFecException( "SiStripConfigDb::usingDatabase" ); 
  }
  
  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::usingDatabase] "
			   << " Using database connection parameters " 
			   << user_ << "/" << passwd_ << "@" << path_;

  return false;
}  

// -----------------------------------------------------------------------------
//
bool SiStripConfigDb::usingXmlFiles() {
  LogDebug("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]";

  bool error = false;
  
  // Check on whether using database
  if ( usingDb_ ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			      << " Using database, not xml files!";
    return true;
  }

  // Input module.xml file
  if ( inputModuleXml_ == "" ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			      << " Null path to input 'module.xml' file!";
    error = true;
  } else {
    if ( checkFileExists( inputModuleXml_ ) ) { 
      try { 
	deviceFactory()->createInputFileAccess(); //@@ necessary?
	//deviceFactory()->addFileName( inputModuleXml_ ); //@@ obsolete?
	deviceFactory()->setFedFecConnectionInputFileName( inputModuleXml_ ); 
      } catch (...) { 
	handleFecException( "SiStripConfigDb::usingXmlFiles" ); 
      }
      edm::LogInfo("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			       << " Added 'module.xml' file: " << inputModuleXml_;
    } else {
      edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " No 'module.xml' file found at " << inputModuleXml_;
      inputModuleXml_ = ""; 
      error = true; 
    }
  }
  
  // Input dcuinfo.xml file
  if ( inputDcuInfoXml_ == "" ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			      << " Null path to input 'dcuinfo.xml' file!";
    error = true;
  } else { 
    if ( checkFileExists( inputDcuInfoXml_ ) ) { 
      try { 
	deviceFactory()->setTkDcuInfoInputFileName( inputDcuInfoXml_ ); 
      } catch (...) { 
	handleFecException( "SiStripConfigDb::usingXmlFiles" ); 
      }
      edm::LogInfo("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			       << " Added 'dcuinfo.xml' file: " << inputDcuInfoXml_;
    } else {
      edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " No 'dcuinfo.xml' file found at " << inputDcuInfoXml_;
      inputDcuInfoXml_ = ""; 
      error = true; 
    } 
  }

  // Input FEC xml files
  if ( inputFecXml_.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			      << " No paths to input 'fec.xml' files!";
    error = true;
  } else {
    vector<string>::iterator iter = inputFecXml_.begin();
    for ( ; iter != inputFecXml_.end(); iter++ ) {
      if ( *iter == "" ) {
	edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				  << " Null path to input 'fec.xml' file!";
	error = true;
      } else {
	if ( checkFileExists( *iter ) ) { 
	  try { 
	    if ( inputFecXml_.size() == 1 ) {
	      deviceFactory()->setFecInputFileName( *iter ); 
	    } else {
	      deviceFactory()->addFecFileName( *iter ); 
	    }
	  } catch (...) { 
	    handleFecException( "SiStripConfigDb::usingXmlFiles" ); 
	  }
	  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				   << " Added 'fec.xml' file: " << *iter;
	} else {
	  edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				    << " No 'fec.xml' file found at " << *iter;
	  *iter = ""; 
	  error = true; 
	} 
      }
    }
  }
    
  // Input FED xml files
  if ( inputFedXml_.empty() ) {
    edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
			      << " No paths to input 'fec.xml' files!";
    error = true;
  } else {
    vector<string>::iterator iter = inputFedXml_.begin();
    for ( ; iter != inputFedXml_.end(); iter++ ) {
      if ( *iter == "" ) {
	edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				  << " Null path to input 'fed.xml' file!";
	error = true;
      } else {
	if ( checkFileExists( *iter ) ) { 
	  try { 
	    if ( inputFecXml_.size() == 1 ) {
	      deviceFactory()->setFedInputFileName( *iter ); 
	    } else {
	      deviceFactory()->addFedFileName( *iter ); 
	    }
	  } catch (...) { 
	    handleFecException( "SiStripConfigDb::usingXmlFiles" ); 
	  }
	  edm::LogInfo("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				   << " Added 'fed.xml' file: " << *iter;
	} else {
	  edm::LogError("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				    << " No 'fed.xml' file found at " << *iter;
	  *iter = ""; 
	  error = true; 
	} 
      }
    }
  }

  // Output module.xml file
  if ( outputModuleXml_ == "" ) { 
    edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " Null path to output 'module.xml' file!"
				<< " Setting to '/tmp/module.xml'...";
    outputModuleXml_ = "/tmp/module.xml"; 
  } else {
    if ( !checkFileExists( outputModuleXml_ ) ) { 
      edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				  << " No file found at " << outputModuleXml_ << "\n"
				  << " Setting to '/tmp/module.xml'...";
      outputModuleXml_ = "/tmp/module.xml"; 
    }
  }

  // Output dcuinfo.xml file
  if ( outputDcuInfoXml_ == "" ) { 
    edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " Null path to output 'dcuinfo.xml' file!"
				<< " Setting to '/tmp/dcuinfo.xml'...";
    outputDcuInfoXml_ = "/tmp/dcuinfo.xml"; 
  } else {
    if ( !checkFileExists( outputDcuInfoXml_ ) ) { 
      edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				  << " No file found at " << outputDcuInfoXml_ << "\n"
				  << " Setting to '/tmp/dcuinfo.xml'...";
      outputDcuInfoXml_ = "/tmp/dcuinfo.xml"; 
    }
  }

  // Output FEC files
  if ( outputFecXml_.empty() ) {
    edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " No path(s) to output 'fec.xml' file(s)!"
				<< " Setting to '/tmp/fec.xml'...";
    outputFecXml_.resize(1,"/tmp/fec.xml");
  } else {
    uint16_t ifec = 0;
    vector<string>::iterator iter = outputFecXml_.begin();
    for ( ; iter != outputFecXml_.end(); iter++ ) {
      if ( *iter == "" ) {
	stringstream ss;
	ss << "[SiStripConfigDb::usingXmlFiles]"
	   << " Null path to output 'fec.xml' file!"
	   << " Setting to '/tmp/fec" << ifec << ".xml'...";
	edm::LogWarning("ConfigDb") << ss.str();
	stringstream sss; sss << "/tmp/fec" << ifec << ".xml"; 
	*iter = sss.str();
      }
      ifec++;
    }
  }

  // Output FED files
  if ( outputFedXml_.empty() ) {
    edm::LogWarning("ConfigDb") << "[SiStripConfigDb::usingXmlFiles]"
				<< " No path(s) to output 'fed.xml' file(s)!"
				<< " Setting to '/tmp/fed.xml'...";
    outputFedXml_.resize(1,"/tmp/fed.xml");
  } else {
    uint16_t ifed = 0;
    vector<string>::iterator iter = outputFedXml_.begin();
    for ( ; iter != outputFedXml_.end(); iter++ ) {
      if ( *iter == "" ) {
	stringstream ss;
	ss << "[SiStripConfigDb::usingXmlFiles]"
	   << " Null path to output 'fed.xml' file!"
	   << " Setting to '/tmp/fed" << ifed << ".xml'...";
	edm::LogWarning("ConfigDb") << ss.str();
	stringstream sss; sss << "/tmp/fed" << ifed << ".xml"; 
	*iter = sss.str();
      }
      ifed++;
    }
  }

  return error;
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::handleFecException( const string& method_name ) {
  try { throw; } // rethrow present exception
  catch ( FecExceptionHandler e ) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" FecExceptionHandler exception thrown with message: " 
			      << e.getMessage();
  }
  catch ( exception& e ) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" std::exception thrown with message: "
			      << e.what();
  }
  catch (...) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" Caught unknown exception!";
  }
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::handleFedException( const string& method_name ) {
  try { throw; } // rethrow present exception
  catch ( ICUtils::ICException& e ) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" ICUtils exception thrown with message: " 
			      << e.what();
  }
  catch ( exception& e ) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" std::exception thrown with message: " 
			      << e.what();
  }
  catch (...) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" Caught unknown exception!";
  }
}

// -----------------------------------------------------------------------------
// 
void SiStripConfigDb::handleSqlException( const string& method_name ) {
  try { throw; } // rethrow present exception
  catch ( oracle::occi::SQLException e ) { 
    edm::LogError("ConfigDb") << "[SiStripConfigDb::closeDbConnection]"
			      << " oracle::occi::SQLException thrown with message: " 
			      << e.getMessage();
  }
  catch ( exception& e ) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" std::exception thrown with message: " << e.what();
  }
  catch (...) {
    edm::LogError("ConfigDb") << "[" << method_name << "]"
			      <<" Caught unknown exception!";
  }
}

// -----------------------------------------------------------------------------
//
bool SiStripConfigDb::checkFileExists( const std::string& path ) {
  fstream fs; 
  fs.open( path.c_str(), ios::in );
  if( !fs.is_open() ) { return false; }
  fs.close();
  return true;
}






//@@ DYNAMIC CASTING

//     FecFactory::deleteVector( descriptions ); // clears vector
//     deviceVector::iterator idevice;
//     for ( idevice = all_descriptions.begin() ; idevice != all_descriptions.end() ; idevice++ ) {
//       if ( device_type == APV ) {
// 	apvDescription* apv = dynamic_cast<apvDescription*>( *idevice );
// 	if ( apv ) { descriptions.push_back( apv ); }
//       } else if ( device_type == PLL ) {
// 	pllDescription* pll = dynamic_cast<pllDescription*>( *idevice );
// 	if ( pll ) { descriptions.push_back( pll ); }
//       } else if ( device_type == MUX ) {
// 	muxDescription* mux = dynamic_cast<muxDescription*>( *idevice );
// 	if ( mux ) { descriptions.push_back( mux ); }
//       } else if ( device_type == DCU ) {
// 	dcuDescription* dcu = dynamic_cast<dcuDescription*>( *idevice );
// 	if ( dcu ) { descriptions.push_back( dcu ); }
//       } else if ( device_type == LLD ) {
// 	laserdriverDescription* lld = dynamic_cast<laserdriverDescription*>( *idevice );
// 	if ( lld ) { descriptions.push_back( lld ); }
//       }	else { 
// 	edm::LogError("ConfigDb") << "[SiStripConfigDb::getDeviceDescriptions]"
// 				  << " Unknown device!!";
//       }
//     }










// // -----------------------------------------------------------------------------
// // 
// void SiStripConfigDb::feDevices( enumDeviceType device_type,
// 				 deviceVector& devices ) {
  
//   string device_name("");
//   if      ( device_type == APV25 )       { device_name = "APV25"; }
//   else if ( device_type == PLL )         { device_name = "PLL"; }
//   else if ( device_type == APVMUX )      { device_name = "APVMUX"; }
//   else if ( device_type == DCU )         { device_name = "DCU"; }
//   else if ( device_type == LASERDRIVER ) { device_name = "LASERDRIVER"; }
//   else { device_name = "UNKNOWN"; }
  
//   pair<int,int> version = partitionVersion( partitionName() );
  
//   try {
//     if ( !allDevices_.empty() ) { FecFactory::deleteVector( allDevices_ ); }
//     deviceFactory()->getFecDeviceDescriptions( partitionName(), 
// 					allDevices_, 
// 					version.first, 
// 					version.second );
//   }
//   catch ( FecExceptionHandler e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::feDevices]"
// 			      <<" Caught FecExceptionHandler exception : " 
// 			      << e.getMessage();
//   }
//   catch ( exception& e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::feDevices]"
// 			      <<" Caught exception : " << e.what();
//   }
//   catch (...) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::feDevices]"
// 			      <<"Caught unknown exception : ";
//   }
  
//   if ( allDevices_.empty() ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::getApvDevices]"
// 			      << " No FE devices found for partition " << partitionName()
// 			      << " and version " << version.first << "." << version.second;
//   } else {
//     LogDebug("ConfigDb") << "[SiStripConfigDb::feDevices]"
// 			 << " Found " << allDevices_.size() << " FE devices "
// 			 << "for partition " << partitionName() 
// 			 << " and version " << version.first << "." << version.second;
//   }
  
//   deviceSummary(); 
  
// }

// // -----------------------------------------------------------------------------
// //
// void SiStripConfigDb::deviceSummary() { 
//   LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary]";
  
//   pair<int,int> version = partitionVersion( partitionName() );
  
//   if ( allDevices_.empty() ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::deviceSummary]"
// 			      << " No devices found for partition " << partitionName()
// 			      << " and version " << version.first << "." << version.second;
//   } else {
//     unsigned int apv_cntr, pll_cntr, mux_cntr, dcu_cntr, las_cntr, doh_cntr, misc_cntr;
//     apv_cntr = pll_cntr = mux_cntr = dcu_cntr = las_cntr = doh_cntr = misc_cntr = 0;
//     deviceVector::iterator idevice;
//     for ( idevice = allDevices_.begin(); idevice != allDevices_.end(); idevice++ ) {
//       if      ( (*idevice)->getDeviceType() == APV25 )       { apv_cntr++; }
//       else if ( (*idevice)->getDeviceType() == PLL )         { pll_cntr++; }
//       else if ( (*idevice)->getDeviceType() == APVMUX )      { mux_cntr++; }
//       else if ( (*idevice)->getDeviceType() == DCU )         { dcu_cntr++; }
//       else if ( (*idevice)->getDeviceType() == LASERDRIVER ) { las_cntr++; }
//       else if ( (*idevice)->getDeviceType() == DOH )         { doh_cntr++; }
//       else { misc_cntr++; }
//     }
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << apv_cntr << " APV devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << pll_cntr << " PLL devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << mux_cntr << " APVMUX devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << dcu_cntr << " DCU devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << las_cntr << " LASERDRIVER devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << doh_cntr << " DOH devices";
//     LogDebug("ConfigDb") << "[SiStripConfigDb::deviceSummary] Found " << misc_cntr << " other MISCELLANEOUS devices";
//     // FecFactory::display( allDevices_ ); 
//   }
  
// }

// // -----------------------------------------------------------------------------
// // 
// void SiStripConfigDb::apvDescriptions( vector<apvDescription*>& apv_descriptions ) {

//   pair<int,int> version = partitionVersion( partitionName() );
  
//   deviceVector apv_devices;
//   try {
//     feDevices( APV25, apv_devices );
//   }
//   catch ( FecExceptionHandler e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::apvDescriptions]"
// 			      <<" Caught FecExceptionHandler exception : " 
// 			      << e.getMessage();
//   }
//   catch ( exception& e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::apvDescriptions]"
// 			      <<" Caught exception : " << e.what();
//   }
//   catch (...) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::apvDescriptions]"
// 			      <<" Caught unknown exception : ";
//   }
  
//   if ( !apv_devices.empty() ) {
//     deviceVector::iterator idevice;
//     for ( idevice = apv_devices.begin() ; idevice != apv_devices.end() ; idevice++ ) {
//       apvDescription* apv = dynamic_cast<apvDescription*>( *idevice );
//       if ( apv ) { 
// 	apv_descriptions.push_back( apv ); 
// 	DeviceAddress addr = hwAddresses( *apv );
// 	LogDebug("ConfigDb") << "[SiStripConfigDb::apvDescriptions] APV25 found at "
// 			     << " FEC crate: " << addr.fecCrate
// 			     << ", FEC slot: " << addr.fecSlot
// 			     << ", FEC ring: " << addr.fecRing
// 			     << ", CCU addr: " << addr.ccuAddr
// 			     << ", CCU chan: " << addr.ccuChan
// 			     << ", I2C Addr: " << addr.i2cAddr;
//       }
//     }
//   }
  
//   if ( apv_descriptions.empty() ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::apvDescriptions]"
// 			      << " No APV descriptions found for partition " 
// 			      << partitionName();
//   } else {
//     LogDebug("ConfigDb") << "[SiStripConfigDb::apvDescriptions] "
// 			 << "Found " << apv_descriptions.size() << " APV descriptions "
// 			 << "for partition " << partitionName() 
// 			 << " and version " << version.first << "." << version.second;
//   }
  
// }

// // -----------------------------------------------------------------------------
// // 
// void SiStripConfigDb::dcuDescriptions( vector<dcuDescription*>& dcu_descriptions ) {
  
//   pair<int,int> version = partitionVersion( partitionName() );
  
//   deviceVector dcu_devices;
//   try {
//     deviceFactory()->getDcuDescriptions( partitionName(), dcuDevices_ );
//   }
//   catch ( FecExceptionHandler e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::dcuDescriptions]"
// 			      <<" Caught FecExceptionHandler exception : " 
// 			      << e.getMessage();
//   }
//   catch ( exception& e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::dcuDescriptions]"
// 			      <<" Caught exception : " << e.what();
//   }
//   catch (...) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::dcuDescriptions]"
// 			      <<" Caught unknown exception : ";
//   }
  
//   if ( !dcu_devices.empty() ) {
//     deviceVector::iterator idevice;
//     for ( idevice = dcu_devices.begin() ; idevice != dcu_devices.end() ; idevice++ ) {
//       dcuDescription* dcu = dynamic_cast<dcuDescription*>( *idevice );
//       if ( dcu ) { 
// 	dcu_descriptions.push_back( dcu ); 
// 	DeviceAddress addr = hwAddresses( *dcu );
// 	LogDebug("ConfigDb") << "[SiStripConfigDb::dcuDescriptions] DCU found at "
// 			     << " FEC crate: " << addr.fecCrate
// 			     << ", FEC slot: " << addr.fecSlot
// 			     << ", FEC ring: " << addr.fecRing
// 			     << ", CCU addr: " << addr.ccuAddr
// 			     << ", CCU chan: " << addr.ccuChan
// 			     << ", I2C Addr: " << addr.i2cAddr;
//       }
//     }
//   }
  
//   if ( dcu_descriptions.empty() ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::dcuDescriptions]"
// 			      << " No DCU descriptions found for partition " 
// 			      << partitionName();
//   } else {
//     LogDebug("ConfigDb") << "[SiStripConfigDb::dcuDescriptions] "
// 			 << "Found " << dcu_descriptions.size() << " DCU descriptions "
// 			 << "for partition " << partitionName() 
// 			 << " and version " << version.first << "." << version.second;
//   }
  
// }


// // -----------------------------------------------------------------------------
// // 
// void SiStripConfigDb::aohDescriptions( vector<laserdriverDescription*>& aoh_descriptions ) {
//   LogDebug("ConfigDb") << "[SiStripConfigDb::aohDescriptions] " 
// 		 << "Retrieving AOH descriptions from database...";

//   try {

//     // Retrieve partition name
//     string partitionName() = partitionName();

//     // Retrieve major/minor versions for given partition
//     pair<int,int> version;
//     partitionVersion( partitionName(), version );

//     // Retrieve all AOH devices 
//     deviceVector aoh_devices;
//     feDevices( LASERDRIVER, aoh_devices );

//     // Retrieve LASERDRIVER descriptions
//     if ( !aoh_devices.empty() ) {
//       deviceVector::iterator idevice;
//       for ( idevice = aoh_devices.begin() ; idevice != aoh_devices.end() ; idevice++ ) {
// 	laserdriverDescription* aoh = dynamic_cast<laserdriverDescription*>( *idevice );
// 	if ( aoh ) { aoh_descriptions.push_back( aoh ); }
// 	LogDebug("ConfigDb") << "[SiStripConfigDb::aohDescriptions] "
// 		      << "LASERDRIVER (AOH) found at: "
// 		      << "FEC slot: " << getFecKey(     aoh->getKey() ) << ", "
// 		      << "FEC ring: " << getChannelKey( aoh->getKey() ) << ", "
// 		      << "CCU addr: " << getCcuKey(     aoh->getKey() ) << ", "
// 		      << "CCU chan: " << getChannelKey( aoh->getKey() ) << ", "
// 		      << "I2C Addr: " << getAddressKey( aoh->getKey() );
//       }
//     }

//     LogDebug("ConfigDb") << "[SiStripConfigDb::aohDescriptions] "
// 		  << "Found " << aoh_descriptions.size() << " AOH descriptions "
// 		  << "for the partition name " << partitionName() 
// 		  << " and version " << version.first << "." << version.second
// 		 ;

//     // Check if any AOH descriptions are found
//     if ( aoh_descriptions.empty() ) {
//       edm::LogError("ConfigDb") << warning("[SiStripConfigDb::aohDescriptions] ")
// 	   << "No LASERDRIVER (AOH) descriptions found for the partition name " << partitionName()
// 	   << endl ;
//     }

//   }
//   catch ( FecExceptionHandler e ) {
//     edm::LogError("ConfigDb") << warning("[SiStripConfigDb::aohDescriptions] ")
// 	 <<"Caught FecExceptionHandler exception : Problems accessing database " 
// 	 << e.getMessage();
//   }
//   catch ( exception& e ) {
//     edm::LogError("ConfigDb") << warning("[SiStripConfigDb::aohDescriptions] ")
// 	 <<"Caught exception : Problems accessing database " 
// 	 << e.what();
//   }
//   catch (...) {
//     edm::LogError("ConfigDb") << warning("[SiStripConfigDb::aohDescriptions] ")
// 	 <<"Caught unknown exception : Problems accessing database " 
// 	;
//   }
  
// }



// // -----------------------------------------------------------------------------
// // Open connection to database
// void SiStripConfigDb::feDevices() {
//   LogDebug("ConfigDb") << "[SiStripConfigDb::feDevices] "
// 		<< " ";
  
// #ifdef USING_DATABASE

//   pair<int,int> version = partitionVersion();
//   SimpleConfigurable<string> partitionName()("nil","SiStripConfigDb:PartitionName");
//   try {
//     deviceVector devices = deviceFactory()->getFecDeviceDescriptions( partitionName(), version.first, version.second );
//     if ( devices.empty() ) {
//       edm::LogError("ConfigDb") << "[SiStripConfigDb::feDevices] "
// 	   << "ERROR : No FE devices exist for the partition name " << partitionName()
// 	   << endl ;
//     }
//     if ( devices.size() ) {
//       LogDebug("ConfigDb") << "[SiStripConfigDb::feDevices] "
// 	   << "Found " << devices.size() << " FE device descriptions "
// 	   << "for partition name " << partitionName();
//       FecFactory::display( devices ); // display devices
//       // count number of APV devices
//       unsigned int apv_cntr = 0;
//       deviceVector::iterator device;
//       for ( device = devices.begin() ; device != devices.end() ; device++ ) {
// 	deviceDescription* description = *device;
// 	if ( description->getDeviceType() == APV25 ) { 
// 	  apv_cntr++; 
// 	  description->display();
// 	}
//       }
//       LogDebug("ConfigDb") << "Found " << apv_cntr << " APV ";
//     }
//   }
//   catch ( FecExceptionHandler e ) {
//     edm::LogError("ConfigDb") << "[SiStripConfigDb::feDevices] "
// 	 <<"ERROR : problems retrieving FE devices " 
// 	 << e.getMessage();
//   }

// #else
//   edm::LogError("ConfigDb") << warning("[SiStripConfigDb::openConnection] ")
//        << "USING_DATABASE not defined! => not using database";        
// #endif // USING_DATABASE

// }


//       // ----------------------------------------------------------------------------------------------------
//       // Upload in database => create a version major with a modification on the PLL
//       // set all the devices with modification in the PLL
//       unsigned int major, minor ;
//       deviceFactory->setFecDeviceDescriptions (devices, partitionName, &major, &minor, true) ;
//       //                                                                                  ^ major version, false = minor version
    
//       LogDebug("ConfigDb") << "-------------------------- Version ---------------------" << endl ;
//       LogDebug("ConfigDb") << "Upload the version " << dec << major << "." << minor << " in the database" << endl ;
//       LogDebug("ConfigDb") << "--------------------------------------------------------" << endl ;
    
//       // ----------------------------------------------------------------------------------------------------
//       // set the version as to be downloaded
//       deviceFactory->setFecDevicePartitionVersion ( partitionName, major, minor ) ;
    
//       // ----------------------------------------------------------------------------------------------------
//       // Upload in database => create a version major with a modification on the PLL
//       // set only the PLL devices
//       deviceFactory->setFecDeviceDescriptions (devices, partitionName, &major, &minor, false) ;
//       //                                                                                  ^ minor version, true = major version
    
//       LogDebug("ConfigDb") << "-------------------------- Version ---------------------" << endl ;
//       LogDebug("ConfigDb") << "Upload the version " << dec << major << "." << minor << " in the database" << endl ;
//       LogDebug("ConfigDb") << "--------------------------------------------------------" << endl ;
   
//       // ---------------------------------------------------------------------------------------------------- 
//       // set the version as to be downloaded
//       deviceFactory->setFecDevicePartitionVersion ( partitionName, major, minor ) ;

//       // ---------------------------------------------------------------------------------------------------- 
//       // delete the vector of deviceDescriptions
//       FecFactory::deleteVector (devices) ;
//     }
//     else {
//       edm::LogError("ConfigDb") << "No devices found in the database" << endl ;
//     }

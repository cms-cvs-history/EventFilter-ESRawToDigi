#include "EventFilter/ESRawToDigi/interface/ESUnpackerTB.h"
#include "EventFilter/ESRawToDigi/interface/ESCrcKchipFast.h"
#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/EcalDetId/interface/EcalDetIdCollections.h"
#include "DataFormats/FEDRawData/interface/FEDHeader.h"
#include "DataFormats/FEDRawData/interface/FEDTrailer.h"

ESUnpackerTB::ESUnpackerTB(const ParameterSet& ps) 
  : pset_(ps), fedId_(0), run_number_(0), orbit_number_(0), bx_(0), lv1_(0), trgType_(0)
{

  debug_ = pset_.getUntrackedParameter<bool>("debugMode", false);

}

ESUnpackerTB::~ESUnpackerTB() {
}

void ESUnpackerTB::interpretRawData(int fedId, const FEDRawData & rawData, ESRawDataCollection & dccs, ESLocalRawDataCollection & kchips, ESDigiCollection & digis) {
  
  int nWords = rawData.size()/sizeof(Word64);
  if (nWords==0) return;
  int dccWords = 6;
  
  // Event header
  const Word64* header = reinterpret_cast<const Word64* >(rawData.data()); --header;
  bool moreHeaders = true;
  while (moreHeaders) {
    ++header;
    FEDHeader ESHeader( reinterpret_cast<const unsigned char*>(header) );
    if ( !ESHeader.check() ) break; // throw exception?
    if ( ESHeader.sourceID() != fedId) throw cms::Exception("PROBLEM in ESUnpackerTB !");

    fedId_ = ESHeader.sourceID();
    lv1_   = ESHeader.lvl1ID();
    bx_    = ESHeader.bxID();

    if (debug_) {
      cout<<"[ESUnpackerTB]: FED Header candidate. Is header? "<< ESHeader.check();
      if (ESHeader.check())
        cout <<". BXID: "<<bx_<<" SourceID : "<<fedId_<<" L1ID: "<<lv1_<<endl;
      else cout<<" WARNING!, this is not a ES Header"<<endl;
    }

    moreHeaders = ESHeader.moreHeaders();
  }

  ESDCCHeaderBlock ESDCCHeader;
  ESDCCHeader.setFedId(fedId_);
  ESDCCHeader.setLV1(lv1_);
  ESDCCHeader.setBX(bx_);

  // Event trailer
  const Word64* trailer = reinterpret_cast<const Word64* >(rawData.data())+(nWords-1); ++trailer;
  bool moreTrailers = true;
  while (moreTrailers) {
    --trailer;
    FEDTrailer ESTrailer(reinterpret_cast<const unsigned char*>(trailer));
    if ( !ESTrailer.check()) { ++trailer; break; } // throw exception?
    if ( ESTrailer.lenght()!= nWords) throw cms::Exception("PROBLEM in ESUnpackerTB !!");
    evtLen_ = ESTrailer.lenght();

    if (debug_)  {
      cout<<"[ESUnpackerTB]: FED Trailer candidate. Is trailer? "<<ESTrailer.check();
      if (ESTrailer.check())
        cout<<". Length of the ES event: "<<ESTrailer.lenght()<<endl;
      else cout<<" WARNING!, this is not a ES Trailer"<<endl;
    }

    moreTrailers = ESTrailer.moreTrailers();
  }

  // DCC data
  DCCHeader.clear();
  for (const Word64* word=(header+1); word!=(header+dccWords+1); ++word) {
    if (debug_) cout<<"DCC   : "<<print(*word)<<endl;
    DCCHeader.push_back(*word);
  }
  word2DCCHeader(DCCHeader);

  ESDCCHeader.setEventLength(evtLen_);
  ESDCCHeader.setDCCErrors(DCCErr_);
  ESDCCHeader.setRunNumber(runNum_);
  ESDCCHeader.setRunType(runType_);
  ESDCCHeader.setTriggerType(trgType_);
  ESDCCHeader.setCompressionFlag(compFlag_);
  ESDCCHeader.setOrbitNumber(orbit_);
  ESDCCHeader.setMajorVersion(vmajor_);
  ESDCCHeader.setMinorVersion(vminor_);
  ESDCCHeader.setOptoRX0(optoRX0_);
  ESDCCHeader.setOptoRX1(optoRX1_);
  ESDCCHeader.setOptoRX2(optoRX2_);
  vector<int> enabledFiber;
  vector<int> FEch_status;
  for (unsigned int i=0; i<36; ++i) {
    enabledFiber.push_back(i);
    FEch_status.push_back(FEch_[i]);
  }
  ESDCCHeader.setFEChannelStatus(FEch_status);
  
  dccs.push_back(ESDCCHeader);  

  if (evtLen_ != 1204) return;

  // Event data
  map<int, vector<Word8> > map_data;
  Word8 word8;
  int count = 0;
  int kchip;
  for (const Word64* word=(header+dccWords+1); word!=trailer; ++word) {

    if (debug_) cout<<"Event : "<<print(*word)<<endl;

    if ((count%2) == 0) {
      kchip = 0;
      word8 = ((*word) >> 0) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 1;
      word8 = ((*word) >> 8) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 2;
      word8 = ((*word) >> 16) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 3;
      word8 = ((*word) >> 24) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 4;
      word8 = ((*word) >> 32) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 5;
      word8 = ((*word) >> 40) & 0xff;
      map_data[kchip].push_back(word8);
    } else if ((count%2) == 1) {
      kchip = 6;
      word8 = ((*word) >> 0) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 7;
      word8 = ((*word) >> 8) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 8;
      word8 = ((*word) >> 16) & 0xff;
      map_data[kchip].push_back(word8);
      kchip = 9;
      word8 = ((*word) >> 24) & 0xff;
      map_data[kchip].push_back(word8);
    }

    count++;
  }

  // merge into Word16 for the time being
  vector<Word8> words8;
  Word16 word16;
  vector<Word16> words;
  for (int i=0; i<10; i++) {
    words.clear();
    for (int j=0; j<598; j+=2) {
      words8 = map_data[i];
      word16 = (Word16(words8[j+1]) << 8) | words8[j];
      words.push_back(word16);
    }
    word2digi(i+1, words, kchips, digis);
  }

}

void ESUnpackerTB::word2DCCHeader(const vector<Word64> & word) {

  //evtLen_   = (word[0])       & 0xffffff;
  DCCErr_   = (word[0] >> 24) & 0x00ff;
  runNum_   = (word[0] >> 32) & 0xffffff;

  runType_  = (word[1])       & 0xffffffff;
  trgType_  = (word[1] >> 32) & 0xffff;
  compFlag_ = (word[1] >> 48) & 0x00ff;

  orbit_    = (word[2])       & 0xffffffff;
  vminor_   = (word[2] >> 40) & 0x00ff;
  vmajor_   = (word[2] >> 48) & 0x00ff;

  optoRX0_  = (word[3] >> 48) & 0x00ff;
  optoRX1_  = (word[4] >> 48) & 0x00ff;
  optoRX2_  = (word[5] >> 48) & 0x00ff;

  for (unsigned int i=0; i<3; ++i) { 
    for (unsigned int j=0; j<12; ++j) {
      FEch_[i*12+j] = (word[i+3] >> (j*4)) & 0x000f;
    }
  }

}

void ESUnpackerTB::word2digi(int fiber, const vector<Word16> & word, ESLocalRawDataCollection & kchips, ESDigiCollection & digis) 
{                

  //for (int i=0; i<word.size(); ++i) cout<<"Fiber : "<<fiber<<" "<<print(word[i])<<endl;

  if (word.size() != 299) {
    cout<<"KChip data length is not 299 for fiber : "<<fiber<<endl;
    return;
  }

  int kBC = word[1] & 0x0fff; 
  int kEC = word[2] & 0x00ff;
  int kID = (word[3] >> 8) & 0x00ff; 
  int kFlag1 = (word[1] >> 12) & 0x000f;
  int kFlag2 = (word[2] >>  8) & 0x00ff; 
  int chksum = word[298] & 0xffff;
  if (debug_) cout<<"Fiber : "<<fiber<<" BC : "<<kBC<<" EC : "<<kEC<<" KID : "<<kID<<" F1 : "<<kFlag1<<" F2 : "<<kFlag2<<" Chksum : "<<chksum<<endl;

  ESCrcKchipFast crcChecker;

  uint32_t packet_length = (kFlag1 & 0x07) ? 5 : 299 ; 

  for(uint32_t kk=1; kk < packet_length; ++kk) crcChecker.add((unsigned int) word[kk]); 

  ESKCHIPBlock ESKCHIP;
  ESKCHIP.setId(kID);
  ESKCHIP.setFiberId(fiber);
  ESKCHIP.setBC(kBC);
  ESKCHIP.setEC(kEC);
  ESKCHIP.setFlag1(kFlag1);
  ESKCHIP.setFlag2(kFlag2);

  if (crcChecker.isCrcOk()) { 
     ESKCHIP.setCRC(1);
     kchips.push_back(ESKCHIP);
  } else { 
     ESKCHIP.setCRC(0);
     kchips.push_back(ESKCHIP);
     return ; 
  }

  int col[4],ix[4],iy[4],adc[4][3],plane;
  for (int i=0; i<3; ++i) {

    col[0] = (word[i*98+4] >> 8) & 0x00ff;
    col[1] = (word[i*98+4])      & 0x00ff;
    col[2] = (word[i*98+5] >> 8) & 0x00ff;
    col[3] = (word[i*98+5])      & 0x00ff;
    if (debug_) cout<<"Column : "<<col[0]<<" "<<col[1]<<" "<<col[2]<<" "<<col[3]<<endl;
  }

  if (kID<=6) plane = 2;
  else if (kID>6) plane = 1;
  
  for (int j=0; j<32; ++j) {    

    for (int i=0; i<3; ++i) {

      adc[0][i] = (word[i*98+6+j*3] >> 4) & 0x0fff;
      if (kID==2) { ix[0] = 2; iy[0] = 2; }
      else if (kID==8)  { ix[0] = 3; iy[0] = 1; }
      else if (kID==5)  { ix[0] = 2; iy[0] = 4; }
      else if (kID==7)  { ix[0] = 3; iy[0] = 3; }
      else if (kID==1)  { ix[0] = 4; iy[0] = 2; }
      else if (kID==11) { ix[0] = 1; iy[0] = 1; }
      else if (kID==3)  { ix[0] = 1; iy[0] = 2; }
      else if (kID==6)  { ix[0] = 1; iy[0] = 4; }
      else if (kID==4)  { ix[0] = 4; iy[0] = 4; }
      else if (kID==10) { ix[0] = 1; iy[0] = 3; }

      adc[1][i] = ((word[i*98+6+j*3] & 0x000f) << 8) ;
      adc[1][i] |= ((word[i*98+7+j*3] >> 8) & 0x00ff);  
      if (kID==3) { ix[1] = 1; iy[1] = 1; }
      else if (kID==2 || kID==8) { ix[1] = 3; iy[1] = 2; }
      else if (kID==6) { ix[1] = 1; iy[1] = 3; }
      else if (kID==5 || kID==7) { ix[1] = 3; iy[1] = 4; }
      else if (kID==11) { ix[1] = 1; iy[1] = 2; }
      else if (kID==1 || kID==4 || kID==10) { ix[1] = 0; iy[1] = 0; }

      adc[2][i] = ((word[i*98+7+j*3] & 0x00ff) << 4);
      adc[2][i] |= ((word[i*98+8+j*3] >> 12) & 0x000f);  
      if (kID==3 || kID==6) { ix[2] = 0; iy[2] = 0; }
      else if (kID==2 || kID==11) { ix[2] = 2; iy[2] = 1; }
      else if (kID==5 || kID==10) { ix[2] = 2; iy[2] = 3; }
      else if (kID==1 || kID==8)  { ix[2] = 4; iy[2] = 1; }
      else if (kID==4 || kID==7)  { ix[2] = 4; iy[2] = 3; }

      adc[3][i] = (word[i*98+8+j*3])      & 0x0fff;
      if (kID==1 || kID==3 || kID==4 || kID==6 || kID==7) { ix[3] = 0; iy[3] = 0; }
      else if (kID==2)  { ix[3] = 3; iy[3] = 1; }
      else if (kID==11) { ix[3] = 2; iy[3] = 2; }
      else if (kID==8)  { ix[3] = 4; iy[3] = 2; }
      else if (kID==5)  { ix[3] = 3; iy[3] = 3; }
      else if (kID==10) { ix[3] = 2; iy[3] = 4; }

    }

    for (int k=0; k<4; ++k) {

      if (ix[k]==0 || iy[k]==0) continue;
      
      ESDetId detId(j+1, ix[k], iy[k], plane, 1);
      ESDataFrame df(detId);
      df.setSize(3);
      
      for (int m=0; m<3; m++) df.setSample(m, adc[k][m]);
      
      digis.push_back(df);
      
      if (debug_) cout<<"Si : "<<detId.zside()<<" "<<detId.plane()<<" "<<detId.six()<<" "<<detId.siy()<<" "<<detId.strip()<<" ("<<kID<<","<<k<<") "<<df.sample(0).adc()<<" "<<df.sample(1).adc()<<" "<<df.sample(2).adc()<<endl;
      
    }

  }
  
}

string ESUnpackerTB::print(const  Word64 & word) const
{
  ostringstream str;
  str << "Word64:  " << reinterpret_cast<const bitset<64>&> (word);
  return str.str();
}

string ESUnpackerTB::print(const  Word16 & word) const
{
  ostringstream str;
  str << "Word16:  " << reinterpret_cast<const bitset<16>&> (word);
  return str.str();
}

string ESUnpackerTB::print(const  Word8 & word) const
{
  ostringstream str;
  str << "Word8:  " << reinterpret_cast<const bitset<8>&> (word);
  return str.str();
}


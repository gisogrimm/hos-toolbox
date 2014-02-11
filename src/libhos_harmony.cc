#include "libhos_harmony.h"
#include <stdlib.h>
#include "defs.h"

ambitus_t::ambitus_t()
{
  // treble: d=-10 g=-5 c'=0 e'=3 a'=9 d''=14 (bb''=22)
  for(int32_t p=-10; p<22;p++)
    treble.set(p,gauss(p-12,8));
  treble.update();
  // alto: G=-17 g'=7 (ees''=15)
  for(int32_t p=-17; p<15;p++)
    tenor.set(p,gauss(p-2,8));
  tenor.update();
  // bass: D=-22 d'=2 (bb'=10)
  for(int32_t p=-22; p<10;p++)
    bass.set(p,gauss(p+12,8));
  bass.update();
  //  fezzo: GG=-29 g=-5 (ees'=3)
  for(int32_t p=-29; p<3;p++)
    fezzo.set(p,gauss(p+17,8));
  fezzo.update();
}

scale_t::scale_t()
{
  for(int oct=-10;oct<=10;oct++){
    major.set(0+12*oct,1);
    major.set(2+12*oct,1);
    major.set(4+12*oct,1); // major third
    major.set(5+12*oct,1);
    major.set(7+12*oct,1); // fifth
    major.set(9+12*oct,1);
    major.set(11+12*oct,1); // major seven
    minor.set(0+12*oct,1);
    minor.set(2+12*oct,1);
    minor.set(3+12*oct,1); // minor third
    minor.set(5+12*oct,1);
    minor.set(7+12*oct,1); // fifth
    minor.set(9+12*oct,1);
    minor.set(10+12*oct,1); // minor seven
    //minor.set(11+12*oct,1); // major seven
  }
  major.update();
  minor.update();
}

triad_t::triad_t()
{
  for(int oct=-10;oct<=10;oct++){
    minor.set(0+12*oct,1);
    minor.set(3+12*oct,1); // minor third
    minor.set(7+12*oct,1); // fifth
    major.set(0+12*oct,1);
    major.set(4+12*oct,1); // major third
    major.set(7+12*oct,1); // fifth
  }
  major.update();
  minor.update();
}

const pmf_t& scale_t::operator[](keysig_t::mode_t m) const
{
  switch( m ){
  case keysig_t::major:
    return major;
  case keysig_t::minor:
    return minor;
  }
  return major;
}

const pmf_t& triad_t::operator[](keysig_t::mode_t m) const
{
  switch( m ){
  case keysig_t::major:
    return major;
  case keysig_t::minor:
    return minor;
  }
  return major;
}

void harmony_model_t::update_tables()
{
  pkeyrel.clear();
  for( pmf_t::const_iterator kit=pkey.begin();kit!=pkey.end();++kit){
    pmf_t ptab;
    for( pmf_t::const_iterator kcit=pchange.begin();kcit!=pchange.end();++kcit){
      keysig_t key(kit->first);
      key+= keysigchange_t(kcit->first);
      double p(kit->second * kcit->second);
      if( p > 0 )
        ptab.set(key.hash(),p);
      ptab.update();
      pkeyrel[kit->first] = ptab;
    }
  }
}

bool harmony_model_t::process(double beat)
{
  if( pbeat.rand() == beat ){
    keysig_t old_key(key_current);
    key_current = key_next;
    key_next = keysig_t(pkeyrel[old_key.hash()].rand());
    return !(key_current == key_next);
  }
  return false;
}

const keysig_t& harmony_model_t::current() const
{
  return key_current;
}

const keysig_t& harmony_model_t::next() const
{
  return key_next;
}

void harmony_model_t::read_xml(xmlpp::Element* e)
{
  pkey.clear();
  pchange.clear();
  pbeat.clear();
  xmlpp::NodeSet nHarmony(e->find("//harmony"));
  if( !nHarmony.empty() ){
    xmlpp::Element* eHarmony(dynamic_cast<xmlpp::Element*>(*(nHarmony.begin())));
    if( eHarmony ){
      // first scan absolute keys:
      xmlpp::Node::NodeList nKey(eHarmony->get_children("key"));
      for(xmlpp::Node::NodeList::iterator nKeyIt=nKey.begin(); nKeyIt != nKey.end(); ++nKeyIt){
        xmlpp::Element* eKey(dynamic_cast<xmlpp::Element*>(*nKeyIt));
        if( eKey ){
          std::string s_val(eKey->get_attribute_value("v"));
          std::string s_prob(eKey->get_attribute_value("p"));
          keysig_t key(s_val);
          pkey.set(key.hash(),atof(s_prob.c_str()));
        }
      }
      // second relative key changes:
      xmlpp::Node::NodeList nChange(eHarmony->get_children("change"));
      for(xmlpp::Node::NodeList::iterator nChangeIt=nChange.begin(); nChangeIt != nChange.end(); ++nChangeIt){
        xmlpp::Element* eKey(dynamic_cast<xmlpp::Element*>(*nChangeIt));
        if( eKey ){
          std::string s_val(eKey->get_attribute_value("v"));
          std::string s_par;
          size_t cp(0);
          if( (cp=s_val.find(":")) != std::string::npos ){
            s_par = s_val.substr(cp+1);
            s_val.erase(cp);
          }
          std::string s_prob(eKey->get_attribute_value("p"));
          keysigchange_t ksc(atoi(s_val.c_str()),s_par == "p" );
          pchange.set(ksc.hash(),atof(s_prob.c_str()));
        }
      }
      // last key change beats:
      xmlpp::Node::NodeList nBeat(eHarmony->get_children("beat"));
      for(xmlpp::Node::NodeList::iterator nBeatIt=nBeat.begin(); nBeatIt != nBeat.end(); ++nBeatIt){
        xmlpp::Element* eKey(dynamic_cast<xmlpp::Element*>(*nBeatIt));
        if( eKey ){
          std::string s_val(eKey->get_attribute_value("v"));
          std::string s_prob(eKey->get_attribute_value("p"));
          pbeat.set(atof(s_val.c_str()),atof(s_prob.c_str()));
        }
      }
    }
  }
  pkey.update();
  pchange.update();
  pbeat.update();
  update_tables();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

#include "libhos_harmony.h"
#include "hos_defs.h"
#include <math.h>
#include <stdlib.h>
#include <tascar/errorhandling.h>

double get_attribute_double(xmlpp::Element* e, const std::string& name)
{
  std::string val(e->get_attribute_value(name.c_str()));
  if(val.size() == 0)
    throw TASCAR::ErrMsg("Empty attribute \"" + name + "\"");
  return atof(val.c_str());
}

double get_attribute_double(xmlpp::Element* e, const std::string& name,
                            double def)
{
  std::string val(e->get_attribute_value(name.c_str()));
  if(val.size() == 0)
    return def;
  return atof(val.c_str());
}

scale_t::scale_t()
{
  for(int oct = -10; oct <= 10; oct++) {
    major.set(0 + 12 * oct, 1);
    major.set(2 + 12 * oct, 1);
    major.set(4 + 12 * oct, 1); // major third
    major.set(5 + 12 * oct, 1);
    major.set(7 + 12 * oct, 1); // fifth
    major.set(9 + 12 * oct, 1);
    major.set(11 + 12 * oct, 1); // major seven
    minor.set(0 + 12 * oct, 1);
    minor.set(2 + 12 * oct, 1);
    minor.set(3 + 12 * oct, 1); // minor third
    minor.set(5 + 12 * oct, 1);
    minor.set(7 + 12 * oct, 1); // fifth
    minor.set(9 + 12 * oct, 1);
    minor.set(10 + 12 * oct, 1); // minor seven
    // minor.set(11+12*oct,1); // major seven
  }
  major.update();
  minor.update();
}

triad_t::triad_t()
{
  for(int oct = -10; oct <= 10; oct++) {
    minor.set(0 + 12 * oct, 1);
    minor.set(3 + 12 * oct, 1); // minor third
    minor.set(7 + 12 * oct, 1); // fifth
    major.set(0 + 12 * oct, 1);
    major.set(4 + 12 * oct, 1); // major third
    major.set(7 + 12 * oct, 1); // fifth
  }
  major.update();
  minor.update();
}

const pmf_t& scale_t::operator[](keysig_t::mode_t m) const
{
  switch(m) {
  case keysig_t::major:
    return major;
  case keysig_t::minor:
    return minor;
  }
  return major;
}

const pmf_t& triad_t::operator[](keysig_t::mode_t m) const
{
  switch(m) {
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
  for(pmf_t::const_iterator kit = pkey.begin(); kit != pkey.end(); ++kit) {
    pmf_t ptab;
    keysig_t basekey(kit->first);
    for(pmf_t::const_iterator kcit = pchange.begin(); kcit != pchange.end();
        ++kcit) {
      keysig_t key(basekey);
      key += keysigchange_t(kcit->first);
      if(pkey.find(key.hash()) != pkey.end()) {
        double p(kcit->second * pkey[key.hash()]);
        if(p > 0)
          ptab.set(key.hash(), p);
      }
    }
    ptab.update();
    if(ptab.icdfempty()) {
      DEBUG(basekey);
      throw TASCAR::ErrMsg("No relative key table for key " + basekey.name() +
                           ".");
    }
    pkeyrel[kit->first] = ptab;
  }
  // DEBUG(pkeyrel.size());
  for(std::map<uint32_t, pmf_t>::iterator it = pkeyrel.begin();
      it != pkeyrel.end(); ++it) {
    std::cout << "-- " << keysig_t(it->first) << " ------\n";
    for(pmf_t::iterator p = it->second.begin(); p != it->second.end(); ++p)
      std::cout << "  " << keysig_t(p->first);
    std::cout << std::endl;
  }
  key_current = keysig_t(pkey.vpmax());
  key_next = key_current;
  // throw TASCAR::ErrMsg("Stop");
}

/**
   \brief Update harmony model based on current beat
   \return True if the key changed.
 */
bool harmony_model_t::process(double beat)
{
  beat = rint(BEATRES * beat) / BEATRES;
  if(pbeat.find(beat) != pbeat.end()) {
    double rand(drand());
    if(pbeat[beat] > rand) {
      keysig_t old_key(key_current);
      key_current = key_next;
      try {
        key_next = keysig_t(pkeyrel[key_current.hash()].rand());
      }
      catch(const std::exception& e) {
        DEBUG(e.what());
      }
      return !(key_current == old_key);
    }
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
  if(!nHarmony.empty()) {
    xmlpp::Element* eHarmony(
        dynamic_cast<xmlpp::Element*>(*(nHarmony.begin())));
    if(eHarmony) {
      // first scan absolute keys:
      xmlpp::Node::NodeList nKey(eHarmony->get_children("key"));
      for(xmlpp::Node::NodeList::iterator nKeyIt = nKey.begin();
          nKeyIt != nKey.end(); ++nKeyIt) {
        xmlpp::Element* eKey(dynamic_cast<xmlpp::Element*>(*nKeyIt));
        if(eKey) {
          std::string s_val(eKey->get_attribute_value("v"));
          keysig_t key(s_val);
          pkey.set(key.hash(), get_attribute_double(eKey, "p"));
        }
      }
      // second relative key changes:
      xmlpp::Node::NodeList nChange(eHarmony->get_children("change"));
      for(xmlpp::Node::NodeList::iterator nChangeIt = nChange.begin();
          nChangeIt != nChange.end(); ++nChangeIt) {
        xmlpp::Element* eChange(dynamic_cast<xmlpp::Element*>(*nChangeIt));
        if(eChange) {
          std::string s_val(eChange->get_attribute_value("v"));
          std::string s_par;
          size_t cp(0);
          if((cp = s_val.find(":")) != std::string::npos) {
            s_par = s_val.substr(cp + 1);
            s_val.erase(cp);
          }
          keysigchange_t ksc(atoi(s_val.c_str()), s_par == "p");
          pchange.set(ksc.hash(), get_attribute_double(eChange, "p"));
        }
      }
      // last key change beats:
      xmlpp::Node::NodeList nBeat(eHarmony->get_children("beat"));
      for(xmlpp::Node::NodeList::iterator nBeatIt = nBeat.begin();
          nBeatIt != nBeat.end(); ++nBeatIt) {
        xmlpp::Element* eBeat(dynamic_cast<xmlpp::Element*>(*nBeatIt));
        if(eBeat) {
          std::string s_val(eBeat->get_attribute_value("v"));
          std::string s_prob(eBeat->get_attribute_value("p"));
          pbeat.set(atof(s_val.c_str()), get_attribute_double(eBeat, "p"));
        }
      }
    }
  }
  pkey.update();
  pchange.update();
  // pbeat.update();
  update_tables();
}

pmf_t harmony_model_t::notes(double triadw) const
{
  triad_t triad;
  scale_t scale;
  // DEBUG(triadw);
  // DEBUG(triad[key_current.mode]);
  pmf_t n(triad[key_current.mode] * triadw +
          scale[key_current.mode] * (1.0 - triadw));
  // DEBUG(n.vadd(key_current.pitch()));
  return n.vadd(key_current.pitch());
}

/**
   \brief Generate a new note.

   Use an underlying harmony model and time signature model to
   calculate possible note probabilities.

   \param beat Current beat within a bar

   \param harmony Underlying harmony model

   \param timesig Underlying time signature model

   \param center Most desired pitch

   \param bandw Typical desired pitch range

   \param harmonyweight Weight between harmony model based composition
   (1) and atonal composition (0)

   \param beatweight Weight between bar hierarchic composition (1) and
   free composition (0)

   \param modf Note length related parameter linked to modulation frequency

   \return New note
 */
note_t melody_model_t::process(double beat, const harmony_model_t& harmony,
                               const time_signature_t& timesig, double center,
                               double bandw, double harmonyweight,
                               double beatweight, double modf)
{
  beat = rint(BEATRES * beat) / BEATRES;
  bool onbeat(fabs(frac(beat)) < EPS);
  double triadw(1.0);
  if(onbeat)
    triadw = 1.0 - onbeatscale;
  else
    triadw = 1.0 - offbeatscale;
  pmf_t notes(harmony.notes(triadw));
  // pmf_t notes(harmony.notes(1.0));
  // medoly model step processing:
  notes *= pstep.vadd(last_pitch);
  // DEBUG(notes);
  // release of rules:
  if(harmonyweight != 1.0) {
    pmf_t equal;
    for(int32_t n = notes.vmin(); n <= notes.vmax(); n++)
      equal.set(n, 1);
    equal.update();
    notes = notes * harmonyweight + equal * (1.0 - harmonyweight);
  }
  // limit to instrument ambitus:
  notes *= pambitus;
  // tilt/center by input range:
  notes *= gauss(center, 0.25 * bandw, pambitus.vmin(), pambitus.vmax(), 1.0);
  notes.update();
  int32_t pitch(PITCH_REST);
  if(!notes.icdfempty()) {
    pitch = notes.rand();
  }
  pmf_t dur(pduration);
  pmf_t valid_times(pbeat);
  // valid_times.update();
  pmf_t all_valid_times;
  double pmax(valid_times.pmax());
  for(uint32_t k = 0; k < 8 * timesig.numerator; k++)
    all_valid_times.set(0.125 * k, pmax);
  all_valid_times.update();
  valid_times =
      valid_times * beatweight + all_valid_times * ((1.0 - beatweight) * 0.125);
  dur *=
      ((valid_times.vadd(-beat) + valid_times.vadd(-beat + timesig.numerator))
           .vthreshold(0)
           .vscale(1.0 / timesig.denominator));

  for(pmf_t::iterator it = dur.begin(); it != dur.end(); ++it) {
    it->second *= gauss(1.0 / it->first - 2 * modf, 4.0);
  }
  dur.update();
  double duration(0);
  if(dur.icdfempty()) {
    pmf_t p1(pbeat.vadd(-beat));
    std::cout << p1;
    pmf_t p2(pbeat.vadd(-beat + timesig.numerator));
    std::cout << p2;
    pmf_t p3(p1 + p2);
    std::cout << p3;
    pmf_t p4(p3.vthreshold(0));
    std::cout << p4;
    pmf_t p5(p4.vscale(1.0 / timesig.denominator));
    std::cout << p5;
    try {
      duration = pduration.rand();
    }
    catch(const std::exception& e) {
      DEBUG(e.what());
      duration = 0.25;
    }
    pitch = PITCH_REST;
    DEBUG(duration);
    DEBUG(beat);
    DEBUG(frac(beat));
  } else {
    duration = dur.rand();
    // DEBUG(duration);
    // DEBUG(beat);
    // DEBUG(frac(beat));
  }
  if(pitch != PITCH_REST)
    last_pitch = pitch;
  return note_t(pitch, closest_length(duration));
}

void melody_model_t::read_xml(xmlpp::Element* e)
{
  name = e->get_attribute_value("name");
  onbeatscale = get_attribute_double(e, "onbeatscale", 0.0);
  offbeatscale = get_attribute_double(e, "offbeatscale", 0.0);
  // ambitus:
  pambitus.clear();
  int32_t pitch_min(get_attribute_double(e, "lowest", -12));
  int32_t pitch_max(get_attribute_double(e, "highest", 12));
  int32_t pitch_central(get_attribute_double(e, "central", 0));
  last_pitch = pitch_central;
  int32_t pitch_dev(get_attribute_double(e, "dev", 8));
  for(int32_t p = pitch_min; p <= pitch_max; p++)
    pambitus.set(p, gauss(p - pitch_central, pitch_dev));
  pambitus.update();
  // steps:
  pstep.clear();
  xmlpp::Node::NodeList nStep(e->get_children("step"));
  for(xmlpp::Node::NodeList::iterator nStepIt = nStep.begin();
      nStepIt != nStep.end(); ++nStepIt) {
    xmlpp::Element* eStep(dynamic_cast<xmlpp::Element*>(*nStepIt));
    if(eStep)
      pstep.set(get_attribute_double(eStep, "v"),
                get_attribute_double(eStep, "p"));
  }
  pstep.update();
  // note lengths:
  pduration.clear();
  xmlpp::Node::NodeList nDuration(e->get_children("notelength"));
  for(xmlpp::Node::NodeList::iterator nDurationIt = nDuration.begin();
      nDurationIt != nDuration.end(); ++nDurationIt) {
    xmlpp::Element* eDuration(dynamic_cast<xmlpp::Element*>(*nDurationIt));
    if(eDuration)
      pduration.set(get_attribute_double(eDuration, "v"),
                    get_attribute_double(eDuration, "p"));
  }
  pduration.update();
  // beats:
  pbeat.clear();
  xmlpp::Node::NodeList nBeat(e->get_children("beat"));
  for(xmlpp::Node::NodeList::iterator nBeatIt = nBeat.begin();
      nBeatIt != nBeat.end(); ++nBeatIt) {
    xmlpp::Element* eBeat(dynamic_cast<xmlpp::Element*>(*nBeatIt));
    if(eBeat)
      pbeat.set(get_attribute_double(eBeat, "v"),
                get_attribute_double(eBeat, "p"));
  }
  pbeat.update();
  if(pduration.icdfempty())
    throw TASCAR::ErrMsg("No valid durations in table.");
  if(pbeat.icdfempty())
    throw TASCAR::ErrMsg("No valid beats in table.");
  if(pambitus.icdfempty())
    throw TASCAR::ErrMsg("No valid ambitus.");
  if(pstep.icdfempty())
    throw TASCAR::ErrMsg("No valid step size table.");
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

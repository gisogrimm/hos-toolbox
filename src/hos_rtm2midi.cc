/*
  Copyright (C) 2004 Ian Esten
    
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <jack/jack.h>
#include <jack/midiport.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "errorhandling.h"
#include "libhos_music.h"
#include "osc_helper.h"
#include <iostream>
#include "defs.h"

class midi_note_t {
public:
  midi_note_t();
  uint8_t velocity;
  uint8_t pitch;
  uint32_t startcnt;
  uint32_t lengthcnt;
  bool process;
};

midi_note_t::midi_note_t()
  : velocity(0),
    pitch(0),
    startcnt(0),
    lengthcnt(0),
    process(false)
{
}

class midi_t : public TASCAR::osc_server_t
{
public:
  midi_t(const std::string& name);
  ~midi_t();
  static int process(jack_nframes_t nframes, void *arg);
  void process(jack_nframes_t nframes);
  void add_note(const note_t& note);
  static int add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void add_note(unsigned int voice,int pitch,unsigned int length,double time);
  void set_time(double t);
private:
  std::vector<midi_note_t> notes;
  jack_client_t *client;
  jack_port_t *output_port;
  double time;
  double samples_per_brevis;
  jack_nframes_t timecnt;
};

int midi_t::add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 4) )
    ((midi_t*)user_data)->add_note(argv[0]->i,argv[1]->i,argv[2]->i,argv[3]->f);
  return 0;
}

int midi_t::set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0] == 'f') )
    ((midi_t*)user_data)->set_time(argv[0]->f);
  return 0;
}

void midi_t::set_time(double t)
{
  if( (t-time > 0.5) && (timecnt > 16000) ){
    samples_per_brevis = 0.8*(double)timecnt/(t-time);
    //DEBUG(samples_per_brevis);
    timecnt = 0;
    time = t;
  }
}

void midi_t::add_note(unsigned int voice,int pitch,unsigned int length,double time)
{
  note_t n;
  n.pitch = pitch;
  n.length = length;
  n.time = time;
  add_note(n);
}


void midi_t::add_note(const note_t& n)
{
  if( n.pitch == PITCH_REST )
    return;
  for(std::vector<midi_note_t>::iterator note=notes.begin();note!=notes.end();++note)
    if( !(note->process) ){
      note->startcnt = 192000;
      note->lengthcnt = n.duration()*samples_per_brevis;
      note->pitch = n.pitch+64;
      note->velocity = 32;
      note->process = true;
      std::cout << n << std::endl;
      return;
    }
}

int midi_t::process(jack_nframes_t nframes, void *arg)
{
  ((midi_t*)arg)->process(nframes);
  return 0;
}

void midi_t::process(jack_nframes_t nframes)
{
  void* port_buf = jack_port_get_buffer(output_port, nframes);
  unsigned char* buffer;
  jack_midi_clear_buffer(port_buf);
  for(jack_nframes_t t=0; t<nframes; t++){
    timecnt++;
    for(std::vector<midi_note_t>::iterator note=notes.begin();note!=notes.end();++note)
      if( note->process ){
        if( note->startcnt ){
          note->startcnt--;
          if( !(note->startcnt) ){
            buffer = jack_midi_event_reserve(port_buf, t, 3);
            buffer[2] = note->velocity;		/* velocity */
            buffer[1] = note->pitch;
            buffer[0] = 0x90;	/* note on */
          }
        }else{
          if( note->lengthcnt ){
            note->lengthcnt--;
            if( !(note->lengthcnt) ){
	      buffer = jack_midi_event_reserve(port_buf, t, 3);
	      buffer[2] = note->velocity;		/* velocity */
	      buffer[1] = note->pitch;
	      buffer[0] = 0x80;	/* note off */
            }
          }else{
            note->process = false;
          }
        }
      }
  }
}

midi_t::midi_t(const std::string& name)
  : TASCAR::osc_server_t("239.255.1.7","9877"),
    time(0),
    samples_per_brevis(48000),
    timecnt(0)
{
  notes.resize(512);
  if((client = jack_client_open (name.c_str(), JackNullOption, NULL)) == 0)
    throw TASCAR::ErrMsg("Unable to open client. jack server not running?");
  jack_set_process_callback (client, process, this);
  output_port = jack_port_register (client, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (jack_activate(client))
    throw TASCAR::ErrMsg("Cannot activate client.");
 
  add_method("/time","f",midi_t::set_time,this);
  add_method("/note","iiif",midi_t::add_note,this);
  osc_server_t::activate();
}

midi_t::~midi_t()
{
  osc_server_t::deactivate();
  jack_deactivate(client);
  jack_client_close(client);
}

int main(int narg, char **args)
{
  midi_t midi("hos_rtm2midi");
  while(true)
    sleep(1);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

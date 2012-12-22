/*

"mm_{hdsp,file,gui,midicc}" is a set of programs to control the matrix
mixer of an RME hdsp compatible sound card using XML formatted files
or a MIDI controller, and to visualize the mixing matrix.

Copyright (C) 2011 Giso Grimm

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/**
\file libhos_midi_ctl.h
   \ingroup apphos
\brief MIDI interface for matrix mixer
*/
#ifndef HOS_MIDI_CTL_H
#define HOS_MIDI_CTL_H

#include <alsa/asoundlib.h>
#include <alsa/seq_event.h>
#include <string>

/**
   \brief Simple MIDI controller base class
 */
class midi_ctl_t {
public:
  /**
   \param cname Client name
  */
  midi_ctl_t(const std::string& cname);
  ~midi_ctl_t();
  /**
     \param client Source client
     \param port Source port
  */
  void connect_input(int client,int port);
  /**
     \param client Destination client
     \param port Destination port
  */
  void connect_output(int client,int port);
  void start_service();
  void stop_service();
  /**
     \brief Send a CC event to output
     \param channel MIDI channel number
     \param param MIDI parameter
     \param value MIDI value
   */
  void send_midi(int channel, int param, int value);
private:
  static void * service(void *);
protected:
  void service();
  /**
     \brief Callback to be called for incoming MIDI events
   */
  virtual void emit_event(int channel, int param, int value) = 0;
private:
  // MIDI sequencer:
  snd_seq_t* seq;
  // input port:
  snd_seq_addr_t port_in;
  // output/feedback port:
  snd_seq_addr_t port_out;
  // flag to control service:
  bool b_run_service;
  pthread_t srv_thread;
};


#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
